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
newVIrv(SV *rv, BUFFER *bp)
{
    SCR *sp;

    if (bp->b_api_private == 0) {
	sp = typecalloc(SCR);
	if (sp == 0)
	    return 0;			/* FIXME: put checks on all calls to
	    				          newVIrv */
	bp->b_api_private = sp;

	sp->bp = bp;
    }
    else 
	sp = bp->b_api_private;

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
	SV *sv = newSVpv(buf, 0);
	if (getregion(&region) == TRUE) {
	    sv_setiv(svStartLine, line_no(curbp, region.r_orig.l));
	    sv_setiv(svStopLine, line_no(curbp, region.r_end.l)-1);
	}
	else {
	    /* Should never get here; but just in case... */
	    sv_setiv(svStartLine, 1);
	    sv_setiv(svStopLine,  line_count(curbp));
	}

	newVIrv(svScreenId, curbp);
	newVIrv(svcurscr,   curbp);

	perl_eval_sv(sv, G_DISCARD | G_NOARGS);
	SvREFCNT_dec(sv);

	SvREFCNT_dec(SvRV(svScreenId));
	SvROK_off(svScreenId);
	SvREFCNT_dec(SvRV(svcurscr));
	SvROK_off(svcurscr);
	api_command_cleanup();
	return TRUE;
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
    
    /* Obtain handles to specific perl variables, creating them
       if they do not exist. */
    svStartLine = perl_get_sv("VI::StartLine", TRUE);
    svStopLine  = perl_get_sv("VI::StopLine",  TRUE);
    svScreenId  = perl_get_sv("VI::ScreenId",  TRUE);
    svcurscr    = perl_get_sv("curscr",        TRUE);

    /* Make the above readonly (from perl) */
    SvREADONLY_on(svStartLine);
    SvREADONLY_on(svStopLine);
    SvREADONLY_on(svScreenId);
    SvREADONLY_on(svcurscr);

    return TRUE;
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

# msg --
#	Set the message line to text.
#
# Perl Command: VI::Msg
# Usage: VI::Msg screenId text

void
Msg(screen, text)
	VI	screen
	char *	text
 
	ALIAS:
	PRINT = 1

	CODE:
	mlforce("%s", text);

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
	char *line, *p;

	PPCODE:
	INITMESSAGE;
	rval = api_gline(screen, linenumber, &p, &len);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSVpv(p, len)));

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

