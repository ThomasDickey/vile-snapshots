/* contortions to avoid typedef conflicts */
#define main perl_main
#define regexp perl_regexp

/* for perl */
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#undef main
#undef regexp
#undef HUGE

/* for vile */
#include "estruct.h"
#include "edef.h"

#include "api.h"

extern REGION *haveregion;

static PerlInterpreter *perl_interp;
static SV *svStartLine;			/* starting line of region,
					   $VI::StartLine in perl*/
static SV *svStopLine;			/* ending line of region,
                                           $VI::StopLine in perl */
static SV *svScreenId;			/* Screen Id (buffer name in VILE),
                                           $VI::ScreenId in perl */
static SV *svcurscr;			/* Same as variable referenced by
					   svScreenId, but just named
					   $curscr.  The nvi code indicates
					   that this is for backwards
					   compatibility.  All I know is
					   that some of the sample scripts
					   wouldn't run without it.*/

static int perl_init(void);
static void xs_init(void);
static void perl_eval(char *string);

/* When no region is specified, this will cause the entire buffer to
   be selected without moving DOT. */
void perl_default_region(void)
{
    static REGION region;
    MARK save_DOT = DOT;
    DOT.l = lforw(buf_head(curbp));
    DOT.o = 0;
    MK.l  = lback(buf_head(curbp));
    MK.o  = 0;
    regionshape = FULLLINE;
    if (getregion(&region))
	haveregion = &region;
    DOT = save_DOT;
}

static SV *
newVIrv(SV *rv, SCR *sp)
{
    if (sp->perl_handle == 0) {
	sp->perl_handle = newSV(0);
	sv_setiv(sp->perl_handle, (IV) sp);
	SvREFCNT_inc(sp->perl_handle);
    }
    else
	SvREFCNT_inc(sp->perl_handle);

    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = sp->perl_handle;
    SvROK_on(rv);
    return sv_bless(rv, gv_stashpv("VI", TRUE));
}

void
perl_free_handle(void *handle)
{
    /*
     * Zero out perl's handle to the SCR structure
     */
    sv_setiv(handle, 0);

    /*
     * Decrement the reference count to indicate the fact that
     * we are no longer referencing it from the api private structure.
     * If there aren't any other references from within perl either,
     * then this scalar will be collected.
     */
    SvREFCNT_dec(handle);
}

int
perl(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int status;
    char buf[NLINE];	/* buffer to receive command into */

    buf[0] = EOS;
    if ((status = mlreply_no_opts("Perl command: ", buf, sizeof(buf))) != TRUE)
	    return(status);

    if (perl_interp || perl_init()) {
	REGION region;
	char *err;
	STRLEN length;

	if (getregion(&region) == TRUE) {
	    sv_setiv(svStartLine, line_no(curbp, region.r_orig.l));
	    sv_setiv(svStopLine, line_no(curbp, region.r_end.l)-1);
	}
	else {
	    /* Should never get here; but just in case... */
	    sv_setiv(svStartLine, 1);
	    sv_setiv(svStopLine,  line_count(curbp));
	}

	newVIrv(svScreenId, api_bp2sp(curbp));
	newVIrv(svcurscr,   api_bp2sp(curbp));

	perl_eval(buf);

	SvREFCNT_dec(SvRV(svScreenId));
	SvROK_off(svScreenId);
	SvREFCNT_dec(SvRV(svcurscr));
	SvROK_off(svcurscr);
	api_command_cleanup();

	err = SvPV(GvSV(errgv), length);
	if (length == 0)
	    return TRUE;
	else {
	    err[length - 1] = '\0';
	    mlforce("%s", err);
	    return FALSE;
	}
    }
    else
	return FALSE;
}

static int perl_init(void)
{
    char *embedding[] = { "", "-e", "0" };
    char *bootargs[]  = { "VI", NULL };

    perl_interp = perl_alloc();
    perl_construct(perl_interp);

    if (perl_parse(perl_interp, xs_init, 3, embedding, NULL)) {
	perl_destruct(perl_interp);
	perl_free(perl_interp);
	perl_interp = NULL;
	return FALSE;
    }
    perl_call_argv("VI::bootstrap", G_DISCARD, bootargs);
    perl_eval("$SIG{__WARN__}='VI::Warn'");

    
    /* Obtain handles to specific perl variables, creating them
       if they do not exist. */
    svStartLine = perl_get_sv("VI::StartLine", TRUE);
    svStopLine  = perl_get_sv("VI::StopLine",  TRUE);
    svScreenId  = perl_get_sv("VI::ScreenId",  TRUE);
    svcurscr    = perl_get_sv("curscr",        TRUE);

    /* Tie STDOUT and STDERR to curscr->Msg() function */
    sv_magic((SV *) gv_fetchpv("STDOUT", TRUE, SVt_PVIO), svcurscr, 'q',
	     Nullch, 0);

    sv_magic((SV *) gv_fetchpv("STDERR", TRUE, SVt_PVIO), svcurscr, 'q',
	     Nullch, 0);

    /* Make the above readonly (from perl) */
    SvREADONLY_on(svStartLine);
    SvREADONLY_on(svStopLine);
    SvREADONLY_on(svScreenId);
    SvREADONLY_on(svcurscr);

    return TRUE;
}

static void
perl_eval(char *string)
{
    SV* sv = newSVpv(string, 0);

    perl_eval_sv(sv, G_DISCARD | G_NOARGS);
    SvREFCNT_dec(sv);
}

/* Register any extra external extensions */

extern void boot_DynaLoader _((CV* cv));
extern void boot_VI _((CV* cv));

static void xs_init()
{
	char *file = __FILE__;
	dXSUB_SYS;
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
	newXS("VI::bootstrap", boot_VI, file);
}

typedef void *	VI;

/* FIXME: Make these macrs do the right wrt the perl message handler */
#define INITMESSAGE
#define ENDMESSAGE

MODULE = VI	PACKAGE = VI

PROTOTYPES: DISABLE

# msg --
#	Set the message line to text.
#
# Perl Command: VI::Msg
# Usage: VI::Msg screenId text

void
Msg(screen, ...)
	VI	screen
 
	ALIAS:
	PRINT = 1

	PREINIT:
	int i;
	char *text = 0;
	STRLEN sz = 0;

	CODE:
	for (i = 1; i < items; i++)
	{
	    STRLEN len;
	    char *arg = SvPV(ST(i), len);

	    if ((text = realloc(text, sz + len + 1)))
	    {
		memcpy(text + sz, arg, len);
		text[sz += len] = 0;
	    }
	    else
		break;
	}

	if (text)
	{
	    mlforce("%s", text);
	    free(text);
	}

# XS_VI_aline --
#	-- Append the string text after the line in lineNumber.
#
# Perl Command: VI::AppendLine
# Usage: VI::AppendLine screenId lineNumber text

void
AppendLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_aline(screen, linenumber, text, length);
	ENDMESSAGE;


# XS_VI_dline --
#	Delete lineNum.
#
# Perl Command: VI::DelLine
# Usage: VI::DelLine screenId lineNum

void 
DelLine(screen, linenumber)
	VI screen
	int linenumber

	ALIAS:
	DeleteLine = 1

	PREINIT:
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_dline(screen, linenumber);
	ENDMESSAGE;

char *
GetLine(screen, linenumber)
	VI screen
	int linenumber

	PREINIT:
	size_t len;
	int rval;
	char *p;

	PPCODE:
	INITMESSAGE;
	rval = api_gline(screen, linenumber, &p, &len);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSVpv(p, len)));

AV *
GetLines(screen, linenumber, count)
	VI screen
	int linenumber
	int count

	PREINIT:
	int rval;
	size_t len;
	char *p;
	LINE *lp;
	int i, maxcount;

	CODE:

	/* Use one call of api_gline to set DOT for us.  This is kinda slimy
	   because we're mixing levels here. Maybe we should try to find a
	   clean way to separate them.  Actually, it'd probably be cleaner
	   just to export something from api.c which sets the DOT for us. */

	api_gline(screen, linenumber, &p, &len);

	maxcount = line_count(sp2bp(screen)) - linenumber + 1;
	if (count > maxcount)
	    count = maxcount;

	RETVAL = newAV();

	if (count > 0)
	    av_unshift(RETVAL, count);

	for (i = 0, lp = DOT.l; count-- > 0; lp = lforw(lp), i++) {
	    p = lp->l_text;
	    len = llength(lp);
	    if (len == 0)
		p = "";
	    av_store(RETVAL, i, newSVpv(p,len));
	}
	/* We need to decrement the reference count of the return AV,
	   but we need to do it in a delayed fashion.  This is because
	   creating a reference below will increment it.  sv_2mortal
	   is used to do this and the documentation assures me that
	   it can be used on AV's as well as SV's.
	 */
	sv_2mortal((SV *) RETVAL);

	OUTPUT:
	RETVAL


# XS_VI_sline --
#	Set lineNumber to the text supplied.
#
# Perl Command: VI::SetLine
# Usage: VI::SetLine screenId lineNumber text

void
SetLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_sline(screen, linenumber, text, length);
	ENDMESSAGE;

# XS_VI_iline --
#	Insert the string text before the line in lineNumber.
#
# Perl Command: VI::InsertLine
# Usage: VI::InsertLine screenId lineNumber text

void
InsertLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_iline(screen, linenumber, text, length);
	ENDMESSAGE;

# XS_VI_lline --
#	Return the last line in the screen.
#
# Perl Command: VI::LastLine
# Usage: VI::LastLine screenId

int 
LastLine(screen)
	VI screen

	PREINIT:
	int last;
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_lline(screen, &last);
	ENDMESSAGE;
	RETVAL=last;

	OUTPUT:
	RETVAL

# XS_VI_iscreen --
#	Create a new screen.  If a filename is specified then the screen
#	is opened with that file.
#
# Perl Command: VI::NewScreen
# Usage: VI::NewScreen screenId [file]

VI
Edit(screen, ...)
	VI screen

	ALIAS:
	NewScreen = 1

	PROTOTYPE: $;$
	PREINIT:
	int rval;
	char *file;
	SCR *nsp;

	CODE:
	file = (items == 1) ? NULL : (char *)SvPV(ST(1),na);
	INITMESSAGE;
	rval = api_edit(screen, file, &nsp, ix);
	ENDMESSAGE;
	
	RETVAL = nsp;

	OUTPUT:
	RETVAL


# XS_VI_fscreen --
#	Return the screen id associated with file name.
#
# Perl Command: VI::FindScreen
# Usage: VI::FindScreen file

VI
FindScreen(file)
	char *file

	CODE:
	RETVAL = api_fscreen(0, file);

	OUTPUT:
	RETVAL

# XS_VI_swscreen --
#	Change the current focus to screen.
#
# Perl Command: VI::SwitchScreen
# Usage: VI::SwitchScreen screenId screenId

void
SwitchScreen(screenFrom, screenTo)
	VI screenFrom
	VI screenTo

	PREINIT:
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_swscreen(screenFrom, screenTo);
	ENDMESSAGE;


void
Warn(warning)
	char *warning;

	PREINIT:
	int i;
	CODE:
	sv_catpv(GvSV(errgv),warning);

