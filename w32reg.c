/*
 * w32ole.cpp:  Winvile OLE registration code (currently only used
 *              for OLE automation).
 *
 * $Header: /users/source/archives/vile.vcs/RCS/w32reg.c,v 1.3 1998/09/03 10:15:45 cmorgan Exp $
 */

#include <windows.h>
#include <objbase.h>
#include <direct.h>
#include <ctype.h>

#include <initguid.h>
#include "w32reg.h"


#include "estruct.h"
#include "edef.h"


static void make_editor_name(char *name),
            make_editor_path(char *path),
            registration_success(char *editor_name, char *which, char *path);

static int  report_last_error(void);

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


/* Pound a bunch of keys into the registry. */
int
oleauto_register(OLEAUTO_OPTIONS *opts)
{
    HKEY  hk, hksub;
    char  key[512], name[64], editor_path[NFILEN];
    long  rc;
    DWORD val_len;
    BYTE  value[NFILEN * 2];

    make_editor_name(name);
    make_editor_path(editor_path);

    /* -------------------------- Step 1 --------------------------- */

    sprintf(key, "%s.Application", name);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hk,
                       NULL) != ERROR_SUCCESS)
    {
        return (report_last_error());
    }
    val_len = sprintf((char *) value, "%s %s Application", name, VTL_VERSTR);
    if (RegSetValueEx(hk, NULL, 0, REG_SZ, value, val_len) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    strcat(key, "\\CLSID");
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub,
                       NULL,
                       0,
                       REG_SZ,
                       (BYTE *) CLSID_VILEAUTO_KEY,
                       sizeof(CLSID_VILEAUTO_KEY) - 1);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "%s.Application\\CURVER", name);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    val_len = sprintf((char *) value, "%s.Application.%s", name, VTL_VERSTR);
    rc = RegSetValueEx(hksub, NULL, 0, REG_SZ, value, val_len);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
        return (report_last_error());

    /* -------------------------- Step 2 --------------------------- */

    sprintf(key, "%s.Application.%s", name, VTL_VERSTR);
    val_len = sprintf((char *) value, "%s %s Application", name, VTL_VERSTR);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hk,
                       NULL) != ERROR_SUCCESS)
    {
        return (report_last_error());
    }
    if (RegSetValueEx(hk, NULL, 0, REG_SZ, value, val_len) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    strcat(key, "\\CLSID");
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub,
                       NULL,
                       0,
                       REG_SZ,
                       (BYTE *) CLSID_VILEAUTO_KEY,
                       sizeof(CLSID_VILEAUTO_KEY) - 1);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
        return (report_last_error());
    sprintf(key, "CLSID\\%s", CLSID_VILEAUTO_KEY);
    val_len = sprintf((char *) value, "%s %s Application", name, VTL_VERSTR);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hk,
                       NULL) != ERROR_SUCCESS)
    {
        return (report_last_error());
    }
    if (RegSetValueEx(hk, NULL, 0, REG_SZ, value, val_len) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    strcat(key, "\\ProgID");
    val_len = sprintf((char *) value, "%s.Application.%s", name, VTL_VERSTR);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub, NULL, 0, REG_SZ, value, val_len);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "CLSID\\%s\\VersionIndependentProgID", CLSID_VILEAUTO_KEY);
    val_len = sprintf((char *) value, "%s.Application", name);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub, NULL, 0, REG_SZ, value, val_len);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "CLSID\\%s\\LocalServer32", CLSID_VILEAUTO_KEY);
    sprintf((char *) value, "%s -Oa", editor_path);
    if (opts->invisible)
        strcat((char *) value, " -invisible");
    if (opts->multiple)
        strcat((char *) value, " -multiple");
    val_len = strlen((char *) value);
    if (opts->rows)
    {
        val_len += sprintf(((char *) value) + val_len,
                           " -geometry %ux%u",
                           opts->cols,
                           opts->rows);
    }
    if (opts->fontstr)
    {
        val_len += sprintf(((char *) value) + val_len,
                           " -fn '%s'",
                           opts->fontstr);
    }
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub, NULL, 0, REG_SZ, value, val_len);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "CLSID\\%s\\TypeLib", CLSID_VILEAUTO_KEY);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub,
                       NULL,
                       0,
                       REG_SZ,
                       (BYTE *) LIBID_VILEAUTO_KEY,
                       sizeof(LIBID_VILEAUTO_KEY) - 1);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "CLSID\\%s\\Programmable", CLSID_VILEAUTO_KEY);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    RegCloseKey(hksub);
    RegCloseKey(hk);

    /* -------------------------- Step 3 --------------------------- */

    sprintf(key, "TypeLib\\%s\\%s", LIBID_VILEAUTO_KEY, VTL_VERSTR);
    val_len = sprintf((char *) value, "%s %s Type Library", name, VTL_VERSTR);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hk,
                       NULL) != ERROR_SUCCESS)
    {
        return (report_last_error());
    }
    if (RegSetValueEx(hk,
                      NULL,
                      0,
                      REG_SZ,
                      value,
                      val_len) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }

    /* -------------------------- Step 4 --------------------------- */

    strcat(key, "\\9\\win32");
    val_len = strlen(strcpy((char *) value, editor_path));
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub, NULL, 0, REG_SZ, value, val_len);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
        return (report_last_error());

    /* -------------------------- Step 5 --------------------------- */

    sprintf(key, "Interface\\%s", IID_IVILEAUTO_KEY);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hk,
                       NULL) != ERROR_SUCCESS)
    {
        return (report_last_error());
    }
    if (RegSetValueEx(hk,
                      NULL,
                      0,
                      REG_SZ,
                      "IVileAuto",
                      sizeof("IVileAuto") - 1) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    strcat(key, "\\TypeLib");
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub,
                       NULL,
                       0,
                       REG_SZ,
                       (BYTE *) LIBID_VILEAUTO_KEY,
                       sizeof(LIBID_VILEAUTO_KEY) - 1);
    RegCloseKey(hksub);
    if (rc != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    sprintf(key, "Interface\\%s\\ProxyStubClsid32", IID_IVILEAUTO_KEY);
    if (RegCreateKeyEx(HKEY_CLASSES_ROOT,
                       key,
                       0,
                       NULL,
                       0,
                       KEY_ALL_ACCESS,
                       NULL,
                       &hksub,
                       NULL) != ERROR_SUCCESS)
    {
        RegCloseKey(hk);
        return (report_last_error());
    }
    rc = RegSetValueEx(hksub,
                       NULL,
                       0,
                       REG_SZ,
                       (BYTE *) CLSID_PROXY_STUB,
                       sizeof(CLSID_PROXY_STUB) - 1);
    RegCloseKey(hksub);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS)
        return (report_last_error());

    /* ------  Get to here, then all is well.  Report status. ------ */

    registration_success(name, "registered", editor_path);
    return (0);
}



/*
 * Undo oleauto_register().  Absence of any of the keys/values is not an
 * error.  This routine can't fail (no news is good news).
 */
int
oleauto_unregister(void)
{
    char keytop[512], keysub[512], name[64];

    make_editor_name(name);

    /*
     * To be compatible with both WinNT and Win95, must delete subkeys
     * before deleting top-level key.
     */

    /* -------------------------- Step 1 --------------------------- */

    sprintf(keytop, "%s.Application", name);
    sprintf(keysub, "%s\\CLSID", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\CURVER", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);

    /* -------------------------- Step 2 --------------------------- */

    sprintf(keytop, "%s.Application.%s", name, VTL_VERSTR);
    sprintf(keysub, "%s\\CLSID", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);
    sprintf(keytop, "CLSID\\%s", CLSID_VILEAUTO_KEY);
    sprintf(keysub, "%s\\ProgID", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\VersionIndependentProgID", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\LocalServer32", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\TypeLib", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\Programmable", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);

    /* ----------------------- Step 3 & 4 -------------------------- */

    sprintf(keytop, "TypeLib\\%s\\%s", LIBID_VILEAUTO_KEY, VTL_VERSTR);
    sprintf(keysub, "%s\\9\\win32", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\9", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);
    sprintf(keytop, "TypeLib\\%s", LIBID_VILEAUTO_KEY);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);

    /* -------------------------- Step 5 --------------------------- */

    sprintf(keytop, "Interface\\%s", IID_IVILEAUTO_KEY);
    sprintf(keysub, "%s\\TypeLib", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    sprintf(keysub, "%s\\ProxyStubClsid32", keytop);
    RegDeleteKey(HKEY_CLASSES_ROOT, keysub);
    RegDeleteKey(HKEY_CLASSES_ROOT, keytop);

    registration_success(name, "unregistered", NULL);
    return (0);
}



static void
make_editor_name(char *name /* must be long enough to hold result */)
{
    strcpy(name, prognam);
    *name = toupper(*name);
}



static void
make_editor_path(char *path /* must be at least NFILEN chars long */)
{
    char temp[NFILEN], name[128], *s;

    /*
     * Can't use "exec_pathname" in this routine because that global var
     * is always initialized to "." on a win32 host (bummer).
     */
    sprintf(name, "%s.exe", prognam);
    if ((s = flook(name, FL_PATH|FL_EXECABLE)) != 0)
        strcpy(path, sl_to_bsl(s));
    else
    {
        /* lookup failed, assume cwd. */

        if (_getcwd(temp, sizeof(temp)) == NULL)
        {
            /* getcwd failed, user must live with '.' . */

            temp[0] = '.';
            temp[1] = '\0';
        }
        sprintf(path, "%s\\%s", sl_to_bsl(temp), name);
    }
}



static int
report_last_error(void)
{
    disp_win32_error(W32_SYS_ERROR, NULL);
    return (1);  /* This becomes the exit status returned to shell. */
}



static void
registration_success(char *editor_name, char *which, char *path)
{
    char status[256];
    int  len;

    len = sprintf(status,
                  "%s OLE Automation successfully %s.",
                  editor_name,
                  which);
    if (path)
    {
        /* Registered -- warn user about registering temp copy of editor. */

        sprintf(status + len, "\r\rNote:  registered path is %s .\r\r"
        "If this path is temporary or otherwise undesirable, install the "
        "editor in its proper destination directory and re-register.",
                path);
    }
    MessageBox(NULL, status, editor_name, MB_ICONINFORMATION|MB_OK);
}
