/*
 * perl.xs		-- perl interface for vile.
 *
 * Author: Kevin Buettner
 *
 * (with acknowledgments to the authors of the nvi perl interface as
 * well as Brendan O'Dea and Sean Ahern who both contributed snippets
 * of code here and there and many valuable suggestions.)
 * Created: Fall, 1997
 *
 * Description: The following code provides an interface to Perl from
 * vile.  The file api.c (sometimes) provides a middle layer between
 * this interface and the rest of vile.
 */

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

/* Some earlier versions of perl don't have GIMME_V or G_VOID. We must
   be careful of the order in which we check things if these defines
   are activated. */
#ifndef GIMME_V
#define GIMME_V GIMME
#endif

#ifndef G_VOID
#define G_VOID G_SCALAR
#endif


/* for vile */
#include "estruct.h"
#include "edef.h"

#include "api.h"

extern REGION *haveregion;

static PerlInterpreter *perl_interp;
static int use_ml_as_prompt; 
static SV *svcurbuf;		/* $Vile::current_buffer in perl */
static int svcurbuf_set(SV *, MAGIC *);
static MGVTBL svcurbuf_accessors = {
	/* Virtual table for svcurbuf magic. */
	NULL, svcurbuf_set, NULL, NULL, NULL
};

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

/*
 * Create a VB buffer handle object.  These objects are both
 * blessed into the Vile::Buffer class as well as made magical
 * so that they may also be used as filehandles.
 */
static SV *
newVBrv(SV *rv, VileBuf *sp)
{
    if (sp->perl_handle == 0) {
	sp->perl_handle = newGVgen("Vile::Buffer"); 
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
    return sv_bless(rv, gv_stashpv("Vile::Buffer", TRUE));
}

void
perl_free_handle(void *handle)
{
    /*
     * Zero out perl's handle to the VileBuf structure
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

static int recursecount = 0;

int
perl(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int status;
    char buf[NLINE];	/* buffer to receive command into */
    int old_discmd; 
    int old_isnamedcmd; 

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
	VileBuf *curvbp;
	
	if (recursecount == 0) {
	    curvbp = api_bp2vbp(curbp);

	    if (haveregion == NULL || getregion(&region) != TRUE) { 
		/* shouldn't ever get here. But just in case... */ 
		perl_default_region(); 
		if (getregion(&region) != TRUE) { 
		    mlforce("BUG: getregion won't return TRUE in perl.xs."); 
		} 
	    }

	    /* Initialize some of the fields in curvbp */ 
	    curvbp->region = region; 
	    curvbp->regionshape = regionshape; 
	    curvbp->inplace_edit = 0; 
     
	    sv_setsv(svcurbuf, newVBrv(sv_2mortal(newSV(0)), curvbp));
	}

	/* We set the following stuff up in the event that we call 
	   one of the mlreply methods.  If they are not set up this 
	   way, we won't always be prompted... */ 
        clexec = FALSE; 
        old_discmd = discmd; 
        discmd = TRUE; 
	old_isnamedcmd = isnamedcmd;	/* for mlreply_dir */ 
	isnamedcmd = TRUE; 
	recursecount++;
 
#define PDEBUG 0
#if PDEBUG
	printf("\nbefore eval\n");
        sv_dump(svcurbuf);
#endif
	perl_eval(buf);
#if PDEBUG
	printf("after eval\n");
        sv_dump(svcurbuf);
#endif

	recursecount--;
        discmd = old_discmd; 
	isnamedcmd = old_isnamedcmd; 
 
	if (recursecount == 0) {
	    SvREFCNT_dec(SvRV(svcurbuf));

	    api_command_cleanup();
	}

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
    static char perldo_dcl[] = "sub Vile::perldo {"; 
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
 
    if ((status = get_fl_region(&region)) != TRUE) { 
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
 
    sv_setsv(svcurbuf, newVBrv(sv_2mortal(newSV(0)), api_bp2vbp(curbp)));
 
    DOT.l = region.r_orig.l;	    /* Current line.	    */ 
    DOT.o = 0; 
    do { 
	LINEPTR nextline = lforw(DOT.l); 
 
	sv_setpvn(GvSV(defgv), DOT.l->l_text, llength(DOT.l)); 
	PUSHMARK(sp); 
	perl_call_pv("Vile::perldo", G_SCALAR | G_EVAL); 
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
 
    SvREFCNT_dec(SvRV(svcurbuf)); 
 
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

static int
svcurbuf_set(SV *sv, MAGIC *mg)
{
    VileBuf *vbp;
    if (sv_isa(sv, "Vile::Buffer")
        && (vbp = (VileBuf *) SvIV((SV*)GvSV((GV*)SvRV(sv)))) != NULL) 
    {
	api_swscreen(NULL, vbp);
    }
    else {
	/* FIXME: Print out warning about reseting things */
	/* Reset to curbp */
	sv_setsv(svcurbuf, newVBrv(sv_2mortal(newSV(0)), api_bp2vbp(curbp)));
    }
    return 1;
}
 
static int
perl_init(void)
{
    char *embedding[] = { "", "-e", "0" };
    char *bootargs[]  = { "Vile", NULL };
    SV   *svminibuf; 
    AV   *av; 
    SV   *sv; 
    char  temp[NFILEN]; 
    static char svcurbuf_name[] = "Vile::current_buffer";

    perl_interp = perl_alloc();
    perl_construct(perl_interp);

    if (perl_parse(perl_interp, xs_init, 3, embedding, NULL)) {
	perl_destruct(perl_interp);
	perl_free(perl_interp);
	perl_interp = NULL;
	return FALSE;
    }
    perl_call_argv("Vile::bootstrap", G_DISCARD, bootargs);
    perl_eval("$SIG{__WARN__}='Vile::Warn'");

    /* Add our own paths to the front of @INC */ 
    av_unshift(av = GvAVn(incgv), 2); 
    av_store(av, 0, newSVpv(lengthen_path(strcpy(temp,"~/.vile/perl")),0));  
    sv = newSVpv(HELP_LOC,0); 
    sv_catpv(sv, "perl"); 
    av_store(av, 1, sv); 
 
    
    /* Obtain handles to specific perl variables, creating them
       if they do not exist. */
    svcurbuf  = perl_get_sv(svcurbuf_name,  TRUE);

    svminibuf   = newVBrv(newSV(0), api_bp2vbp(bminip)); 

    /* Tie STDOUT and STDERR to miniscr->PRINT() function */ 
    sv_magic((SV *) gv_fetchpv("STDOUT", TRUE, SVt_PVIO), svminibuf, 'q', 
	     Nullch, 0); 
    sv_magic((SV *) gv_fetchpv("STDERR", TRUE, SVt_PVIO), svminibuf, 'q', 
	     Nullch, 0); 
    sv_magic((SV *) gv_fetchpv("STDIN", TRUE, SVt_PVIO), svminibuf, 'q', 
	     Nullch, 0);

    sv_magic(svcurbuf, NULL, '~', svcurbuf_name, strlen(svcurbuf_name));
    mg_find(svcurbuf, '~')->mg_virtual = &svcurbuf_accessors;
    SvMAGICAL_on(svcurbuf);

    /* Some things are better (or easier) to do in perl... */
    perl_eval("sub Vile::Buffer::PRINTF { \
                   my $fh=shift; my $fmt=shift;\
		   print $fh sprintf($fmt,@_);\
	       }");

    /* Load user or system wide initialization script */
    perl_eval("require 'vileinit.pl'");

    return TRUE;
}

static void
perl_eval(char *string)
{
    SV* sv = newSVpv(string, 0);

    sv_setpv(GvSV(errgv),"");
    perl_eval_sv(sv, G_DISCARD | G_NOARGS | G_KEEPERR);
    SvREFCNT_dec(sv);
}

/* Register any extra external extensions */

extern void boot_DynaLoader _((CV* cv));
extern void boot_Vile _((CV* cv));

static void 
xs_init() 
{
    char *file = __FILE__; 
    dXSUB_SYS; 
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file); 
    newXS("Vile::bootstrap", boot_Vile, file); 
} 

/*
 * Stringify a code ref so it may be called from vile.
 */

static char *CRfmtstr = "perl \"&{$Vile::CRs[%d]}\"";
static AV *CRarray   = 0;
static int freeCRidx = 0;

static char *
stringify_coderef(SV *coderef) {
    char buf[40];
    int idx;
    int badstore = 0;

    if (CRarray == 0) {
	/* Short name to keep the size of strings short on the vile side */
	CRarray = perl_get_av("Vile::CRs", TRUE);
	freeCRidx = -1;
    }

    if (freeCRidx >= 0) {
	SV **svp;
	idx = freeCRidx;
	svp = av_fetch(CRarray, (I32) idx, 0);
	if (svp == 0) {
	    /* Something's screwy... */
	    freeCRidx = -1;
	}
	else {
	    freeCRidx = SvIV(*svp);
	    SvREFCNT_dec(*svp);
	}
	if (av_store(CRarray, (I32) idx, SvREFCNT_inc(coderef)) == 0) {
	    badstore = 1;
	    SvREFCNT_dec(coderef);
	}
    }

    if (freeCRidx < 0 || badstore) {
	av_push(CRarray, SvREFCNT_inc(coderef));
	idx = av_len(CRarray);
    }

    sprintf(buf, CRfmtstr, idx);
    return strdup(buf);
}

void
perl_free_callback(char *callback)
{
    int idx;
    if (sscanf(callback, CRfmtstr, &idx) == 1) {
	SV **svp;
	SV *svfreeCRidx;
	svp = av_fetch(CRarray, (I32) idx, 0);
	if (svp == 0)
	    return;	/* Something screwy, bail... */

	if (!SvROK(*svp) || SvTYPE(SvRV(*svp)) != SVt_PVCV)
	    return;	/* Most likely freed already (?) */

	SvREFCNT_dec(*svp);	/* This should deallocate it */

	svfreeCRidx = SvREFCNT_inc(newSViv(freeCRidx));
	if (av_store(CRarray, (I32) idx, svfreeCRidx) == 0) {
	    /* Not successful (!) */
	    SvREFCNT_dec(svfreeCRidx);
	}
	else {
	    freeCRidx = idx;
	}
    }
}
 
/* 
 * Returns a line number given an SV.  '$' represents the last line 
 * in the file. '$$' represents the line after the last line.  The 
 * only time that '$' and '$$' represent the same line is when the 
 * buffer is empty. 
 */ 
 
static I32 
sv2linenum(SV *sv) 
{ 
    I32 linenum; 
 
    if (!SvIOKp(sv) && strcmp(SvPV(sv,na),"$") == 0) {  
	linenum = line_count(curbp);  
    }  
    else if (!SvIOKp(sv) && strcmp(SvPV(sv,na),"$$") == 0) {  
	linenum = line_count(curbp) + 1;  
    } 
    else {  
	linenum = SvIV(sv);  
	if (linenum < 1) {  
	    linenum = 1;  
	}  
	else if (linenum > line_count(curbp)) {  
	    linenum = line_count(curbp);  
	}  
    }  
    return linenum; 
} 
 
 
/*  
 * Returns an offset within the current line (where DOT is) given an 
 * SV.  '$' represents the last non-newline character in the line.  '$$' 
 * represents the newline character.  The only time '$' and '$$' represent 
 * the same position is when the line is empty. 
 */ 
 
static I32 
sv2offset(SV *sv) 
{ 
    I32 offset;  
    if (!SvIOKp(sv) && strcmp(SvPV(sv,na),"$") == 0) {  
	offset = llength(DOT.l) - 1;  
	if (offset < 0) 
	    offset = 0; 
    }  
    else if (!SvIOKp(sv) && strcmp(SvPV(sv,na),"$$") == 0) {  
	offset = llength(DOT.l);  
    } 
    else {  
	offset = SvIV(sv);  
	if (offset < 0) {  
	    offset = 0;  
	}  
	else if (offset > llength(DOT.l)) {  
	    offset = llength(DOT.l);  
	}  
    }  
    return offset; 
}

/* 
 * Fetch a line or portion thereof from the current buffer.  Like
 * api_dotgline(), except it's faster because it creates an SV of
 * the right size from the outset.  It's also faster because it
 * relies on the caller to set up the fake window properly.  (Not
 * a big deal if only getting a scalar, but it can be if fetching
 * an entire array.)
 *
 * Does not handle rectangular regions.
 */

static int
svgetline(SV **svp, VileBuf *vbp, char *rsstr, int rslen)
{
    int len;
    int nllen;
    char *text;
    SV *sv;
    if (   is_header_line(DOT, curbp)  
        || (   DOT.l == vbp->region.r_end.l  
	    && (   vbp->regionshape == FULLLINE 
	        || (   vbp->regionshape == EXACT 
		    && DOT.o >= vbp->region.r_end.o)))) 
    { 
	*svp = newSVsv(&sv_undef);
	return FALSE; 
    } 

    len = llength(DOT.l) - DOT.o;
    text = DOT.l->l_text + DOT.o;

    if (vbp->regionshape == EXACT && DOT.l == vbp->region.r_end.l) { 
	len -= llength(DOT.l) - vbp->region.r_end.o;
	nllen = 0;
	if (vbp->inplace_edit)
	    vbp->ndel += len;
	DOT.o += len;
    }
    else {
	/* FIXME: Doesn't DOS use \r\n?  Maybe we should set nllen to 2
	   in this case. */
	if (lforw(DOT.l) != buf_head(curbp) || b_val(curbp, MDNEWLINE))
	    nllen = 1;
	else
	    nllen = 0;
	if (vbp->inplace_edit)
	    vbp->ndel += len + (nllen != 0);
	DOT.o = 0;
	DOT.l = lforw(DOT.l);
    }

    if (len < 0)
	len = 0;

    *svp = sv = newSV(len + nllen + 1);	/* +1 for \0 */

    if (len > 0) {
	sv_setpvn(sv, text, len);
	if (nllen > 0)
	    sv_catpvn(sv, "\n", 1);
    }
    else if (nllen > 0) {
	sv_setpvn(sv, "\n", 1);
    }
    else {
	sv_setpvn(sv, "", 0);
    }

    return TRUE;
}

/*
 * Fetch an entire region (or remainder thereof).
 */

static int
svgetregion(SV **svp, VileBuf *vbp, char *rsstr, int rslen)
{
    int len;
    SV *sv;
    LINEPTR lp, llp;
    int off;

    if (   is_header_line(DOT, curbp)  
        || (   DOT.l == vbp->region.r_end.l  
	    && (   vbp->regionshape == FULLLINE 
	        || (   vbp->regionshape == EXACT 
		    && DOT.o >= vbp->region.r_end.o)))) 
    { 
	*svp = newSVsv(&sv_undef);
	return FALSE; 
    } 

    /* Compute length of region */
    lp = DOT.l;
    llp = vbp->region.r_end.l;
    off = DOT.o;
    len = 0;
    for (;;) {
	if (lp == llp) {
	    if (vbp->regionshape == EXACT) {
		int elen = vbp->region.r_end.o - off;
		if (elen < 0)
		    elen = 0;
		len += elen;
	    }
	    break;
	}
	else {
	    len += llength(lp) + 1 - off;	/* + 1 for the newline */
	}
	lp = lforw(lp);
	off = 0;
    }

    if (len < 0)
	len = 0;				/* shouldn't happen... */

    *svp = sv = newSV(len + 1);	/* + 1 for \0 */
    sv_setpvn(sv, "", 0);

    if (vbp->inplace_edit)
	vbp->ndel += len;

    lp = DOT.l;
    off = DOT.o;
    while (len > 0) {
	int clen = llength(lp) - off;

	if (clen > len)
	    clen = len;

	if (clen > 0) {
	    sv_catpvn(sv, lp->l_text + off, clen);
	    len -= clen;
	    off += clen;
	}

	if (len > 0) {
	    if (lforw(lp) != buf_head(curbp) || b_val(curbp, MDNEWLINE))
		sv_catpvn(sv, "\n", 1);
	    len--;
	    off++;
	}

	if (off > llength(lp)) {
	    lp = lforw(lp);
	    off = 0;
	}
    }
    DOT.l = lp;
    DOT.o = off;

    return TRUE;
}

/*
 * Gets the next portion of a region up to the next record separator
 * or the end of region, whichever comes first.
 */

static int
svgettors(SV **svp, VileBuf *vbp, char *rsstr, int rslen)
{
    int len;
    SV *sv;
    int rs1;
    int orig_rslen = rslen;
    LINEPTR lp, llp;
    int off;


    /* See if we're already at the end of the region and have nothing
       to do. */
    if (   is_header_line(DOT, curbp)  
        || (   DOT.l == vbp->region.r_end.l  
	    && (   vbp->regionshape == FULLLINE 
	        || (   vbp->regionshape == EXACT 
		    && DOT.o >= vbp->region.r_end.o)))) 
    { 
	*svp = newSVsv(&sv_undef);
	return FALSE; 
    } 

    /* Adjust rsstr if need be */
    if (rslen == 0) {
	rsstr = "\n\n";
	rslen = 2;
    }

    /* Get first separator character */
    rs1 = *rsstr;

    /* Compute length of region up to record separator or til
       end of region, whichever comes first */
    lp  = DOT.l;
    llp = vbp->region.r_end.l;
    off = DOT.o;
    len = 0;
    for (;;) {
	int loff;
	int cont_off;
	LINEPTR cont_lp;
	int fidx;
	int rsidx;

	if (off > llength(lp)) {
	    off = 0;
	    lp = lforw(lp);
	}

	if (   lp == buf_head(curbp)  
	    || (   lp == llp
		&& (   vbp->regionshape == FULLLINE 
		    || (   vbp->regionshape == EXACT 
			&& off >= vbp->region.r_end.o))))
	{
	    goto have_length;
	}

	/* loff is the last offset that we'll do our initial search
	   to on this line */
	loff = llength(lp);

	/* Adjust loff as appropriate if on last line in region */
	if (lp == llp) {
	    if (vbp->regionshape == EXACT) {
		if (vbp->region.r_end.o < loff)
		    loff = vbp->region.r_end.o;
	    }
	    else 
		goto have_length;
	}

	/* Try to find the first record separator character */
	if (rs1 == '\n') {
	    /* newline; no searching needed, must be at end of line */
	    if (loff < llength(lp)) {
		len += loff;
		goto have_length;
	    }
	    else
		fidx = loff;
	}
	else {
	    /* Gotta search */
	    for (fidx = off; fidx < loff && lp->l_text[fidx] != rs1; fidx++)
		;
	    if (fidx >= loff) {
		if (loff < llength(lp)) {
		    len += loff;
		    goto have_length;
		}
		len += loff - off + 1;
		off = loff + 1;
		continue;
	    }
	}

	/* If we get to this point, fidx points at first character in 
	   the record separator. */
	len += fidx - off + 1;
	cont_lp = lp;
	cont_off = fidx + 1;

	/* Attempt to match the rest of the record separator */
	for (rsidx = 1; rsidx < rslen; rsidx++) {
	    fidx++;
	    if (fidx >= llength(lp)) {
		lp = lforw(lp);
		fidx = 0;
	    }
	    if (   lp == buf_head(curbp)  
		|| (   lp == llp
		    && (   vbp->regionshape == FULLLINE 
			|| (   vbp->regionshape == EXACT 
			    && fidx >= vbp->region.r_end.o))))
	    {
		off = fidx;
		goto have_length;
	    }
	    if (rsstr[rsidx] == '\n') {
		if (fidx < llength(lp))
		    break;
	    }
	    else if (rsstr[rsidx] != lp->l_text[fidx])
		break;
	}

	if (rsidx >= rslen) {
	    len += rslen - 1;
	    off = fidx;
	    goto have_length;
	}
	lp = cont_lp;
	off = cont_off;
    }
have_length:

    /* See if we have the special paragraph separator and if so, consume
       as many additional newlines as we can */
    if (orig_rslen == 0) {
	lp = lforw(lp);
	off = 0;
	while (  ! (   lp == buf_head(curbp)  
		    || (   lp == llp
			&& (   vbp->regionshape == FULLLINE 
			    || (   vbp->regionshape == EXACT 
				&& off >= vbp->region.r_end.o))))
	       && llength(lp) == 0)
	{
	    len++;
	    lp = lforw(lp);
	    off = 0;
	}
    }

    /* Make sure there's still something left to return */
    if (len <= 0) {
	*svp = newSVsv(&sv_undef);
	return FALSE; 
    }

    /* Now copy the region over to the SV... */
    *svp = sv = newSV(len + 1);	/* + 1 for \0 */
    sv_setpvn(sv, "", 0);

    if (vbp->inplace_edit)
	vbp->ndel += len;

    lp = DOT.l;
    off = DOT.o;
    while (len > 0) {
	int clen = llength(lp) - off;

	if (clen > len)
	    clen = len;

	if (clen > 0) {
	    sv_catpvn(sv, lp->l_text + off, clen);
	    len -= clen;
	    off += clen;
	}

	if (len > 0) {
	    if (lforw(lp) != buf_head(curbp) || b_val(curbp, MDNEWLINE))
		sv_catpvn(sv, "\n", 1);
	    len--;
	    off++;
	}

	if (off > llength(lp)) {
	    lp = lforw(lp);
	    off = 0;
	}
    }
    DOT.l = lp;
    DOT.o = off;

    return TRUE;
}

/* Note: The commentary below prefixed with # characters may be
   retrieved via

   	perl -ne 'print $1 if /^#((\s|$).*)$/s' perl.xs
*/

MODULE = Vile	PACKAGE = Vile

PROTOTYPES: DISABLE

MODULE = Vile	PACKAGE = Vile::Buffer

#
# current_buffer
# current_buffer BUFOBJ
# current_buffer PKGNAME
# current_buffer BUFOBJ   NEWBUFOBJ
# current_buffer PKGNAME  NEWBUFOBJ
#
#	Returns the current buffer.  When first entering perl from a
#	vile session, the current buffer is the one that the user is
#	actively editing.  Several buffers may be on the screen at once,
#	but only one of them is current.  The current one will be the
#	one in which the cursor appears.
#
# 	This method may also be used to set the current buffer.  When used
# 	in the form
#
#		$oldbuf->current_buffer($newbuf)
#
# 	then $newbuf will replace $oldbuf in one of the visible windows.
#	(This only makes sense when $oldbuf was visible in some window
#	on the screen.  If it wasn't visible, it'll just replace whatever
#	buffer was last both current and visible.)
#
#	When used as a setter, the current buffer is still returned.  In
#	this case it will be the new buffer object which becomes the
#	current buffer.
#
#	Note also that the current_buffer method is in both the Vile
#	module and the Vile::Buffer module.  I couldn't decide which
#	module it should be in so I put it into both.  It seemed like
#	a real hassle to have to say
#
#		my $curbuf = Vile::Buffer->current_buffer
#
# 	So instead, you can just say
#
#		my $curbuf = Vile->current_buffer;
#
# 	current_buffer is also a variable, so you can also do it this
#	way:
#
#		my $curbuf = $Vile::current_buffer;
#
#	If you want $main::curbuf (or some other variable) to be an
#	alias to the current buffer, you can do it like this:
#
#		*main::curbuf = \$Vile::current_buffer;
#
#	Put this in some bit of initialization code and then you'll
#	never have to call the current_buffer method at all.
#
#	One more point, since $Vile::current_buffer is magical, the
#	alias above will be magical too, so you'll be able to do
#
#		$curbuf = $newbuf;
#
# 	in order to set the buffer.  (Yeah, this looks obvious, but
#	realize that doing the assignment actually causes some vile
#	specific code to run which will cause $newbuf to become the
#	new current buffer upon return.)
#

VileBuf *
current_buffer(...)

    ALIAS:
	Vile::current_buffer = 1

    PREINIT:
	VileBuf *callbuf;
	VileBuf *newbuf;

    PPCODE:
	if (items > 2)
	    croak("Too many arguments to current_buffer");
	else if (items == 2) {
	    if (sv_isa(ST(0), "Vile::Buffer")) {
		callbuf = (VileBuf *)SvIV((SV*)GvSV((GV*)SvRV(ST(0))));
		if (callbuf == 0) {
		    croak("buffer no longer exists");
		}
	    }
	    else
		callbuf = 0;

	    if (sv_isa(ST(1), "Vile::Buffer")) {
		newbuf = (VileBuf *)SvIV((SV*)GvSV((GV*)SvRV(ST(1))));
		if (newbuf == 0) {
		    croak("switched to buffer no longer exists");
		}
	    }
	    else {
		croak("switched to buffer of wrong type");
	    }

	    if (api_swscreen(callbuf, newbuf))
		sv_setsv(svcurbuf, ST(1));
	}

	XPUSHs(svcurbuf);

# 
# print BUFOBJ STR1,..,STRN
# insert BUFOBJ STR1,...,STRN
#
# 	Inserts one or more strings the buffer object at the current
# 	position of DOT.  DOT will be left at the end of the strings
# 	just inserted.
#
# 	When STDERR or STDOUT are printed to, the output will be
#	directed to the message line.
#
#	Examples:
#
#	print "Hello, world!";		# Print a well known greeting on
#					# the message line.
#	print $Vile::current_buffer "new text";
#					# put some new text in the current
#					# buffer.
#
#	my $passbuf = new Vile::Buffer '/etc/passwd';
#					# Fetch the password file
#	$passbuf->dot('$$');		# Set the position at the end
#	print $passbuf "joeuser::1000:100:Joe User:/home/joeuser:/bin/bash
#					# Add 'joeuser' to the this buffer
#	Vile->current_buffer($passbuf);	# Make it visible to the user.
#

void
PRINT(vbp, ...)
    VileBuf *vbp

    ALIAS:
	insert = 1

    CODE:
	if (vbp2bp(vbp) == bminip) {
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

	    if (text && sz >= 1) {
		if (text[sz-1] == '\n')
		    text[sz-1] = 0;
		mlforce("%s", text);
		use_ml_as_prompt = 1;
		free(text);
	    }
	}
	else {
	    int i;
	    for (i = 1; i < items; ) {
		STRLEN len;
		char *arg = SvPV(ST(i), len);
		api_dotinsert(vbp, arg, len);
		i++;
		if (i < items && ofslen > 0)
		    api_dotinsert(vbp, ofs, ofslen);
	    }
	    if (orslen > 0)
		api_dotinsert(vbp, ors, orslen);
	}
 
#
# <BUFOBJ>
#
# 	When used in a scalar context, returns the next line or portion
#	of thereof in the current region.
#
# 	When used in an array context, returns the rest of the lines (or
# 	portions thereof) in the current region.
#
# 	The current region is either set with setregion or set by default
# 	for you when perl is invoked from vile.  This region will either
# 	be the region that the user specified or the whole buffer if not
# 	user specified.  Unless you know for sure that the region is set
#	properly, it is probably best to set it explicitly.
#
# 	After a line is read, DOT is left at the next location in the
#	buffer at which to start reading.  Note, however, that the value
#	of DOT (which a convenient name for the current position in the
#	buffer) is not propogated back to any of the users windows unless
#	it has been explicitly set by calling dot (the method).
#
#	When the inplace_edit flag has been set via the inplace_edit
#	method, text that is retrieved from the buffer is deleted
#	immediately after retrieval.
#
#	Examples:
#
#	# Example 1: Put all lines in the current buffer into
#	#            an array
#
#	$Vile::current_buffer->setregion(1,'$$');
#					# Set the region to be the
#					# entire buffer.
#	my @lines = <$Vile::current_buffer>;
#					# Fetch all lines and put them
#					# in the @lines array.
#	print $lines[$#lines/2] if @lines;
#					# Print the middle line to
#					# the status line
#
#	
#	# Example 2: Selectively delete lines from a buffer
#
#	my $curbuf = $Vile::current_buffer;
#					# get an easier to type handle
#					# for the current buffer
#	$curbuf->inplace_edit(1);	# set the inplace_edit flag
#					# so that lines will be deleted
#					# as they are read
#
#	while (<$curbuf>) {		# fetch line into $_
#	    unless (/MUST\s+DELETE/) {	# see if we should keep the line
#		print $curbuf $_;	# put it back if we should keep it
#	    }
#       }
#

void
READLINE(vbp)
    VileBuf * vbp
 
    PPCODE:
	if (vbp2bp(vbp) == bminip) {
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
	    if (status != TRUE && status != FALSE) {
		PUSHs(&sv_undef);
	    }
	    else {
		use_ml_as_prompt = 0;
		PUSHs(sv_2mortal(newSVpv(buf,0)));
	    }
	}
	else {
	    I32 gimme = GIMME_V;
	    struct MARK old_DOT;
	    int (*f)(SV**,VileBuf*,char*,int);
	    char *rsstr;
	    int rslen;

	    if (RsSNARF(rs)) {
		f = svgetregion;
		rsstr = 0;
		rslen = 0;
	    }
	    else {
		rsstr = SvPV(rs, rslen);
		if (rslen == 1 && *rsstr == '\n')
		    f = svgetline;
		else
		    f = svgettors;
	    }

	    /* Set up the fake window */
	    api_setup_fake_win(vbp, TRUE);
	    if (!vbp->dot_inited) {
		DOT = vbp->region.r_orig;	/* set DOT to beginning of region */
		vbp->dot_inited = 1;
	    }

	    old_DOT = DOT;

	    if (gimme == G_VOID || gimme == G_SCALAR) {
		SV *sv;
		(void) f(&sv, vbp, rsstr, rslen);
		if (gimme == G_SCALAR) {
		    XPUSHs(sv_2mortal(sv));
		}
	    }
	    else { /* wants an array */
		SV *sv;
 
		while (f(&sv, vbp, rsstr, rslen)) {
		    XPUSHs(sv_2mortal(sv));
		}
	    }
	    if (vbp->inplace_edit) {
		DOT = old_DOT;
	    }
	}

#
# fetch BUFOBJ
#
#	Returns the current region or remainder thereof.
#

void
fetch(vbp)
    VileBuf * vbp
 
    PREINIT:
	SV *sv;
	struct MARK old_DOT;

    PPCODE:
	/* Set up the fake window */
	api_setup_fake_win(vbp, TRUE);
	if (!vbp->dot_inited) {
	    DOT = vbp->region.r_orig;	/* set DOT to beginning of region */
	    vbp->dot_inited = 1;
	}

	old_DOT = DOT;

	svgetregion(&sv, vbp, 0, 0);

	XPUSHs(sv_2mortal(sv));

	if (vbp->inplace_edit)
	    DOT = old_DOT;

 

#
# new BUFOBJ
# new PKGNAME
# new BUFOBJ  FILENAME
# new PKGNAME FILENAME
#
# edit BUFOBJ
# edit PKGNAME
# edit BUFOBJ  FILENAME
# edit PKGNAME FILENAME
# 
# 	These methods create a new buffer and return it.
#
# 	When no filename is supplied, an anonymous buffer is created.
#	These buffer's will be named [unnamed-1], [unnamed-2], etc. and
#	will not have a file name associated with them.
#
# 	When a name is supplied as an argument to new or edit, a check
#	is made to see if the name is the same as an already existing
#	buffer.  If so, that buffer is returned.  Otherwise, the name is
#	taken to be a file name.  If the file exists, it is opened and
#	read into the newly created buffer.  If the file does not exist,
#	a new buffer will be created with the associated file name.  The
#	name of the buffer will be based on the file name.  The file will
#	be created when the buffer is first written out to disk.
#
# 	new and edit are synonyms.  In each case, PKGNAME is Vile::Buffer.
# 	There is no difference between Vile::Buffer->new($fname) and
# 	$buf->new($fname).  These two different forms are merely provided
#	for convenience.
#
# 	Example:
#
#	$Vile::current_buffer = new Vile::Buffer 'makefile';
#					# open makefile and make it visible
#					# on the screen.
#
#	$abuf = new Vile::Buffer;	# Create an anonymous buffer
#	print $abuf "Hello";		# put something in it
#	Vile->current_buffer($abuf);	# make the anonymous buffer current
#					#   (viewable).
#
#	Vile->current_buffer($abuf->edit('makefile'));
#					# Now makefile is the current
#					#   buffer
#	$abuf->current_buffer(Vile::Buffer->new('makefile'));
#					# Same thing
#

VileBuf *
new(...)

    ALIAS:
	edit = 1

    PREINIT:
	char *name;
	VileBuf *newvbp;

    CODE:
	if (items > 2)
	    croak("Too many arguments to current_buffer");

	name = (items == 1) ? NULL : (char *)SvPV(ST(1),na);

	(void) api_edit(NULL, name, &newvbp);
    
	RETVAL = newvbp;

    OUTPUT:
	RETVAL

  
#
# inplace_edit BUFOBJ 
# inplace_edit BUFOBJ VALUE 
# 	 
# 	Sets the value of the "inplace edit" flag (either true of false). 
# 	Returns the old value.  When used without an argument, merely 
# 	returns current value without modifying the current value. 
#  
# 	This flag determines whether a line is deleted after being 
# 	read.  E.g, 
#  
#	my $curbuf = $Vile::current_buffer;
# 	$curbuf->inplace_edit(1); 
# 	while (<$curbuf>) { 
# 		s/foo/bar/g; 
# 		print; 
# 	} 
#  
# 	The <$curbuf> operation will cause one line to be read and 
# 	deleted.  DOT will be left at the beginning of the next line. 
# 	The print statment will cause $_ to get inserted prior the 
# 	the next line. 
#  
# 	Setting this flag to true is very similar to setting the 
# 	$INPLACE_EDIT flag (or $^I) for normal filehandles or 
# 	using the -i switch from the command line. 
#  
# 	Setting it to false (which is its default value) will cause 
# 	the lines that are read to be left alone. 
#
 
int 
inplace_edit(vbp, ...) 
    VileBuf *vbp
 
    CODE:
	RETVAL = vbp->inplace_edit; 
	if (items > 1) 
	    vbp->inplace_edit = SvIV(ST(1)); 
	 
    OUTPUT:
	RETVAL 
 
#
# setregion BUFOBJ 
# setregion BUFOBJ MOTIONSTR
# setregion BUFOBJ STARTLINE, ENDLINE 
# setregion BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET 
# setregion BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET, 'rectangle' 
# setregion BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET, 'exact' 
#  
#	Sets the region upon which certain other methods will operate
#	and sets DOT to the beginning of the region.  
# 
#	Either the line number or offset (or both) may be the special 
#	string '$' which represents the last line in the buffer and the 
#	last character on a line. 
# 
#	Often times, however, the special string '$$' will be more 
#	useful.  It truly represents the farthest that it possible 
#	to go in both the vertical and horizontal directions.  As 
#	a line number, this represents the line beyond the last 
#	line of the buffer.  Characters inserted at this point 
#	will form a new line.  As an offset, '$$' refers to the 
#	newline character at the end of a line.  Characters inserted 
#	at this point will be inserted before the newline character. 
# 
#	When used in an array context, returns a five element array 
#	with the start line, start offset, end line, end offset, and a 
#	string indicating the type of region (one of 'line', 
#	'rectangle', or 'exact'). 
# 
#	When used in a scalar context, returns the buffer object 
#	so that cascading method calls may be performed, i.e, 
# 
#		$Vile::current_buffer->setregion(3,4)
#                                    ->attribute_cntl_a_sequences; 
#	
#	There is a special form of setregion which may be used
#	as follows:
#
#		$Vile::current_buffer->setregion('j2w');
#
#	The above statement will set the region beginning at the current
#	location of DOT and ending at the location arrived at by moving
#	down one line and over two words.  This may be viewed as a shorthand
#	way of expressing the following (somewhat cumbersome) statement:
#
#		$Vile::current_buffer->setregion(
#			$Vile::current_buffer->motion('j2w'));
# 
# Note: rectangular regions are not implemented yet. 
#
 
void 
setregion(vbp, ...) 
    VileBuf *vbp
 
    PREINIT:
	I32 gimme; 
	char *shapestr; 
 
    PPCODE:
	api_setup_fake_win(vbp, TRUE);
	switch (items) { 
	    case 1: 
		/* set DOT */ 
		DOT = vbp->region.r_orig; 
		break; 
	    case 2: {
		/* Set up a "motion" region */
		vbp->regionshape   = EXACT;
		vbp->region.r_orig = DOT;
		if (api_motion(vbp, SvPV(ST(1), na))) {
		    if ((line_no(curbp, vbp->region.r_orig.l)
		                 > line_no(curbp, DOT.l))
		        || (vbp->region.r_orig.l == DOT.l
			    && vbp->region.r_orig.o > DOT.o))
		    {
			/* DOT's at beginning of region */
			vbp->region.r_end = vbp->region.r_orig;
			vbp->region.r_orig = DOT;
		    }
		    else {
			/* DOT's (presently) at end of region */
			vbp->region.r_end = DOT;
			DOT = vbp->region.r_orig;
		    }
		}
		else {
		    /* Return an empty (?) region */
		    vbp->region.r_end = DOT;
		}
		break;
	    }
	    case 3: 
		/* Set up a full line region */ 
		api_gotoline(vbp, sv2linenum(ST(2))+1);  
		vbp->region.r_end = DOT; 
		api_gotoline(vbp, sv2linenum(ST(1)));  
		vbp->region.r_orig = DOT; 
		break; 
	    case 5: 
		/* Set up an exact region */ 
		vbp->regionshape = EXACT; 
		goto setregion_common; 
		break; 
	    case 6: 
		/* Set up any kind of region (exact, fullline,  
		   or rectangle) */ 
		shapestr = SvPV(ST(5), na); 
		if (strcmp(shapestr, "exact")) 
		    vbp->regionshape = EXACT; 
		else if (strcmp(shapestr, "rectangle")) 
		    vbp->regionshape = RECTANGLE; 
		else if (strcmp(shapestr, "fullline")) 
		    vbp->regionshape = FULLLINE; 
		else { 
		    croak("Region shape argument not one of \"exact\", \"fullline\", or \"rectangle\""); 
		} 
	    setregion_common: 
		if (vbp->regionshape == FULLLINE) { 
		    api_gotoline(vbp, sv2linenum(ST(3))+1);  
		    DOT.o = 0; 
		} 
		else { 
		    api_gotoline(vbp, sv2linenum(ST(3)));  
		    DOT.o = sv2offset(ST(4));  
		} 
		vbp->region.r_end = DOT; 
 
		api_gotoline(vbp, SvIV(ST(1))); 
		if (vbp->regionshape == FULLLINE) 
		    DOT.o = 0; 
		else 
		    DOT.o = sv2offset(ST(2));  
		vbp->region.r_orig = DOT; 
		break; 
	    default: 
		croak("Invalid number of arguments to setregion"); 
		break; 
	} 
	vbp->dot_inited = 1; 
	gimme = GIMME_V; 
	if (gimme == G_SCALAR) { 
	    XPUSHs(ST(0)); 
	} 
	else if (gimme == G_ARRAY) { 
	    /* Return range information */ 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, vbp->region.r_orig.l)))); 
	    XPUSHs(sv_2mortal(newSViv(vbp->region.r_orig.o))); 
	    XPUSHs(sv_2mortal(newSViv(line_no(curbp, vbp->region.r_end.l) 
	                                             - (vbp->regionshape == FULLLINE)))); 
	    XPUSHs(sv_2mortal(newSViv(vbp->region.r_end.o))); 
	    XPUSHs(sv_2mortal(newSVpv( 
		vbp->regionshape == FULLLINE ? "fullline" : 
		vbp->regionshape == EXACT    ? "exact" 
		                            : "rectangle",    0 ))); 
	} 
 
 
#
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
#	Either the line number or offset (or both) may be the special 
#	string '$' which represents the last line in the buffer and the 
#	last character on a line. 
# 
#	Often times, however, the special string '$$' will be more 
#	useful.  It truly represents the farthest that it possible 
#	to go in both the vertical and horizontal directions.  As 
#	a line number, this represents the line beyond the last 
#	line of the buffer.  Characters inserted at this point 
#	will form a new line.  As an offset, '$$' refers to the 
#	newline character at the end of a line.  Characters inserted 
#	at this point will be inserted before the newline character. 
#	 
#  
# 	Examples: 
#
#	my $cb = $VILE::current_buffer;	# Provide a convenient handle
#					# for the current buffer.
#  
# 	$linenum = $cb->dot;		# Fetch the line number at which dot 
# 					# is located. 
#  
# 	$cb->dot($cb->dot+1);		# Advance dot by one line 
# 	$cb->dot($cb->dot('$') - 1); 
# 					# Set dot to the penultimate line of 
# 					# the buffer. 
#  
# 	$cb->dot(25, 6);		# Set dot to line 25, character 6 
#  
# 	($ln,$off) = $cb->dot;		# Fetch the current position 
# 	$cb->dot($ln+1,$off-1);		# and advance one line, but 
# 					# back one character. 
#  
# 	$cb->inplace_edit(1); 
# 	$cb->setregion(scalar($cb->dot), $cb->dot+5); 
# 	@lines = <$cb>; 
# 	$cb->dot($cb->dot - 1); 
# 	print $cb @lines; 
# 					# The above block takes (at 
# 					# most) six lines starting at 
# 					# the line DOT is on and moves 
# 					# them before the previous 
# 					# line. 
#
 
void 
dot(vbp, ...) 
    VileBuf *vbp
 
    PREINIT:
	I32 gimme;  
 
    PPCODE:
	api_setup_fake_win(vbp, TRUE);
	if ( items > 3) { 
	    croak("Invalid number of arguments");  
	} 
	else if (items > 1) { 
	    /* Expect a line number or '$' */ 
 
	    api_gotoline(vbp, sv2linenum(ST(1)));  
 
	    if (items == 3) 
		DOT.o = sv2offset(ST(2));  
 
	    /* Don't allow api_dotgline to change dot if dot is explicitly 
	       set.  OTOH, simply querying dot doesn't count. */ 
	    vbp->dot_inited = TRUE;  
	    /* Indicate that DOT has been explicitly changed which means 
	       that changes to DOT will be propogated upon return to vile */ 
	    vbp->dot_changed = TRUE; 
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
# delete BUFOBJ
#
#	Deletes the currently set region.
#
#	Returns the buffer object if all went well, undef otherwise.
#

VileBuf *
delete(vbp)
    VileBuf *vbp

    CODE:
	if (api_delregion(vbp))
	    RETVAL = vbp;
	else
	    RETVAL = 0;		/* which gets turned into undef */
    OUTPUT:
	RETVAL

#
# motion BUFOBJ MOTIONSTR
#
#	Moves dot (the current position) by the given MOTIONSTR in
#	BUFOBJ.
#
#	When used in an array context, returns a 4-tuple containing
#	the beginning and ending positions.  This 4-tuple is suitable
#	for passing into setregion.
#
#	When used in a scalar context, returns the buffer object that
#	it was called with.
#
#	In either an array or scalar context, if the motion string was
#	bad, and undef is returned.  Motions that don't work are okay,
#	such as 'h' when you're already at the left edge of a line.  But
#	attempted "motions" like 'inewstring' will result in an error.
#
#	Example:
#
#	# The following code deletes the previous 2 words and then
#	# positions the cursor at the next occurrence of the word
#	# "foo".
#
#	my $cb = $Vile::current_buffer;
#	$cb->setregion($cb->motion("2b"))->delete;
#				# delete the previous two words
#
#	$cb->setregion("2b")->delete;
#				# another way to delete the previous
#				# two words
#
#	$cb->motion("/foo/");	# position DOT at the beginning of
#				# "foo".
#
#	$cb->dot($cb->dot);	# Make sure DOT gets propogated back.
#				# (It won't get propogated unless
#				# explicitly set.)
#
 
void
motion(vbp,mstr)
    VileBuf *vbp
    char *mstr

    PREINIT:
	I32 gimme;
	struct MARK old_DOT;
	int status;

    PPCODE:
	old_DOT = DOT;
    	status = api_motion(vbp, mstr);

	gimme = GIMME_V; 
	if (!status) {
	    XPUSHs(&sv_undef);		/* return undef */
	}
	else if (gimme == G_SCALAR) {
	    XPUSHs(ST(0));		/* return the buffer object */
	}
	else if (gimme == G_ARRAY) {
	    I32 sl, el, so, eo;
	    sl = line_no(curbp, old_DOT.l);
	    so = old_DOT.o;
	    el = line_no(curbp, DOT.l);
	    eo = DOT.o;
	    if (sl > el) {
		I32 tl = sl;
		sl = el;
		el = tl;
	    }
	    if (sl == el && so > eo) {
		I32 to = so;
		so = eo;
		eo = to;
	    }
	    XPUSHs(sv_2mortal(newSViv(sl)));
	    XPUSHs(sv_2mortal(newSViv(so)));
	    XPUSHs(sv_2mortal(newSViv(el)));
	    XPUSHs(sv_2mortal(newSViv(eo)));
	}
 
#
# attribute_cntl_a_sequences BUFOBJ 
# 
#	Causes the editor to attach attributes to the <Ctrl>A 
#	sequences found in the buffer for the current region (which 
#	may be set via setregion). 
# 
#	Returns the buffer object. 
#
 
VileBuf *
attribute_cntl_a_sequences(vbp) 
    VileBuf *vbp
 
    CODE:
	api_setup_fake_win(vbp, TRUE);
	attribute_cntl_a_sequences_over_region(&vbp->region, vbp->regionshape); 
	RETVAL = vbp; 
 
    OUTPUT:
	RETVAL

#
# attribute BUFOBJ LIST
#
#	Attach an attributed region to the region associated with BUFOBJ
#	with the attributes found in LIST.
#
#	These attributes may be any of the following:
#
#		'color' => NUM		(where NUM is the color number
#					 from 0 to 15)
#		'underline'
#		'bold'
#		'reverse'
#		'italic'
#		'hyper' => HYPERCMD	(where HYPERCMD is a string
#					representing a vile command to
#					execute.  It may also be a
#					(perl) subroutine reference.
#		'normal'
#
#	Normal is a special case.  It will override any other arguments
#	passed in and remove all attributes associated with the region.
#		

void
attribute(vbp, ...)
    VileBuf *vbp

    PPCODE:
	if (items <= 1) {
	    /* Hmm.  What does this mean?  Should we attempt to fetch
	       the attributes for this region?  Should we turn off all
	       the attributes?

	       Personally, I think it'd be cool to return a list of
	       all the regions and their attributes.  But I'll save
	       that exercise for another night...
	    */
	}
	else {
	    int i;
	    char *atname;
	    VIDEO_ATTR vattr = 0;
	    int normal = 0;
	    char *hypercmd = 0;
	    int status;

	    for (i = 1; i < items; i++) {
		atname = SvPV(ST(i), na);
		if (       strcmp(atname, "underline") == 0) {
		    vattr |= VAUL;
		} else if (strcmp(atname, "bold"     ) == 0) {
		    vattr |= VABOLD;
		} else if (strcmp(atname, "reverse"  ) == 0) {
		    vattr |= VAREV;
		} else if (strcmp(atname, "italic"   ) == 0) {
		    vattr |= VAITAL;
		} else if (strcmp(atname, "normal"   ) == 0) {
		    normal = 1;
		} else if (strcmp(atname, "color"    ) == 0) {
		    i++;
		    if (i < items) {
			vattr |= VCOLORATTR(SvIV(ST(i)) & 0xf);
		    }
		    else {
			croak("Color attribute not supplied");
		    }
		} else if (strcmp(atname, "hyper"    ) == 0
		        || strcmp(atname, "hypertext") == 0) {
		    i++;
		    if (i < items) {
			if (SvROK(ST(i)) 
			    && SvTYPE(SvRV(ST(i))) == SVt_PVCV)
			{
			    /* We have a code ref */
			    hypercmd = stringify_coderef(ST(i));
			}
			else {
			    /* It's just a string */
			    hypercmd = strdup(SvPV(ST(i),na));
			}
		    }
		    else {
			croak("Hypertext command not supplied");
		    }
		} else {
		    croak("Invalid attribute");
		}
	    }

	    if (normal) {
		vattr = 0;
		hypercmd = 0;
	    }

	    status = attributeregion_over_region(
	    		&vbp->region, vbp->regionshape, vattr, hypercmd);

	    if (status == TRUE)		/* not the same as "if (status)" */
		XPUSHs(ST(0));		/* return buffer object */
	    else
		XPUSHs(&sv_undef);	/* else return undef */
	}

 
#
# unmark 
# 
#	Clear the "modified" status of the buffer. 
# 
#	Returns the buffer object. 
#
 
VileBuf *
unmark(vbp) 
    VileBuf *vbp
 
    CODE:
	api_setup_fake_win(vbp, TRUE);
	unmark(0,0); 
	RETVAL = vbp; 
 
    OUTPUT:
	RETVAL 

#
# command BUFOBJ CMDLINE 
# 
#	executes the given vile command line (as if it were typed
#	on the : line) with BUFOBJ as the current buffer. 
#
#	Returns BUFOBJ if successful, otherwise returns undef.
# 
 
void
command(vbp,cline)  
    VileBuf *vbp
    char *cline
 
    PREINIT:
	int status;
    PPCODE:
	api_setup_fake_win(vbp, TRUE);
	status = docmd(cline, TRUE, FALSE, 1);  
	if (status) {
	    XPUSHs(ST(0));		/* return buffer object */
	}
	else {
	    XPUSHs(&sv_undef);		/* return undef */
	}

#
# buffername BUFOBJ
#
#	Returns the buffer name associated with BUFOBJ.
#
# buffername BUFOBJ BUFNAME
#
#	Sets the buffer name associated with BUFOBJ to the string
#	given by BUFNAME.  This string must be unique.  If the name
#	given is already being used by another buffer, or if it's
#	malformed in some way, undef will be returned.  Otherwise
#	the name of the buffer will be returned.
#
#	Note: The name of the buffer returned may be different than
#	that passed in due some adjustments that may be done on the
#	buffer name.  (It will be trimmed of spaces and a length limit
#	is imposed.)
#
# filename BUFOBJ
#
#	Returns the file name associated with BUFOBJ.
#
# filename BUFOBJ FILENAME
#
#	Sets the name of the file associated with BUFOBJ to the string
#	given by FILENAME.
#

void
buffername(vbp,...)
    VileBuf *vbp

    ALIAS:
	filename = 1

    PREINIT:
	int status;

    PPCODE:

	status = TRUE;
	api_setup_fake_win(vbp, TRUE);

	if (items > 2)
	    croak("Too many arguments to %s", 
	          ix == 0 ? "buffername" : "filename");
	else if (items == 2) {
	    if (ix == 0)
		status = renamebuffer(curbp, SvPV(ST(1),na));
	    else
		ch_fname(curbp, SvPV(ST(1),na));
	}

	if (status == TRUE) {
	    XPUSHs(sv_2mortal(newSVpv(ix == 0 ? 
	                              curbp->b_bname : curbp->b_fname, 0)));
	}
	else {
	    XPUSHs(&sv_undef);		/* return undef */
	}

  

MODULE = Vile	PACKAGE = Vile

void
Warn(warning)
    char *warning

    CODE:
	mlforce("%s",SvPV(GvSV(errgv), na));
	/* don't know if this actually works... */
	sv_catpv(GvSV(errgv),warning);

 
 
#
# mlreply PROMPT 
# mlreply PROMPT, INITIALVALUE 
# 
#	Prompts the user with the given prompt and (optional) supplied 
#	initial value.  Certain characters that the user may input have 
#	special meanings to mlreply and may have to be escaped by the 
#	user to be input.  If this is unacceptable, use mlreply_no_opts 
#	instead. 
# 
#	Returns the user's response string.  If the user aborts 
#	(via the use of the escape key) the query, an undef is 
#	returned. 
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
 
	RETVAL = (status == TRUE || status == FALSE) 
	         ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
    OUTPUT:
	RETVAL
 
#
# mlreply_no_opts PROMPT 
# mlreply_no_opts PROMPT, INITIALVALUE 
# 
#	Prompts the user with the given prompt and (optional) supplied 
#	initial value.  All printable characters may be entered by the 
#	without any special escapes. 
# 
#	Returns the user's response string.  If the user aborts 
#	(via the use of the escape key) the query, an undef is 
#	returned. 
#
 
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
 
	RETVAL = (status == TRUE || status == FALSE) 
	         ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
    OUTPUT:
	RETVAL
 
#
# mlreply_file PROMPT 
# mlreply_file PROMPT, INITIALVALUE 
# 
#	Prompts the user for a filename with the given prompt and 
#	(optional) initial value.  Filename completion (via the 
#	TAB key, if enabled) may be used to assist in entering 
#	the filename. 
# 
#	Returns the user's response string.  If the user aborts 
#	(via the use of the escape key) the query, an undef is 
#	returned. 
#
 
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
	 
	buf[0] = EOS;
	status = mlreply_file(prompt, &last, FILEC_UNKNOWN, buf); 
 
	RETVAL = (status == TRUE || status == FALSE)
	         ? newSVpv(buf, 0) : newSVsv(&sv_undef); 
 
    OUTPUT:
	RETVAL 
 
#
# mlreply_dir PROMPT 
# mlreply_dir PROMPT, INITIALVALUE 
# 
#	Prompts the user for a directory name with the given prompt 
#	and (optional) initial value.  Filename completion (via the 
#	TAB key, if enabled) may be used to assist in entering 
#	the directory name. 
# 
#	Returns the user's response string.  If the user aborts 
#	(via the use of the escape key) the query, an undef is 
#	returned. 
#
 
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
	 
	buf[0] = EOS;
	status = mlreply_dir(prompt, &last, buf);
 
	RETVAL = (status == TRUE || status == FALSE)
	         ? newSVpv(buf, 0) : newSVsv(&sv_undef);
 
    OUTPUT:
	RETVAL


#
# command CMDLINE 
# 
# 	executes the given vile command line (as if it were typed
#	on the : line). 
# 
# 	This is not exactly safe in all contexts.  (It is easy to
#	cause seg faults.)
#
 
int
command(cline)
    char *cline
 
    CODE:
	RETVAL = docmd(cline, TRUE, FALSE, 1);

    OUTPUT:
	RETVAL

#
# keystroke
# keystroke WAITVAL
#
#	Returns a single, fairly raw keystroke from the keyboard.
#
#	The optional WAITVAL indicates if the editor should wait
#	for the next keystroke.  When WAITVAL is false, undef will
#	be returned if no input is ready.
#

void
keystroke(...)

    PPCODE:
	if (items > 1)
	    croak("Too many arguments to keystroke");
	else if (items == 1 && !SvTRUE(ST(0))) {
	    if (!sysmapped_c_avail()) {
		XPUSHs(&sv_undef);
	    }
	}
	XPUSHs(sv_2mortal(newSViv(sysmapped_c())));


#
# working
#
# working VAL
#
#	Returns value 1 if working message will be printed during
#	substantial pauses, 0 if disabled.
#
#	When passed an argument, modifies value of the working message.
#

int
working(...)

    CODE:
	if (items > 1)
	    croak("Too many arguments to working");
	else if (items == 1) {
	    no_working = !SvIV(ST(0));
	}

	RETVAL = !no_working;
    OUTPUT:
	RETVAL

#
# update
#
#	Update the editor's display.  It is usually not necessary to
#	call this if you're returning to the editor in fairly short
#	order.  It will be necessary to call this, for example, if
#	you write an input loop in perl which writes things to some
#	on-screen buffers, but does not return to the editor immediately.
#

void
update()
    PPCODE:
	api_update();

#
# watchfd FD, WATCHTYPE, CALLBACK 
#
#	Adds a callback so that when the file descriptor FD is available
#	for a particular type of I/O activity (specified by WATCHTYPE),
#	the callback associated with CALLBACK is called.
#
#	WATCHTYPE must be one of 'read', 'write', or 'except' and have
#	the obvious meanings.
#
#	The callback should either be a string representing a vile
#	command to execute (good luck) or (more usefully) a Perl subroutine
#	reference.
#

void
watchfd(fd, watchtypestr, ...)
    int fd
    char *watchtypestr

    PREINIT:
	char *cmd;
	int   watchtype;

    
    PPCODE:
	if (items != 3)
	    croak("Wrong number of arguments to watchfd");

	if (strcmp(watchtypestr, "read") == 0)
	    watchtype = WATCHREAD;
	else if (strcmp(watchtypestr, "write") == 0)
	    watchtype = WATCHWRITE;
	else if (strcmp(watchtypestr, "except") == 0)
	    watchtype = WATCHEXCEPT;
	else
	    croak("Second argument to watchfd must be one of \"read\", \"write\", or \"except\".");

	if (SvROK(ST(2)) 
	    && SvTYPE(SvRV(ST(2))) == SVt_PVCV)
	{
	    /* We have a code ref (cool) */
	    cmd = stringify_coderef(ST(2));
	}
	else {
	    /* It's just a string (how boring) */
	    cmd = strdup(SvPV(ST(2),na));
	}
	watchfd(fd, watchtype, cmd);

#
# unwatchfd FD
#
#	Removes the callback associated with FD and frees up the
#	associated data structures.
#

void
unwatchfd(fd)
    int fd

    PPCODE:
	unwatchfd(fd);
