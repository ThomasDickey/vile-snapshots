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
static int use_ml_as_prompt; 

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
	sp->perl_handle = newGVgen("VI"); 
	GvSV((GV*)sp->perl_handle) = newSV(0); 
	sv_setiv(GvSV((GV*)sp->perl_handle), (IV) sp); 
	SvREFCNT_inc(sp->perl_handle);
	sv_magic(sp->perl_handle, rv, 'q', Nullch, 0); 
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
    sv_setiv(GvSV((GV*)handle), 0); 

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

    hst_flush(); 
    hst_init(EOS); 
 
    use_ml_as_prompt = 0; 
 
    if (perl_interp || perl_init()) {
	REGION region;
	char *err;
	STRLEN length;
	SCR *curscr = api_bp2sp(curbp); 

	if (getregion(&region) != TRUE) { 
	    /* shouldn't ever get here. But just in case... */ 
	    perl_default_region(); 
	    if (getregion(&region) != TRUE) { 
		mlforce("BUG: getregion won't return TRUE in perl.xs."); 
	    } 
	}

	sv_setiv(svStartLine, line_no(curbp, region.r_orig.l)); 
	sv_setiv(svStopLine, line_no(curbp, region.r_end.l)-1); 
 
	/* Initialize some of the fields in curscr */ 
	curscr->region = region; 
	curscr->regionshape = regionshape; 
	curscr->inplace_edit = 0; 
 
	newVIrv(svScreenId, curscr); 
	newVIrv(svcurscr,   curscr); 

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

static int 
replace_line(void) 
{ 
    char *str; 
    STRLEN length; 
 
    if (SvOK(GvSV(defgv))) { 
	str = SvPV(GvSV(defgv), length); 
	if (   length == llength(DOT.l)  
	    && strncmp(str, DOT.l->l_text, length) == 0) 
	{ 
	    /* Don't uselessly fill up the undo buffers... */ 
	    return TRUE; 
	} 
	ldelete(llength(DOT.l), TRUE); 
	/* Now add the right stuff back in... */ 
	while (length-- > 0) { 
	    if (*str == '\n') 
		lnewline(); 
	    else 
		linsert(1,*str); 
	    str++; 
	} 
    } 
    else { 
	ldelete(llength(DOT.l) + 1, TRUE); 
    } 
    return TRUE; 
} 
 
int 
perldo(int f GCC_UNUSED, int n GCC_UNUSED) 
{ 
    register int status; 
    char buf[NLINE];	/* buffer to receive command into */ 
    REGION region; 
    char *err; 
    STRLEN length; 
    static char perldo_dcl[] = "sub VI::perldo {"; 
    SV *sv; 
    dSP; 
 
    buf[0] = EOS; 
    if ((status = mlreply_no_opts("Perl command: ", buf, sizeof(buf))) != TRUE) 
	    return(status); 
 
    use_ml_as_prompt = 0; 
 
    if (!perl_interp) { 
	if (!perl_init()) { 
	    return FALSE; 
	} 
	SPAGAIN; 
    } 
 
    if (status = get_fl_region(&region) != TRUE) { 
	return status; 
    } 
 
    length = strlen(buf); 
    sv = newSV(length + sizeof(perldo_dcl)-1 + 1); 
    sv_setpvn(sv, perldo_dcl, sizeof(perldo_dcl)-1); 
    sv_catpvn(sv, buf, length); 
    sv_catpvn(sv, "}", 1); 
    perl_eval_sv(sv, G_DISCARD | G_NOARGS); 
    SvREFCNT_dec(sv); 
    err = SvPV(GvSV(errgv), length); 
 
    if (length) 
	goto error; 
 
 
#if 0 
    lines_changed = 
    total_changes = 0; 
#endif 
    ENTER; 
    SAVETMPS; 
 
    newVIrv(svcurscr,   api_bp2sp(curbp)); 
 
    DOT.l = region.r_orig.l;	    /* Current line.	    */ 
    DOT.o = 0; 
    do { 
	LINEPTR nextline = lforw(DOT.l); 
 
	sv_setpvn(GvSV(defgv), DOT.l->l_text, llength(DOT.l)); 
	PUSHMARK(sp); 
	perl_call_pv("VI::perldo", G_SCALAR | G_EVAL); 
	err = SvPV(GvSV(errgv), length); 
	if (length != 0) 
	    break; 
	SPAGAIN; 
	if (SvTRUEx(POPs)) { 
	    if (replace_line() != TRUE) { 
		PUTBACK; 
		break; 
	    } 
	} 
	PUTBACK; 
 
	DOT.l = nextline; 
	DOT.o = 0; 
    } while (!sameline(DOT, region.r_end)); 
 
    SvREFCNT_dec(SvRV(svcurscr)); 
    SvROK_off(svcurscr); 
 
    FREETMPS; 
    LEAVE; 
 
#if 0 
    if (do_report(total_changes)) { 
	    mlforce("[%d change%s on %d line%s]", 
		    total_changes, PLURAL(total_changes), 
		    lines_changed, PLURAL(lines_changed)); 
    } 
#endif 
    if (length == 0) 
	return TRUE; 
error: 
    err[length - 1] = '\0'; 
    mlforce("%s", err); 
    return FALSE; 
} 
 
static int perl_init(void)
{
    char *embedding[] = { "", "-e", "0" };
    char *bootargs[]  = { "VI", NULL };
    SV   *svminiscr; 
    AV   *av; 
    SV   *sv; 

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

    /* Add our own paths to the front @INC */ 
    av_unshift(av = GvAVn(incgv), 2); 
    av_store(av, 0, newSVpv("~/.vile/perl",0)); 
    sv = newSVpv(HELP_LOC,0); 
    sv_catpv(sv, "perl"); 
    av_store(av, 1, sv); 
 
    
    /* Obtain handles to specific perl variables, creating them
       if they do not exist. */
    svStartLine = perl_get_sv("VI::StartLine", TRUE);
    svStopLine  = perl_get_sv("VI::StopLine",  TRUE);
    svScreenId  = perl_get_sv("VI::ScreenId",  TRUE);
    svcurscr    = perl_get_sv("curscr",        TRUE);

    svminiscr   = newVIrv(newSV(0), api_bp2sp(bminip)); 

    /* Tie STDOUT and STDERR to miniscr->PRINT() function */ 
    sv_magic((SV *) gv_fetchpv("STDOUT", TRUE, SVt_PVIO), svminiscr, 'q', 
	     Nullch, 0); 
    sv_magic((SV *) gv_fetchpv("STDERR", TRUE, SVt_PVIO), svminiscr, 'q', 
	     Nullch, 0); 
    sv_magic((SV *) gv_fetchpv("STDIN", TRUE, SVt_PVIO), svminiscr, 'q', 
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
	    use_ml_as_prompt = 1; 
	    free(text);
	}

# 
# PRINT 
 
void 
PRINT(screen, ...) 
	VI	screen 
  
	PREINIT: 
	int i; 
	char *text = 0; 
	STRLEN sz = 0; 
 
	CODE: 
	if (sp2bp(screen) == bminip) { 
	    int i; 
	    char *text = 0; 
	    STRLEN sz = 0; 
	    for (i = 1; i < items; i++) { 
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
 
 
	    if (text) { 
		mlforce("%s", text); 
		use_ml_as_prompt = 1; 
		free(text); 
	    } 
	} 
	else { 
	    for (i = 1; i < items; i++) { 
		STRLEN len; 
		char *arg = SvPV(ST(i), len); 
		api_dotinsert(screen, arg, len); 
	    } 
	} 
 
void 
READLINE(screen) 
	VI screen 
 
	PPCODE: 
	if (sp2bp(screen) == bminip) { 
	    int status; 
	    char buf[NLINE]; 
	    char prompt[NLINE]; 
	    buf[0] = EOS; 
	    strcpy(prompt, "(perl input) "); 
	    if (use_ml_as_prompt && !is_empty_buf(bminip)) { 
		LINE *lp = lback(buf_head(bminip)); 
		int len = llength(lp); 
		if (len > NLINE-1) 
		    len = NLINE-1; 
		strncpy(prompt, lp->l_text, len); 
		prompt[len] = 0; 
	    } 
	    status = mlreply_no_opts(prompt, buf, sizeof(buf)); 
	    EXTEND(sp,1); 
	    if (!status) { 
		PUSHs(&sv_undef); 
	    } 
	    else { 
		use_ml_as_prompt = 0; 
		PUSHs(sv_2mortal(newSVpv(buf,0))); 
	    } 
	} 
	else { 
	    I32 gimme = GIMME_V; 
	    if (gimme == G_VOID || gimme == G_SCALAR) { 
		size_t len; 
		int rval; 
		char *p; 
		rval = api_dotgline(screen, &p, &len); 
		if (gimme == G_SCALAR) { 
		    EXTEND(sp,1); 
		    if (!rval) { 
			PUSHs(&sv_undef); 
		    } 
		    else { 
			SV *sv = newSVpv(p, len); 
			sv_catpv(sv, "\n"); 
			PUSHs(sv_2mortal(sv)); 
		    } 
		} 
	    } 
	    else { /* wants an array */ 
		size_t len; 
		int status; 
		char *p; 
 
		while (api_dotgline(screen, &p, &len)) { 
		    SV *sv = newSVpv(p, len); 
		    sv_catpv(sv, "\n"); 
		    XPUSHs(sv_2mortal(sv)); 
		} 
	    } 
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

int 
VileCommand(cline) 
	char *cline; 
 
	CODE: 
	RETVAL = docmd(cline,FALSE,1); 
 
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
 
int 
inplace_edit(screen, ...) 
	VI screen 
 
	CODE: 
	RETVAL = ((SCR *) screen)->inplace_edit; 
	if (items > 1) 
	    ((SCR *) screen)->inplace_edit = SvIV(ST(1)); 
	 
	OUTPUT: 
	RETVAL 
 
void 
setregion(screen, ...) 
	VI screen 
 
	PREINIT: 
	SCR *scrp; 
	I32 gimme; 
	char *shapestr; 
 
	PPCODE: 
	scrp = (SCR *)screen; 
	api_setup_fake_win(scrp); 
	switch (items) { 
	    case 1: 
		/* set DOT */ 
		DOT = scrp->region.r_orig; 
		break; 
	    case 3: 
		/* Set up a full line region */ 
		api_gotoline(scrp, SvIV(ST(2))+1); 
		scrp->region.r_end = DOT; 
		api_gotoline(scrp, SvIV(ST(1))); 
		scrp->region.r_orig = DOT; 
		break; 
	    case 5: 
		/* Set up an exact region */ 
		scrp->regionshape = EXACT; 
		goto setregion_common; 
		break; 
	    case 6: 
		/* Set up any kind of region (exact, fullline,  
		   or rectangle) */ 
		shapestr = SvPV(ST(5), na); 
		if (strcmp(shapestr, "exact")) 
		    scrp->regionshape = EXACT; 
		else if (strcmp(shapestr, "rectangle")) 
		    scrp->regionshape = RECTANGLE; 
		else if (strcmp(shapestr, "fullline")) 
		    scrp->regionshape = FULLLINE; 
		else { 
		    croak("Region shape argument not one of \"exact\", \"fullline\", or \"rectangle\""); 
		} 
	    setregion_common: 
		if (scrp->regionshape == FULLLINE) { 
		    api_gotoline(scrp, SvIV(ST(3))+1); 
		    DOT.o = 0; 
		} 
		else { 
		    api_gotoline(scrp, SvIV(ST(3))); 
		    DOT.o = SvIV(ST(4)); 
		    if (DOT.o < 0) 
			DOT.o = 0; 
		    if (DOT.o > llength(DOT.l)) 
			DOT.o = llength(DOT.l); 
		} 
		scrp->region.r_end = DOT; 
 
		api_gotoline(scrp, SvIV(ST(1))); 
		if (scrp->regionshape == FULLLINE) { 
		    DOT.o = 0; 
		} 
		else { 
		    DOT.o = SvIV(ST(2)); 
		    if (DOT.o < 0) 
			DOT.o = 0; 
		    if (DOT.o > llength(DOT.l)) 
			DOT.o = llength(DOT.l); 
		} 
		scrp->region.r_orig = DOT; 
		break; 
	    default: 
		croak("Invalid number of arguments"); 
		break; 
	} 
	scrp->dot_inited = 1; 
	gimme = GIMME_V; 
	if (gimme == G_SCALAR) { 
	    warn("setregion should only be called in void or array context"); 
	    XPUSHs(sv_newmortal());		/* Push undef */ 
	} 
	else if (gimme == G_ARRAY) { 
	    /* Return range information */ 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, scrp->region.r_orig.l)))); 
	    XPUSHs(sv_2mortal(newSViv(scrp->region.r_orig.o))); 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, scrp->region.r_end.l) 
	                                             - (scrp->regionshape == FULLLINE)))); 
	    XPUSHs(sv_2mortal(newSViv(scrp->region.r_end.o))); 
	    XPUSHs(sv_2mortal(newSVpv( 
		scrp->regionshape == FULLLINE ? "fullline" : 
		scrp->regionshape == EXACT    ? "exact" 
		                            : "rectangle",    0 ))); 
	} 
 
 
# dot BUFOBJ 
# dot BUFOBJ LINENUM 
# dot BUFOBJ LINENUM, OFFSET 
#  
# 	Returns the current value of dot (which represents the the 
# 	current position in the buffer).  When used in a scalar 
# 	context returns, the line number of dot.  When used in an 
# 	array context, returns the line number and position within the 
# 	line. 
#  
# 	When supplied with one argument, the line number, dot is set to 
# 	the beginning of that line.  When supplied with two arguments, 
# 	both the line number and offset components are set. 
#  
# 	The line number may also be the special character '$' which 
# 	represents the last line of the buffer. 
#  
# 	Examples: 
#  
# 	$linenum = $curscr->dot;	# Fetch the line number at which dot 
# 					# is located. 
#  
# 	$curscr->dot($curscr->dot+1);	# Advance dot by one line 
# 	$curscr->dot($curscr->dot('$') - 1); 
# 					# Set dot to the penultimate line of 
# 					# the buffer. 
#  
# 	$curscr->dot(25, 6);		# Set dot to line 25, character 6 
#  
# 	($ln,$off) = $curscr->dot;	# Fetch the current position 
# 	$curscr->dot($ln+1,$off-1);	# and advance one line, but 
# 					# back one character. 
#  
# 	$curscr->inplace_edit(1); 
# 	$curscr->setregion(scalar($curscr->dot), $curscr->dot+5); 
# 	@lines = <$curscr>; 
# 	$curscr->dot($curscr->dot - 1); 
# 	print $curscr @lines; 
# 					# The above block takes (at 
# 					# most) six lines starting at 
# 					# the line DOT is on and moves 
# 					# them before the previous 
# 					# line. 
 
void 
dot(screen, ...) 
	VI screen 
 
	PREINIT: 
	SCR *scrp;  
	I32 gimme;  
 
	PPCODE: 
	scrp = (SCR *)screen;  
	api_setup_fake_win(scrp);  
	if ( items > 3) { 
	    croak("Invalid number of arguments");  
	} 
	else if (items > 1) { 
	    I32 linenum; 
	    /* Expect a line number or '$' */ 
	    if (!SvIOKp(ST(1)) && strcmp(SvPV(ST(1),na),"$") == 0) { 
		linenum = line_count(curbp); 
	    } 
	    else { 
		linenum = SvIV(ST(1)); 
		if (linenum < 1) { 
		    linenum = 1; 
		} 
		else if (linenum > line_count(curbp)) { 
		    linenum = line_count(curbp); 
		} 
	    } 
 
	    api_gotoline(scrp, linenum); 
 
	    if (items == 3) { 
		I32 offset; 
		/* Expect an offset within the line or '$' */ 
		if (!SvIOKp(ST(2)) && strcmp(SvPV(ST(2),na),"$") == 0) { 
		    offset = llength(DOT.l); 
		} 
		else { 
		    offset = SvIV(ST(2)); 
		    if (offset < 0) { 
			offset = 0; 
		    } 
		    else if (offset > llength(DOT.l)) { 
			offset = llength(DOT.l); 
		    } 
		} 
		DOT.o = offset; 
	    } 
	    /* Don't allow api_dotgline to change dot if dot is explicitly 
	       set.  OTOH, simply querying dot doesn't count. */ 
	    scrp->dot_inited = TRUE;  
	    /* Indicate that DOT has been explicitly changed which means 
	       that changes to DOT will be propogated upon return to vile */ 
	    scrp->dot_changed = TRUE; 
	} 
	gimme = GIMME_V; 
	if (gimme == G_SCALAR) { 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, DOT.l)))); 
	} 
	else if (gimme == G_ARRAY) { 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, DOT.l)))); 
	    XPUSHs(sv_2mortal(newSViv(DOT.o))); 
	} 
 
 
 
 
# 
# Various mlreply functions.  I lack sufficient imagination right now 
# to think of better names. 
# 
 
SV * 
mlreply(prompt, ...) 
	char *prompt 
 
	PREINIT: 
	char buf[NLINE]; 
	int status; 
 
	CODE: 
	if (items == 2) { 
	    strncpy(buf, SvPV(ST(1),na), NLINE-1); 
	    buf[NLINE-1] = EOS; 
	} 
	else if (items > 2) 
	    croak("Too many arguments to mlreply"); 
	else 
	    buf[0] = EOS; 
 
	status = mlreply(prompt, buf, sizeof(buf)); 
	RETVAL = status ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
	OUTPUT: 
	RETVAL 
 
SV * 
mlreply_no_opts(prompt, ...) 
	char *prompt 
 
	PREINIT: 
	char buf[NLINE]; 
	int status; 
 
	CODE: 
	if (items == 2) { 
	    strncpy(buf, SvPV(ST(1),na), NLINE-1); 
	    buf[NLINE-1] = EOS; 
	} 
	else if (items > 2) 
	    croak("Too many arguments to mlreply_no_opts"); 
	else 
	    buf[0] = EOS; 
 
	status = mlreply_no_opts(prompt, buf, sizeof(buf)); 
	RETVAL = status ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
	OUTPUT: 
	RETVAL 
 
 
SV * 
mlreply_file(prompt, ...) 
	char *prompt 
 
	PREINIT: 
	char buf[NFILEN]; 
	static TBUFF *last; 
	int status; 
 
	CODE: 
	if (items == 2) { 
	    tb_scopy(&last, SvPV(ST(1),na)); 
	} 
	else if (items > 2) { 
	    croak("Too many arguments to mlreply_file"); 
	} 
	 
	status = mlreply_file(prompt, &last, FILEC_UNKNOWN, buf); 
 
	RETVAL = status ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
	OUTPUT: 
	RETVAL 
 
SV * 
mlreply_dir(prompt, ...) 
	char *prompt 
 
	PREINIT: 
	char buf[NFILEN]; 
	static TBUFF *last; 
	int status; 
 
	CODE: 
	if (items == 2) { 
	    tb_scopy(&last, SvPV(ST(1),na)); 
	} 
	else if (items > 2) { 
	    croak("Too many arguments to mlreply_dir"); 
	} 
	 
	status = mlreply_dir(prompt, &last, buf); 
 
	RETVAL = status ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
	OUTPUT: 
	RETVAL 

