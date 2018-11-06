/*
 * w32reg.c:  Winvile OLE registration code (currently only used for OLE
 *            automation).
 *
 * $Id: w32reg.c,v 1.17 2018/11/05 23:21:42 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#include <objbase.h>
#include <direct.h>
#include <ctype.h>
#include <psapi.h>

#include <initguid.h>
#include "w32reg.h"

static void make_editor_name(char *name);
static void make_editor_path(char *path);
static void registration_success(char *editor_name, char *which, char *path);

static int report_w32_error(long w32_err_code);

/* ---------------------------------------------------------------------- */

/*
 * Description of ole automation registry info...borrowed heavily from
 * MS SDK sample code.
 *
 * Register type info using the following 5 simple :-) steps....
 *
 * 1) Version independent registration. Points to Version <verstr>

 HKEY_CLASSES_ROOT\<vilename>.Application = <vilename> <verstr> Application
 HKEY_CLASSES_ROOT\<vilename>.Application\CLSID = <GUID_clsid>
 HKEY_CLASSES_ROOT\<vilename>.Application\CURVER =
                                             <vilename>.Application.<verstr>

 * 2) Explicit version registration

 HKEY_CLASSES_ROOT\<vilename>.Application.<verstr> =
                                              <vilename> <verstr> Application
 HKEY_CLASSES_ROOT\<vilename>.Application.<verstr>\CLSID = <GUID_clsid>
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid> = <vilename> <verstr> Application
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid>\ProgID = <vilename>.Application.<verstr>
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid>\VersionIndependentProgID =
                                                       <vilename>.Application
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid>\LocalServer32 = <path_to_vile> -Oa [opts]
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid>\TypeLib = <GUID_libid>
 HKEY_CLASSES_ROOT\CLSID\<GUID_clsid>\Programmable

 * 3) Type library registration information

 HKEY_CLASSES_ROOT\TypeLib\<GUID_libid>\<verstr> =
                                                <vilename> <verstr> Type Library
 HKEY_CLASSES_ROOT\TypeLib\<GUID_libid>\<verstr>\HELPDIR = N/A (unsupported)

 * 4) English

 HKEY_CLASSES_ROOT\TypeLib\<GUID_libid>\<verstr>\9\win32 = <path_to_vile,
                                  contains embedded English help strings>

 * 5) Interface registration. All interfaces that support vtable binding must
 *    be registered.

 HKEY_CLASSES_ROOT\Interface\<GUID_iid> = IVileAuto
 HKEY_CLASSES_ROOT\Interface\<GUID_iid>\TypeLib = <GUID_libid>
 HKEY_CLASSES_ROOT\Interface\<GUID_iid>\ProxyStubClsid32 = <GUID_proxy>

 */

#ifdef UNICODE
static long
delete_key(HKEY key, const char *value)
{
    long rc = ERROR_NOT_ENOUGH_MEMORY;
    W32_CHAR *actual = w32_charstring(value);
    if (actual != 0) {
	rc = RegDeleteKey(key, actual);
	free(actual);
    }
    TRACE(("delete_key %s ->%ld\n", value, rc));
    return rc;
}

static long
create_key(HKEY key, const char *value, HKEY * result)
{
    long rc = ERROR_NOT_ENOUGH_MEMORY;
    W32_CHAR *actual = w32_charstring(value);
    if (actual != 0) {
	rc = RegCreateKeyEx(key,
			    actual,
			    0,
			    NULL,
			    0,
			    KEY_ALL_ACCESS,
			    NULL,
			    result,
			    NULL);
	free(actual);
    }
    TRACE(("create_key %s ->%ld\n", value, rc));
    return rc;
}
#else
#define delete_key(key,value) RegDeleteKey(key,value)
#define create_key(key,value,result) RegCreateKeyEx(key,value,0,NULL,0,KEY_ALL_ACCESS,NULL,result,NULL)
#endif

/* Pound a bunch of keys into the registry. */
int
oleauto_register(OLEAUTO_OPTIONS * opts)
{
    HANDLE processHandle = NULL;
    HKEY hk, hksub;
    char key[512], name[64], editor_path[NFILEN];
    long rc;
    char value[NFILEN * 2];
    TCHAR fullpath[NFILEN];

    TRACE(("** oleauto_register\n"));

    make_editor_name(name);

    *editor_path = EOS;
    processHandle = OpenProcess((PROCESS_QUERY_INFORMATION
				 | PROCESS_VM_READ),
				FALSE,
				GetCurrentProcessId());
    if (processHandle != NULL) {
	if (GetModuleFileNameEx(processHandle, NULL, fullpath, NFILEN) == 0) {
	    TRACE(("?? Failed to get module filename.\n"));
	} else {
	    strcpy(editor_path, asc_charstring(fullpath));
	    TRACE(("Module filename is %s\n", editor_path));
	}
	CloseHandle(processHandle);
    } else {
	TRACE(("?? Failed to open process.\n"));
    }

    if (!*editor_path) {
	make_editor_path(editor_path);
	TRACE(("Fallback filename is %s\n", editor_path));
    }

    /* -------------------------- Step 1 --------------------------- */

    sprintf(key, "%s.Application", name);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hk)) != ERROR_SUCCESS) {
	return (report_w32_error(rc));
    }
    sprintf(value, "%s %s Application", name, VTL_VERSTR);
    if ((rc = w32_set_reg_sz(hk, NULL, value)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    strcat(key, "\\CLSID");
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub,
			NULL,
			CLSID_VILEAUTO_KEY);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "%s.Application\\CURVER", name);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(value, "%s.Application.%s", name, VTL_VERSTR);
    rc = w32_set_reg_sz(hksub, NULL, value);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
	return (report_w32_error(rc));

    /* -------------------------- Step 2 --------------------------- */

    sprintf(key, "%s.Application.%s", name, VTL_VERSTR);
    sprintf(value, "%s %s Application", name, VTL_VERSTR);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hk)) != ERROR_SUCCESS) {
	return (report_w32_error(rc));
    }
    if ((rc = w32_set_reg_sz(hk,
			     NULL,
			     value)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    strcat(key, "\\CLSID");
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub,
			NULL,
			CLSID_VILEAUTO_KEY);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
	return (report_w32_error(rc));
    sprintf(key, "CLSID\\%s", CLSID_VILEAUTO_KEY);
    sprintf(value, "%s %s Application", name, VTL_VERSTR);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hk)) != ERROR_SUCCESS) {
	return (report_w32_error(rc));
    }
    if ((rc = w32_set_reg_sz(hk,
			     NULL,
			     value)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    strcat(key, "\\ProgID");
    sprintf(value, "%s.Application.%s", name, VTL_VERSTR);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub, NULL, value);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "CLSID\\%s\\VersionIndependentProgID", CLSID_VILEAUTO_KEY);
    sprintf(value, "%s.Application", name);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub, NULL, value);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "CLSID\\%s\\LocalServer32", CLSID_VILEAUTO_KEY);
    sprintf(value, "%s -Oa", editor_path);
    if (opts->invisible)
	strcat(value, " -invisible");
    if (opts->multiple)
	strcat(value, " -multiple");
    if (opts->rows) {
	sprintf(value + strlen(value),
		" -geometry %ux%u",
		opts->cols,
		opts->rows);
    }
    if (opts->fontstr) {
	sprintf(value + strlen(value),
		" -fn '%s'",
		opts->fontstr);
    }
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub, NULL, value);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "CLSID\\%s\\TypeLib", CLSID_VILEAUTO_KEY);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub,
			NULL,
			LIBID_VILEAUTO_KEY);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "CLSID\\%s\\Programmable", CLSID_VILEAUTO_KEY);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    RegCloseKey(hksub);
    RegCloseKey(hk);

    /* -------------------------- Step 3 --------------------------- */

    sprintf(key, "TypeLib\\%s\\%s", LIBID_VILEAUTO_KEY, VTL_VERSTR);
    sprintf(value, "%s %s Type Library", name, VTL_VERSTR);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hk)) != ERROR_SUCCESS) {
	return (report_w32_error(rc));
    }
    if ((rc = w32_set_reg_sz(hk,
			     NULL,
			     value)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }

    /* -------------------------- Step 4 --------------------------- */

    strcat(key, "\\9\\win32");
    strcpy(value, editor_path);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub, NULL, value);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
	return (report_w32_error(rc));

    /* -------------------------- Step 5 --------------------------- */

    sprintf(key, "Interface\\%s", IID_IVILEAUTO_KEY);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hk)) != ERROR_SUCCESS) {
	return (report_w32_error(rc));
    }
    if ((rc = w32_set_reg_sz(hk,
			     NULL,
			     "IVileAuto")) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    strcat(key, "\\TypeLib");
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub,
			NULL,
			LIBID_VILEAUTO_KEY);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    sprintf(key, "Interface\\%s\\ProxyStubClsid32", IID_IVILEAUTO_KEY);
    if ((rc = create_key(HKEY_CLASSES_ROOT, key, &hksub)) != ERROR_SUCCESS) {
	RegCloseKey(hk);
	return (report_w32_error(rc));
    }
    rc = w32_set_reg_sz(hksub,
			NULL,
			CLSID_PROXY_STUB);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
	return (report_w32_error(rc));

    /* ------  Get to here, then all is well.  Report status. ------ */

    registration_success(name, "registered", editor_path);
    return (0);
}

/*
 * Undo oleauto_register().  Absence of any of the keys/values is not an
 * error.  This routine can't fail, but we will check periodically for
 * permission (access) errors.
 */
int
oleauto_unregister(void)
{
    char keytop[512], keysub[512], name[64];
    long rc;

    TRACE(("** oleauto_unregister\n"));

    make_editor_name(name);

    /*
     * To be compatible with both WinNT and Win95, must delete subkeys
     * before deleting top-level key.
     */

    /* -------------------------- Step 1 --------------------------- */

    sprintf(keytop, "%s.Application", name);
    sprintf(keysub, "%s\\CLSID", keytop);
    if ((rc = delete_key(HKEY_CLASSES_ROOT, keysub)) != ERROR_SUCCESS) {
	if (rc == ERROR_ACCESS_DENIED)
	    return (report_w32_error(rc));
    }
    sprintf(keysub, "%s\\CURVER", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    (void) delete_key(HKEY_CLASSES_ROOT, keytop);

    /* -------------------------- Step 2 --------------------------- */

    sprintf(keytop, "%s.Application.%s", name, VTL_VERSTR);
    sprintf(keysub, "%s\\CLSID", keytop);
    if ((rc = delete_key(HKEY_CLASSES_ROOT, keysub)) != ERROR_SUCCESS) {
	if (rc == ERROR_ACCESS_DENIED)
	    return (report_w32_error(rc));
    }
    (void) delete_key(HKEY_CLASSES_ROOT, keytop);
    sprintf(keytop, "CLSID\\%s", CLSID_VILEAUTO_KEY);
    sprintf(keysub, "%s\\ProgID", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\VersionIndependentProgID", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\LocalServer32", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\TypeLib", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\Programmable", keytop);
    (void) delete_key(HKEY_CLASSES_ROOT, keysub);
    (void) delete_key(HKEY_CLASSES_ROOT, keytop);

    /* ----------------------- Step 3 & 4 -------------------------- */

    sprintf(keytop, "TypeLib\\%s\\%s", LIBID_VILEAUTO_KEY, VTL_VERSTR);
    sprintf(keysub, "%s\\9\\win32", keytop);
    if ((rc = delete_key(HKEY_CLASSES_ROOT, keysub)) != ERROR_SUCCESS) {
	if (rc == ERROR_ACCESS_DENIED)
	    return (report_w32_error(rc));
    }
    sprintf(keysub, "%s\\9", keytop);
    delete_key(HKEY_CLASSES_ROOT, keysub);
    delete_key(HKEY_CLASSES_ROOT, keytop);
    sprintf(keytop, "TypeLib\\%s", LIBID_VILEAUTO_KEY);
    delete_key(HKEY_CLASSES_ROOT, keytop);

    /* -------------------------- Step 5 --------------------------- */

    sprintf(keytop, "Interface\\%s", IID_IVILEAUTO_KEY);
    sprintf(keysub, "%s\\TypeLib", keytop);
    if ((rc = delete_key(HKEY_CLASSES_ROOT, keysub)) != ERROR_SUCCESS) {
	if (rc == ERROR_ACCESS_DENIED)
	    return (report_w32_error(rc));
    }
    sprintf(keysub, "%s\\ProxyStubClsid32", keytop);
    delete_key(HKEY_CLASSES_ROOT, keysub);
    delete_key(HKEY_CLASSES_ROOT, keytop);

    registration_success(name, "unregistered", NULL);
    return (0);
}

static void
make_editor_name(char *name /* must be long enough to hold result */ )
{
    strcpy(name, prognam);

    /*
     * Can't call toUpper() here because vile's character mapping array is
     * not initialized in all cases _before_ make_editor_name() is called.
     */
    *name = (char) toupper(*name);
}

static void
make_editor_path(char *path /* must be at least NFILEN chars long */ )
{
    char temp[NFILEN], name[128], *s;
    UINT which = FL_PATH | FL_EXECABLE;

    if (strcmp(exec_pathname, "."))
	which |= FL_EXECDIR;
    sprintf(name, "%s.exe", prognam);
    if ((s = cfg_locate(name, which)) != 0) {
	if (!(s[0] == '.' && s[1] == '\\')) {
	    strcpy(path, sl_to_bsl(s));
	    return;
	}
    }

    /* lookup failed or path prefix was ".\", fall back on cwd */
    if (_getcwd(temp, sizeof(temp)) == NULL) {
	/* getcwd failed, user must live with '.' . */

	temp[0] = '.';
	temp[1] = '\0';
    }
    sprintf(path, "%s\\%s", sl_to_bsl(temp), name);
}

static int
report_w32_error(long w32_err_code)
{
    TRACE(("report_w32_error code %#lx\n", w32_err_code));
    if (w32_err_code == ERROR_ACCESS_DENIED) {
	/* give an error message other than "Access denied" */

	MessageBox(NULL,
		   W32_STRING("Access denied while updating the registry.")
		   W32_STRING("\r\rRegistering/Unregistering an OLE automation server requires ")
		   W32_STRING("administrator privilege."),
		   w32_prognam(),
		   MB_OK | MB_ICONSTOP);

    } else {
	disp_win32_error(w32_err_code, NULL);
    }
    return (w32_err_code);
}

static void
registration_success(char *editor_name, char *which, char *path)
{
    W32_CHAR *msg;
    W32_CHAR *arg;
    char status[NFILEN + 256];
    int len;

    TRACE(("** registration_success %s %s\n", editor_name, which));
    len = sprintf(status,
		  "%s OLE Automation successfully %s.",
		  editor_name,
		  which);
    if (path) {
	/* Registered -- warn user about registering temp copy of editor. */

	sprintf(status + len,
		"\r\rNote:  registered path is %s .\r\r"
		"If this path is temporary or otherwise undesirable, install the "
		"editor in its proper destination directory and re-register.",
		path);
    }
    msg = w32_charstring(status);
    arg = w32_charstring(editor_name);
    MessageBox(NULL, msg, arg, MB_ICONINFORMATION | MB_OK);
    free(msg);
    free(arg);
}
