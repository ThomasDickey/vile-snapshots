/*
 * perl.xs		-- perl interface for vile.
 *
 * Author: Kevin Buettner, Brendan O'Dea
 *
 * (with acknowledgments to the authors of the nvi perl interface and
 * to Sean Ahern who has contributed snippets of code here and there
 * and many valuable suggestions.)
 *
 * Created: Fall, 1997
 *
 * Description: The following code provides an interface to Perl from
 * vile.  The file api.c (sometimes) provides a middle layer between
 * this interface and the rest of vile.
 *
 * $Id: perl.xs,v 1.148 2025/01/26 16:43:53 tom Exp $
 */

/*
 * Perl's header (and macro expansions) produce too many warnings to be useful.
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wcompound-token-split-by-macro"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wnested-externs"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif

/*#
  #
  # Note: This embedded documentation may be retrieved for formatting
  # with one of the pod transformers as follows.  To make the
  # pod file, do
  #
  #     perl -lne 'print if s/^\s{1,2}#\s{0,1}//' perl.xs
  #
  # To transform the pod file into something nicely formatted, do
  # one or more of the following:
  #
  #     pod2text vile-perl-api.pod >vile-perl-api.doc
  #
  #     pod2html vile-perl-api.pod >vile-perl-api.html
  #
  #     pod2man  vile-perl-api.pod >vile-perl-api.man
  #
  #     pod2latex vile-perl-api.pod >vile-perl-api.latex
  #
  # I experimented with different formatting layouts.  I found that I
  # was unable to dispense with the initial transformation because
  # xsubpp doesn't like it when some of my pod documentation started
  # in the left-most column.  I've also found that placing the # in
  # the left-most column will sometimes screw up xsubpp wrt to
  # preprocessor statements.  It does not get confused when when there
  # are one or more spaces preceding the pound sign.  I usually like
  # to indent things by four spaces, and yet I wanted to use ^A-f in
  # vile to reformat things, so I settled on two spaces, followed by
  # a pound sign, followed by a single space, followed by whatever
  # as the most pleasing layout.
  #
  #
  # =pod
  #
  # =head1 NAME
  #
  # vile-perl-api       -- Vile/Perl interface reference
  #
  # =head1 DESCRIPTION
  #
  # This document describes the interface by which by Perl scripts may
  # access the I<vile> editor's internals when run from an editor in which
  # Perl has been embedded.
  #
  # There are presently two packages which comprise this interface.  They
  # are:
  #
  # =over 4
  #
  # =item Vile
  #
  # Subroutines for accessing and controlling vile in general.
  #
  # =for html <br><br>
  #
  # =item Vile::Buffer
  #
  # Subroutines and methods for accessing individual buffers.
  #
  # =for html <br><br>
  #
  # =item Vile::Window
  #
  # Subroutines and methods for manipulating Vile's windows.
  #
  # =back
  #
  # =head2 Calling Perl from Vile
  #
  # The perl interpreter may be invoked from I<vile> using either
  # the I<perl> or I<perldo> commands.
  #
  # =over 4
  #
 */

#ifdef WIN32
#include "w32vile.h"
#undef WIN32_LEAN_AND_MEAN
#endif

#if !(defined(__GNUC__) || defined(__attribute__))
#define __attribute__(p) /*nothing*/
#endif

/* for vile */
#define MARK vile_MARK
#define regexp vile_regexp
#include "estruct.h"
#include "edef.h"
#include "api.h"
#undef regexp
#undef MARK
#undef ABORT

/* for perl */
#define main perl_main
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#undef main
#undef dofile

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/* Some earlier versions of perl don't have GIMME_V or G_VOID. We must
   be careful of the order in which we check things if these defines
   are activated. */
#ifndef GIMME_V
#define GIMME_V GIMME
#endif

#ifndef G_VOID
#define G_VOID G_SCALAR
#endif

#ifndef INT2PTR
#define INT2PTR(n) (n)
#endif

/* Prior to perl5.005, the PL_ prefix wasn't used for things such
   as PL_rs.  Define the PL_ macros that we use if necessary. */

#include <patchlevel.h>		/* This is perl's patchlevel.h */

#ifndef pTHX
#define pTHX			/* nothing */
#endif

#ifndef pTHX_
#define pTHX_			/* nothing */
#endif

#ifndef PERL_MAGIC_tiedscalar
#define	PERL_MAGIC_tiedscalar	'q'
#endif

#ifndef PERL_MAGIC_ext
#define	PERL_MAGIC_ext		'~'
#endif

#if !defined(__PATCHLEVEL_H_INCLUDED__) && !defined(SUBVERSION)
#error The patchlevel.h file is probably not from Perl
#endif

#ifndef PERL_VERSION
#ifdef  PATCHLEVEL
#define PERL_VERSION PATCHLEVEL
#else
#error The patchlevel.h file does not define PATCHLEVEL
#endif
#endif

#if PERL_VERSION < 5		/* before perl 5.5 */
#define PL_incgv	incgv
#define PL_rs		rs
#define PL_errgv	errgv
#define PL_na		na
#define PL_sv_undef	sv_undef
#endif

#define PDEBUG 0		/* For debugging reference counts */

static PerlInterpreter *perl_interp;
static int use_ml_as_prompt;
static SV *svcurbuf;		/* $Vile::current_buffer in perl */
static int svcurbuf_set(pTHX_ SV *, MAGIC *);
static WINDOW * set_curwp0(WINDOW *wp);
static MGVTBL svcurbuf_accessors = {
	/* Virtual table for svcurbuf magic. */
	NULL, svcurbuf_set, NULL, NULL, NULL
#if PERL_VERSION >= 8
	, NULL, NULL
#endif
#if PERL_VERSION >= 10
	, NULL
#endif
};

static SV *ofs_sv;
static SV *ors_sv;

#ifdef PERL_SYS_INIT3
#define USE_SYS_INIT3 1
#endif

static int real_perl_init(void);
static void xs_init(pTHX);
static int  perl_prompt(void);
static int  perldo_prompt(void);

/* tie a filehandle */
static void
tie_handle(SV *sv, SV *obj)
{
    SV *tie = sv;
#if PERL_VERSION >= 8
    if (!GvIOp(sv))
	GvIOp(sv) = newIO();

    tie = (SV *) GvIOp(sv);
#endif

    sv_magic(tie, obj, PERL_MAGIC_tiedscalar, Nullch, 0);
}

/* write each line to message line */
static int
write_message(const char *prefix, SV *sv)
{
    int count = 0;
    char *text = SvPV(sv, PL_na);
    char *nl;

    while (text)
    {
	if ((nl = strchr(text, '\n')) != NULL)
	{
	    *nl = 0;
	    mktrimmed(text);
	    while (*++nl == '\n')
		;

	    if (!*nl)
		nl = NULL;
	}

	if (*text)
	    mlforce("%s%s", prefix, text);
	else
	    mlerase();
	text = nl;
	count++;
    }

    return count;
}

/* require a file, `optional' indicates that it is OK for the file not
   to exist */
static int
require(const char *file, int optional)
{
    TRACE((T_CALLED "Vile::require(%s,%s)\n", file, optional ? "optional" : "required"));
    /* require the file */
    perl_require_pv(file);

    /* OK */
    if (!SvTRUE(GvSV(PL_errgv)))
	returnCode(TRUE);

    if (optional)
    {
	/* this error is OK for optional files */
	SV *tmp = newSVpv("Can't locate ", (size_t) 0);
	char const *my_check;
	STRLEN sz;
	int not_found;

	sv_catpv(tmp, file);
	sv_catpv(tmp, " ");
	my_check = SvPV(tmp, sz);
	not_found = !strncmp(SvPV(GvSV(PL_errgv), PL_na), my_check, sz);
	SvREFCNT_dec(tmp);

	if (not_found)
	    returnCode(SORTOFTRUE);
    }

    write_message("perl require:", GvSV(PL_errgv));
    returnCode(FALSE);
}

/* When no region is specified, this will cause the entire buffer to
   be selected without moving DOT. */
void
perl_default_region(void)
{
    static REGION region;
    vile_MARK save_DOT = DOT;

    DOT.l = lforw(buf_head(curbp));
    DOT.o = b_left_margin(curbp);

    MK.l  = lback(buf_head(curbp));
    MK.o  = b_left_margin(curbp);

    regionshape = rgn_FULLLINE;
    haveregion = NULL;
    if (getregion(curbp, &region)) {
	haveregion = &region;
	/* This should really go in getregion(), but other parts of
	   vile break when we do this. */
	if (is_header_line(region.r_end, curbp) && !b_val(curbp, MDNEWLINE))
	    region.r_size -= (B_COUNT) len_record_sep(curbp);
    }
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
    if (sp->perl_handle == NULL) {
	sp->perl_handle = newGVgen("Vile::Buffer");
	GvSV((GV*)sp->perl_handle) = newSV((size_t) 0);
	sv_setiv(GvSV((GV*)sp->perl_handle), PTR2IV(sp));
	tie_handle(sp->perl_handle, rv);
	gv_IOadd((GV*)sp->perl_handle);
	IoLINES(GvIO((GV*)sp->perl_handle)) = 0;	/* initialise $. */
    }

    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = sp->perl_handle;
    SvREFCNT_inc(SvRV(rv));
    SvROK_on(rv);
    rv = sv_bless(rv, gv_stashpv("Vile::Buffer", TRUE));
#if PDEBUG
    fprintf(stderr, "newVBrv: ");
    sv_dump(rv);
#endif
    return rv;
}

static VileBuf *
getVB(SV *sv, const char **croakmessage_ptr)
{
    VileBuf *vbp = NULL;
    if (sv_isa(sv, "Vile::Buffer")) {
	vbp = INT2PTR(VileBuf *, SvIV((SV*)GvSV((GV*)SvRV(sv))));
	if (vbp == NULL) {
	    *croakmessage_ptr = "buffer no longer exists";
	}
    }
    else {
	*croakmessage_ptr = "buffer of wrong type";
    }
    return vbp;
}

void
perl_free_handle(void *handle)
{
    /* Remove the magic from the handle.  This should break the
       circular structure which would otherwise prevent the handle
       from getting freed. */
    sv_unmagic(handle, PERL_MAGIC_tiedscalar);
#if PDEBUG
    fprintf(stderr, "In perl_free_handle: ");
    sv_dump((SV*)handle);
#endif
    /*
     * Zero out perl's handle to the VileBuf structure
     */
    sv_setiv(GvSV((GV*)handle), (size_t) 0);

    /*
     * Decrement the reference count to indicate the fact that
     * we are no longer referencing it from the api private structure.
     * If there aren't any other references from within perl either,
     * then this scalar will be collected.
     */
    SvREFCNT_dec(handle);
}

/*
 * Create a VileWin object.  This is a good deal simpler than a VileBuf
 * since we are merely taking a reference to an integer (the windows
 * id) and blessing it.
 */
static SV *
newVWrv(SV *rv, VileWin vw)
{
    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = newSViv((IV)win2id(vw));
    SvROK_on(rv);
    rv = sv_bless(rv, gv_stashpv("Vile::Window", TRUE));
#if PDEBUG
    fprintf(stderr, "newVWrv: ");
    sv_dump(rv);
#endif
    return rv;
}

static VileWin
getVW(SV *sv, const char **croakmessage_ptr)
{
    VileWin vw = NULL;
    if (sv_isa(sv, "Vile::Window")) {
	vw = id2win((WIN_ID)SvIV((SV*)SvRV(sv)));
	if (!vw) {
	    *croakmessage_ptr = "window no longer exists";
	}
    }
    else {
	*croakmessage_ptr = "window of wrong type";
    }
    return vw;
}

static int recursecount = 0;

static int
do_perl_cmd(SV *cmd, int inplace)
{
    int old_isnamedcmd;
    REGION region;
    VileBuf *curvbp;

    TRACE((T_CALLED "do_perl_cmd\n"));

    use_ml_as_prompt = 0;

    if (recursecount == 0) {
	curvbp = api_bp2vbp(curbp);
	if (curvbp == NULL)
	    returnCode(FALSE);

	if (DOT.l == NULL || MK.l == NULL || getregion(curbp, &region) != TRUE) {
	    /* shouldn't ever get here. But just in case... */
	    perl_default_region();
	    if (getregion(curbp, &region) != TRUE) {
		mlforce("BUG: getregion won't return TRUE in perl.xs.");
	    }
	}
	if (is_header_line(region.r_end, curbp) && !b_val(curbp, MDNEWLINE))
	    region.r_size -= (B_COUNT) len_record_sep(curbp);

	/* Initialize some of the fields in curvbp */
	curvbp->region = region;
	curvbp->regionshape = regionshape;
	curvbp->inplace_edit = inplace;

	{
	    SV *sv = newVBrv(newSV((size_t) 0), curvbp);
	    sv_setsv(svcurbuf, sv);
	    SvREFCNT_dec(sv);
	}
	IoLINES(GvIO((GV*)curvbp->perl_handle)) = 0;  /* initialise $. */
    }

    /* Make sure that mlreply_dir and mlreply_file will actually prompt
       the user.  It is necessary to do this because isnamedcmd was not
       getting set when invoked through a binding. */
    old_isnamedcmd = isnamedcmd;
    isnamedcmd = TRUE;

    recursecount++;

#if PDEBUG
    fprintf(stderr, "do_perl_command: before eval:\n");
    sv_dump(svcurbuf);
#endif
    sv_setpv(GvSV(PL_errgv),"");
    if (SvROK(cmd) && SvTYPE(SvRV(cmd)) == SVt_PVCV)
    {
	dSP;
	PUSHMARK(sp);
	PUTBACK;
	perl_call_sv(cmd, G_EVAL|G_VOID|G_DISCARD);
    }
    else
	perl_eval_sv(cmd, G_DISCARD|G_NOARGS|G_KEEPERR);
#if PDEBUG
    fprintf(stderr, "do_perl_command: after eval: \n");
    sv_dump(svcurbuf);
#endif

    isnamedcmd = old_isnamedcmd;
    recursecount--;
    if (recursecount == 0) {
	sv_setsv(svcurbuf, &PL_sv_undef);
	api_command_cleanup();
    }
    else {
	/* We don't do the hardcore cleanup if we're recursing, but
	   we at least need to make sure that curwp points at a
	   visible window */
	if (curwp_visible)
	    set_curwp0(curwp_visible);
    }
    if (!is_visible_window(curwp))
	mlforce("BUG: curwp not set to a visible window");

    if (SvTRUE(GvSV(PL_errgv)) == 0)
	returnCode(TRUE);

    write_message("perl cmd:", GvSV(PL_errgv));
    returnCode(FALSE);
}

/*
 * procedures for bindable callbacks: see Vile::register*
 */

static SV *opsv;

static int
perl_oper(void)
{
    TRACE((T_CALLED "perl_oper\n"));
    returnCode(do_perl_cmd(opsv, FALSE));
}

int
perl_call_sub(void *data, int oper, int f, int n)
{
    AV *av = data;	/* callback is an array containing: */
    SV **name = NULL;	/* the registered name, */
    SV **sub = NULL;	/* a sub name or coderef to call, */
    SV **req = NULL;	/* and an [optional] file to require */

    TRACE((T_CALLED "perl_call_sub\n"));
    switch (av_len(av))
    {
	case 2: /* (name, sub, require) */
	    if ((req = av_fetch(av, 2, 0)) != NULL && SvTRUE(*req))
		if (!require(SvPV(*req, PL_na), FALSE))
		    returnCode(FALSE);

	    /* FALLTHRU */

	case 1: /* (name, sub) */
	    if ((name = av_fetch(av, 0, 0)) == NULL || !SvTRUE(*name))
		croak("BUG: can't fetch name SV");

	    if ((sub = av_fetch(av, 1, 0)) == NULL || !SvTRUE(*sub))
		croak("BUG: can't fetch subroutine SV");

	    break;

	default:
	    croak("BUG: array contains %d elements", (int) av_len(av) + 1);
    }

    /* call the subroutine */
    if (oper)
    {
	opcmd = OPOTHER;
	opsv = *sub;
	f = vile_op(f, n, perl_oper, SvPV(*name, PL_na));
    }
    else
    {
	if (!f)
	    n = 1;

	while (n-- && (f = do_perl_cmd(*sub, FALSE)) != 0)
	    ;
    }

    returnCode(f);
}

void
perl_free_sub(void *data)
{
    AV *av = data;
    av_undef(av);
}

/*
 * Prompt for and execute a perl command.
 *
 * This function is actually only a wrapper for perl_prompt below to make
 * the history management easier.
 *
  #
  # =item :perl STMTS
  #
  # The I<perl> command will cause perl to execute one or more
  # perl statements.  The user is usually prompted for the statements
  # to execute immediately after ":perl " is entered.  The user is
  # expected to enter legal perl statements or expressions.  These
  # statements must all fit on one line.  (Vile's :-line will scroll
  # horizontally though, so don't worry about running out of space.)
  #
  # The perl command may also appear in macros in vile's internal
  # macro language, in which case the perl statements to execute must
  # appear as a double quoted string to the perl command.  The user
  # is not prompted in this case.
  #
  # Regardless, prior to execution, the global variable,
  # C<$Vile::>C<current_buffer> is set to an object of type C<Vile::Buffer>
  # which represents the current buffer.  The statements to be executed
  # may choose to act either directly or indirectly on the current
  # buffer via this variable or a suitable alias.
  #
  # Normally, the cursor's current position, also referred to as I<dot>,
  # is left unchanged upon return from perl.  It can be propagated
  # back to a viewable window by explicitly setting via the
  # C<Vile::Buffer::>C<dot> method.
  #
  # For purposes of reading from the buffer, there is always a region
  # associated with the buffer object.  By default, this region is the
  # entire buffer.  (Which means that potentially, the entire buffer
  # may be acted upon.) This range may be restricted by the user in
  # the normal way through the use of a range specification which
  # precedes the perl command.   E.g,
  #
  #     30,40perl @l = <$Vile::current_buffer>
  #
  # will cause lines 30 thru 40 to be placed into the @l array.
  #
  # =for html <br><br>
  #
 */

int
perl(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status = FALSE;

#if OPT_HISTORY
    if (recursecount == 0)
	hst_init(EOS);
#endif

    if (perl_interp || real_perl_init())
	status = perl_prompt();

#if OPT_HISTORY
    if (recursecount == 0)
	hst_flush();
#endif

    return status;
}

static int
perl_prompt(void)
{
    register int status;
    char buf[NLINE];	/* buffer to receive command into */
    SV *cmd;

    TRACE((T_CALLED "perl_prompt\n"));

    buf[0] = EOS;
    if ((status = mlreply_no_opts("Perl command: ", buf, (UINT) sizeof(buf))) != TRUE)
	returnCode(status);

    /* Hack to workaround problem with perl5.005 in which package Dynaloader
       sometimes winds up being the current package */
#if 0
    /* This is what the code ought to look like... */
    cmd = newSVpv(buf, 0);
#else
    /* This is the hack... */
    cmd = newSVpv("package main; ", (size_t) 0);
    sv_catpv(cmd, buf);
#endif
    status = do_perl_cmd(cmd, FALSE);
    SvREFCNT_dec(cmd);
    returnCode(status);
}

#define isoctal(c) ((c) >= '0' && (c) <= '7')
static int octal(char **s)
{
    int oct = 0;
    int i = (**s > '3') ? 2 : 3;

    while (i-- && isoctal(**s))
    {
	oct *= 8;
	oct += *((*s)++) - '0';
    }

    return oct;
}

/*#
  #
  # =item :perldo STMTS <Enter> OPTIONS
  #
  # The I<perldo> command is like the perl command, but it takes
  # various options making it possible to write "one liners" to
  # operate on the current buffer in much the same way that you might
  # write a one line perl command at the prompt of your favorite shell
  # to operate on a file.  The options even mimic those provided by
  # the perl interpreter, so if you are familiar with one, you'll be
  # familiar with the other.
  #
  # After entering the perldo command (preceded by an optional range
  # specification) on the :-line, the user will be prompted for some
  # perl statements to execute.  These should usually be written to
  # operate on the $_ variable and leave the result in $_.
  #
  # After pressing the B<Enter> key, you'll be prompted for a set
  # of options.  The default options are -lpi and will even be displayed
  # as such.  The B<-i> switch causes the buffer to be edited in place.
  # The B<-p> switch causes the user supplied statements to be placed
  # in a loop which fetches lines one by one place them in $_ for each
  # iteration of the loop along with a trailing C<print> which'll cause
  # whatever's left in $_ to be put back into the buffer.  The B<-l> switch
  # causes an initial chomp to be done on each line after it is read.
  # It will also cause the output record separator to be set so that
  # when $_ is written back to the buffer, it will end up on a line of
  # its own.
  #
  # For example, the command:
  #
  #     :25,30perldo $_ = sprintf("%4d",$lnum++) . $_
  #                  -lpi
  #
  # will cause each line in between 20 and 30 inclusive to be prefixed
  # with a the number given by $lnum, which is also incremented for
  # each line processed.  You'll probably want to initialize $lnum to
  # some appropriate value via the I<perl> command first, perhaps
  # like this:
  #
  #     :perl $lnum = 142;
  #
  # [I include this example, because this is something that I've
  # wanted to do from time to time, when citing snippets of code
  # which I want to discuss in an email message.]
  #
  # =item perldo options
  #
  # =for html <br><br>
  #
  # =over 4
  #
  # =item -n
  #
  # Enclose the perl statement(s) in a loop which iterates of the records
  # (usually lines) of the region.  Each record in the region will
  # be placed in $_.
  #
  # =for html <br><br>
  #
  # =item -p
  #
  # Like B<-n>, but do a print (of $_) at the end of the loop.
  #
  # =for html <br><br>
  #
  # =item -i
  #
  # Enable the I<inplace_edit> flag for the buffer.  When used with
  # either B<-n> or B<-p>, this will cause the lines to be deleted from the
  # buffer as they are read.
  #
  # Unlike the corresponding perl command line switch, it is not possible
  # to specify a backup file.  If you don't like what happens, just hit
  # the 'B<u>' key to undo it.
  #
  # =for html <br><br>
  #
  # =item -l
  #
  # Only meaningful when used with either B<-n> or B<-p>.  This will
  # perform an initial chomp on $_ after a record has been read.
  #
  # =for html <br><br>
  #
  # =item -0
  #
  # This must be followed by one or more digits which represent the
  # value with which to set $/ (which is the input record separator).
  # The special value B<00> indicates that $/ should be set to the
  # empty string which will cause Perl to slurp input in paragraph
  # mode.  The special value 0777 indicates that perl should slurp
  # the entire region without paying attention to record separators.
  # Normally, $/ is set to '\n' which corresponds to -012
  #
  # =for html <br><br>
  #
  # =item -a
  #
  # Turn on autosplit mode.  Upon being read, each record is split
  # into the @F array.
  #
  # =for html <br><br>
  #
  # =item -F
  #
  # When used with B<-a>, specify an alternate pattern to split on.
  #
  # =for html <br><br>
  #
  # =back
  #
  # The default region for the perldo command is the line on which
  # the cursor is currently on.  The reason for this is that it is
  # often used like vile's builtin substitute operator is and this
  # is the default region for the substitute command.  You can of
  # course use any of the standard means to operate over larger
  # regions, e.g,
  #
  #     :1,$perldo s/a/b/g
  #
  #
 */

int
perldo(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int status = FALSE;

#if OPT_HISTORY
    hst_init(EOS);
#endif

    if (perl_interp || real_perl_init())
	status = perldo_prompt();

#if OPT_HISTORY
    hst_flush();
#endif

    return status;
}

static int
perldo_prompt(void)
{
    register int status;
    char buf[NLINE];	/* buffer to receive command into */
    char obuf[NLINE];	/* buffer for options */
    SV *cmd;		/* constructed command string */

#define	OPT_i	001
#define	OPT_n	002
#define	OPT_p	004
#define	OPT_l	010
#define	OPT_a	020
    int opts = 0;
    char *o = obuf;
    char *split = "' '";

#define	RS_PARA	0776
#define	RS_NONE	0777
    int i_rs = '\n';
    int o_rs = RS_NONE;

    TRACE((T_CALLED "perldo_prompt\n"));

    buf[0] = EOS;
    if ((status = mlreply_no_opts("Perl command: ", buf, (UINT) sizeof(buf))) != TRUE)
	returnCode(status);

#if OPT_HISTORY
    hst_glue('\r');
#endif

    strcpy(obuf, "-lpi");
    if ((status = mlreply_no_opts("options: ", obuf, (UINT) sizeof(obuf))) != TRUE)
	returnCode(status);

    /* skip optional leading `-' */
    if (*o == '-')
	o++;

    /* parse options */
    while (*o)
	switch (*o)
	{
	case 'a': opts |= OPT_a; o++; break;
	case 'i': opts |= OPT_i; o++; break;
	case 'n': opts &= ~OPT_p; opts |= OPT_n; o++; break;
	case 'p': opts &= ~OPT_n; opts |= OPT_p; o++; break;
	case 'l':
	    opts |= OPT_l;
	    o++;
	    if (isoctal(*o))
		o_rs = octal(&o);
	    else
		o_rs = i_rs;

	    break;

	case '0':
	    /* special cases: 00, 0777 */
	    if (*++o == '0' && !isoctal(*(o+1)))
	    {
		i_rs = RS_PARA;
		o++;
	    }
	    else if (!strncmp(o, "777", (size_t) 3))
	    {
		i_rs = RS_NONE;
		o += 3;
	    }
	    else
		i_rs = octal(&o);

	    break;

	case 'F':
	    opts |= OPT_a; /* implied */
	    if (*++o == '/' || *o == '"' || *o == '\'')
	    {
		char sep = *o;
		char esc = 0;

		split = o++;
		while (*o)
		{
		    if (*o == sep && !esc)
		    {
			o++;
			break;
		    }

		    if (*o++ == '\\')
			esc ^= 1;
		    else
			esc = 0;
		}

		if (*o && *o != ' ')
		{
		    mlforce("[no closing %c]", sep);
		    returnCode(FALSE);
		}
	    }
	    else if (*o)
	    {
		split = o++;
		while (*o && *o != ' ') o++;
	    }
	    else
	    {
		mlforce("[-F requires an argument]");
		returnCode(FALSE);
	    }

	    if (*o)
		*o++ = 0; /* terminate */
		/* FALLTHRU */
	    else
		break;

	case ' ':
	    while (*o == ' ') o++;
	    if (!*o)
		break; /* trailing spaces */

	    if (*o == '-' && *(o+1))
	    {
		o++;
		break;
	    }

	    /* FALLTHRU */

	default:
	    mlforce("[invalid option -%s]", o);
	    returnCode(FALSE);
	}

    /* construct command: block with localised $/ and $\ */
#if 0	/* See comment in perl_prompt() regarding this hack... */
    /* No hack */
    cmd = newSVpv("{local $/=", 0); /*}*/
#else
    /* Package name hack - works around a bug in perl5.00503 */
    cmd = newSVpv("package main; {local $/=", (size_t) 0); /*}*/
#endif
    if (i_rs == RS_NONE)
	sv_catpv(cmd, "undef");
    else if (i_rs == RS_PARA)
	sv_catpv(cmd, "''");
    else
	sv_catpvf(cmd, "\"\\x%02x\"", i_rs);

    sv_catpv(cmd, ";local $\\=");
    if (o_rs == RS_NONE)
	sv_catpv(cmd, "undef");
    else if (o_rs == RS_PARA)
	sv_catpv(cmd, "\"\\n\\n\"");
    else
	sv_catpvf(cmd, "\"\\x%02x\"", o_rs);

    /* set default output handle */
    sv_catpv(cmd, ";my $_save_fh=select ");
    if (opts & OPT_i)
	sv_catpv(cmd, "$Vile::current_buffer"); /* -i goes to buffer */
    else
	sv_catpv(cmd, "STDOUT"); /* mini */

    /* implicit loop for -n/-p */
    if (opts & (OPT_n|OPT_p))
    {
	sv_catpv(cmd, ";LINE:while(<$Vile::current_buffer>){"); /*}*/
	if (opts & OPT_l)
	    sv_catpv(cmd, "chomp;");

	/* autosplit to @F */
	if (opts & OPT_a)
	{
	    sv_catpv(cmd, "@F=split ");
	    if (*split == '/' || *split == '"' || *split == '\'')
		sv_catpv(cmd, split);
	    else
	    {
		char delim;
		const char *test = "'~#\200\1";
		/* try to find a delimiter not in the string */
		while (*test && vl_index(split, *test)) test++;
		delim = *test;
		sv_catpvf(cmd, "q%c%s%c", delim, split, delim);
	    }

	    sv_catpv(cmd, ";");
	}
    }
    else
	sv_catpv(cmd, ";");

    /* add the command */
    sv_catpv(cmd, buf);

    /* close the loop */
    if (opts & (OPT_n|OPT_p))
    {
/*{*/	sv_catpv(cmd, "}");
	if (opts & OPT_p)
	    sv_catpv(cmd, "continue{print}");
    }
    else
	sv_catpv(cmd, ";");

    /* reset handle and close block */
/*{*/ sv_catpv(cmd, "select $_save_fh}");

    status = do_perl_cmd(cmd, opts & OPT_i);
    SvREFCNT_dec(cmd);

    returnCode(status);
}

static int
svcurbuf_set(pTHX_ SV *sv, MAGIC *mg GCC_UNUSED)
{
    VileBuf *vbp;
    if (sv_isa(sv, "Vile::Buffer")
	&& (vbp = INT2PTR(VileBuf *, SvIV((SV*)GvSV((GV*)SvRV(sv))))) != NULL)
    {
	api_swscreen(NULL, vbp);
    }
    else {
	VileBuf *curvbp = api_bp2vbp(curbp);
	if (curvbp != NULL) {
	    SV *my_sv = newVBrv(newSV((size_t) 0), curvbp);
	    sv_setsv(svcurbuf, my_sv);
	    SvREFCNT_dec(my_sv);
	}
    }
    return 1;
}

static void
prepend_include(char *path)
{
    AV   *av;
    SV   *sv;

    if (is_directory(path)) {
	TRACE(("prepend_include(%s)\n", path));
	av_unshift(av = GvAVn(PL_incgv), 1);
	sv = newSVpv(path, (size_t) 0);
	av_store(av, 0, sv);
    }
}

static int
real_perl_init(void)
{
    static char empty[1];
    static char option_e[] = "-e";
    static char values_0[] = "0";
    static char my_class[] = "Vile";
    char *embedding[] = { empty, option_e, values_0 };
    char *bootargs[]  = { my_class, NULL };
    SV   *svminibuf;
    char  temp[NFILEN];
    char *vile_path;
    static char svcurbuf_name[] = "Vile::current_buffer";

    TRACE((T_CALLED "perl_init\n"));

    perl_interp = perl_alloc();
    perl_construct(perl_interp);

    if (perl_parse(perl_interp, xs_init, 3, embedding, NULL)) {
	perl_destruct(perl_interp);
	perl_free(perl_interp);
	perl_interp = NULL;
	returnCode(FALSE);
    }
    perl_call_argv("Vile::bootstrap", G_DISCARD, bootargs);

    /* Add our own paths to the front of @INC */
#ifdef HELP_LOC
    prepend_include(lengthen_path(strcpy(temp, "~/.vile/perl")));
    prepend_include(lengthen_path(pathcat(temp, HELP_LOC, "perl")));
#endif
    /* Always recognize environment variable */
    if ((vile_path = vile_getenv("VILE_LIBDIR_PATH")) != NULL)
    {
	const char *cp = vile_path;
	char result[NFILEN];
	int first = TRUE;
	/*
	 * "patch" @INC to look (first) for scripts in the directory
	 * %VILE_LIBDIR_PATH%\\perl .
	 */
	TRACE(("perl_init VILE_LIBDIR_PATH=%s\n", vile_path));
	while ((cp = parse_pathlist(cp, result, &first)) != NULL) {
	    TRACE(("parsed %s\n", cp));
	    prepend_include(pathcat(temp, result, "perl"));
	}
    }

    /* Obtain handles to specific perl variables, creating them
       if they do not exist. */
    svcurbuf  = perl_get_sv(svcurbuf_name,  TRUE);

    svminibuf   = newVBrv(newSV((size_t) 0), api_bp2vbp(bminip));

    /* Tie STDOUT and STDERR to miniscr->PRINT() function */
    tie_handle((SV *) gv_fetchpv("STDOUT", TRUE, SVt_PVIO), svminibuf);
    tie_handle((SV *) gv_fetchpv("STDERR", TRUE, SVt_PVIO), svminibuf);
    tie_handle((SV *) gv_fetchpv("STDIN", TRUE, SVt_PVIO), svminibuf);

    sv_magic(svcurbuf, NULL, PERL_MAGIC_ext, svcurbuf_name,
	     (I32) strlen(svcurbuf_name));

    mg_find(svcurbuf, PERL_MAGIC_ext)->mg_virtual = &svcurbuf_accessors;
    SvMAGICAL_on(svcurbuf);

    /* Some things are better (or easier) to do in perl... */
    perl_eval_pv("$SIG{__WARN__}='Vile::Warn';"
		 "sub Vile::Buffer::PRINTF {"
		 "    my $fh=shift; my $fmt=shift;"
		 "    print $fh sprintf($fmt,@_);"
		 "}", G_DISCARD);

    /* Fetch $\ and $, */
    ors_sv = perl_get_sv("\\", TRUE);
    ofs_sv = perl_get_sv(",", TRUE);

    /* Load user or system wide initialization script */
    require("vileinit.pl", TRUE);
    returnCode(TRUE);
}

/*
 * This is called from main.c in a context that precludes including Perl's
 * header files.
 */
void perl_init(int *argc, char ***argv, char ***envp)
{
#ifdef USE_SYS_INIT3
    PERL_SYS_INIT3(argc, argv, envp);
#endif
}

/* make sure END blocks and destructors get called */
void perl_exit()
{
    TRACE((T_CALLED "perl_exit\n"));
    if (perl_interp) {
	perl_run(perl_interp);		/* process END blocks */
	perl_destruct(perl_interp);	/* global destructors */
	perl_free(perl_interp);
	perl_interp = NULL;
#ifdef USE_SYS_INIT3
	PERL_SYS_TERM();
#endif
    }
    returnVoid();
}

/* Register any extra external extensions */

#ifdef __cplusplus
extern "C" {
#endif

extern void boot_DynaLoader(pTHX_ CV* cv);

#if (defined(__CYGWIN__) || defined(WIN32)) && defined(USE_DYNAMIC_LOADING)
__declspec(dllexport)
#else
extern
#endif

void boot_Vile(pTHX_ CV* cv);

#ifdef __cplusplus
}
#endif

static void
xs_init(pTHX)
{
    const char *file = __FILE__;
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
    newXS("Vile::bootstrap", boot_Vile, file);
}

/*
 * Stringify a code ref so it may be called from vile.
 */

static const char *CRfmtstr = "perl \"&{$Vile::CRs[%d]}\"";
static AV *CRarray   = NULL;
static int freeCRidx = 0;

static char *
stringify_coderef(SV *coderef) {
    char buf[40];
    int idx = 0;
    int cantstore = 0;

    if (CRarray == NULL) {
	/* Short name to keep the size of strings short on the vile side */
	CRarray = perl_get_av("Vile::CRs", TRUE);
	freeCRidx = -1;
    }

    if (freeCRidx >= 0) {
	SV **svp;
	idx = freeCRidx;
	svp = av_fetch(CRarray, (I32) idx, 0);
	if (svp == NULL) {
	    /* Something's screwy... */
	    freeCRidx = -1;
	}
	else {
	    freeCRidx = (int) SvIV(*svp);
	}
	if (av_store(CRarray, (I32) idx, SvREFCNT_inc(coderef)) == NULL) {
	    cantstore = 1;
	    SvREFCNT_dec(coderef);
	}
    }
    else
	cantstore = 1;

    if (cantstore) {
	av_push(CRarray, SvREFCNT_inc(coderef));
	idx = av_len(CRarray);
    }

    sprintf(buf, CRfmtstr, idx);
    return strdup(buf);
}

#define HAVE_BROKEN_PERL_ANON_SUB_DEALLOC 1

#if HAVE_BROKEN_PERL_ANON_SUB_DEALLOC
static unsigned CRs_tofree_maxsize = 0;
static unsigned CRs_tofree_idx = 0;
static SV **CRs_tofree = NULL;
#endif /* HAVE_BROKEN_PERL_ANON_SUB_DEALLOC */

void
perl_free_deferred()
{
#if HAVE_BROKEN_PERL_ANON_SUB_DEALLOC
    while (CRs_tofree_idx > 0) {
	CRs_tofree_idx--;
	SvREFCNT_dec(CRs_tofree[CRs_tofree_idx]);
    }
#endif /* HAVE_BROKEN_PERL_ANON_SUB_DEALLOC */
}

int
perl_free_callback(char *callback)
{
    int idx;
    if (sscanf(callback, CRfmtstr, &idx) == 1) {
	SV **svp;
	SV *svfreeCRidx;
	svp = av_fetch(CRarray, (I32) idx, 0);
	if (svp == NULL)
	    return 0;	/* Something screwy, bail... */

	if (!SvROK(*svp) || SvTYPE(SvRV(*svp)) != SVt_PVCV)
	    return 0;	/* Most likely freed already (?) */


	/* I used to have the following line in the code here:

		SvREFCNT_dec(*svp);

	   But I now think this is wrong.  The reason is that while
	   the av_store does not increment the reference count of the
	   thing you're storing into the array, it does appear to
	   decrement the reference count (and potentially free) the
	   thing being replaced.

	   So we won't decrement the reference count here.  If
	   we did it here, we'd be doing it twice which is wrong. */

#if HAVE_BROKEN_PERL_ANON_SUB_DEALLOC
	/* There's a bug in perl5.00503 (and probably other
	   versions as well) in which perl tries to look
	   at some memory associated with a coderef after
	   it's been freed.  Perhaps it was freed too early;
	   I don't know.  I have been able to reproduce this
	   problem in an ordinary (extension free) perl program,
	   so I do know that there's a bug in perl.

	   So here's how we work around it... we'll increment
	   the reference count and stash the coderef away
	   so that we can decrement it at a later time when
	   it'll (hopefully) be safer to do so. */

	SvREFCNT_inc(*svp);
	if (CRs_tofree_idx >= CRs_tofree_maxsize) {
	    unsigned oldsize = CRs_tofree_maxsize;
	    CRs_tofree_maxsize += 4;
	    GROW(CRs_tofree, SV*, oldsize, CRs_tofree_maxsize);
	}
	CRs_tofree[CRs_tofree_idx++] = *svp;
#endif /* HAVE_BROKEN_PERL_ANON_SUB_DEALLOC */

	svfreeCRidx = newSViv((IV)freeCRidx);
	if (av_store(CRarray, (I32) idx, svfreeCRidx) == NULL) {
	    /* Not successful (!) */
	    SvREFCNT_dec(svfreeCRidx);
	}
	else {
	    freeCRidx = idx;
	}
    }
    return 1;
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

    if (!SvIOKp(sv) && strcmp(SvPV(sv,PL_na),"$") == 0) {
	linenum = vl_line_count(curbp);
    }
    else if (!SvIOKp(sv) && strcmp(SvPV(sv,PL_na),"$$") == 0) {
	linenum = vl_line_count(curbp) + 1;
    }
    else {
	linenum = (I32) SvIV(sv);
	if (linenum < 1) {
	    linenum = 1;
	}
	else if (linenum > vl_line_count(curbp)) {
	    linenum = vl_line_count(curbp);
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
    if (!SvIOKp(sv) && strcmp(SvPV(sv,PL_na),"$") == 0) {
	offset = llength(DOT.l) - 1;
	if (offset < 0)
	    offset = 0;
    }
    else if (!SvIOKp(sv) && strcmp(SvPV(sv,PL_na),"$$") == 0) {
	offset = llength(DOT.l);
    }
    else {
	offset = (I32) SvIV(sv);
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
 *
 * Implementation notes:  In my first attempts at implementing these
 * functions, I used the region's end mark to determine when/where to
 * stop.  This usually worked, but there were times (particularly
 * when the inplace_edit flag was set to true) when the end marker
 * would either disappear entirely or would move to some undesirable
 * location.  I considered "fixing" the mark machinery so that it
 * would work as I desired.  The only problem with this is that my
 * attempts to do this could either destabilize the rest of the editor
 * or cause vile to be incompatible with real vi.
 *
 * So I decided that a lot of the code would be cleaner if I simply
 * gave up trying to use the end marker.  Instead I rely on the
 * length field, r_size, located in the region structure to determine
 * the right time to stop.  The only problem I see with this approach
 * is that motions could screw it up... but I suppose we can adjust
 * the r_size field in one way or another to account for this.
 *
 * In any event, I've decided that it is a bad idea to rely on
 * either of the marks once buffer modifications have started.
 */

static int
svgetline(SV **svp, VileBuf *vbp, char *rsstr GCC_UNUSED, STRLEN rslen GCC_UNUSED)
{
    B_COUNT len;
    B_COUNT nllen;
    char *text;
    SV *sv;
    const char *ending = get_record_sep(curbp);
    B_COUNT len_rs = (B_COUNT) len_record_sep(curbp);

    if (is_header_line(DOT, curbp)
     || vbp->region.r_size <= 0
     || llength(DOT.l) < DOT.o) {
	*svp = newSVsv(&PL_sv_undef);
	return FALSE;
    }

    len = (B_COUNT) (llength(DOT.l) - DOT.o);
    text = lvalue(DOT.l) + DOT.o;

    if (len > vbp->region.r_size)
	len = vbp->region.r_size;

    if (   vbp->region.r_size > 0
	&& (   lforw(DOT.l) != buf_head(curbp)
	    || b_val(curbp, MDNEWLINE)))
    {
	nllen = len_rs;
	DOT.o = b_left_margin(curbp);
	DOT.l = lforw(DOT.l);
    }
    else {
	nllen = 0;
	DOT.o += (C_NUM) len;
    }

    vbp->region.r_size -= len + nllen;

    if (vbp->inplace_edit)
	vbp->ndel += len + nllen;

    *svp = sv = newSV(len + nllen + 1);	/* +1 for \0 */

    if (len > 0) {
	sv_setpvn(sv, text, len);
	if (nllen > 0)
	    sv_catpvn(sv, ending, len_rs);
    }
    else if (nllen > 0) {
	sv_setpvn(sv, ending, len_rs);
    }
    else {
	sv_setpvn(sv, "", (size_t) 0);
    }

    return TRUE;
}

/*
 * Fetch an entire region (or remainder thereof).
 */

static int
svgetregion(SV **svp, VileBuf *vbp, char *rsstr GCC_UNUSED, STRLEN rslen GCC_UNUSED)
{
    B_COUNT len;
    SV *sv;
    LINE *lp;
    C_NUM off;
    const char *ending = get_record_sep(curbp);
    STRLEN len_rs = (STRLEN) len_record_sep(curbp);

    if (is_header_line(DOT, curbp) || vbp->region.r_size <= 0) {
	*svp = newSVsv(&PL_sv_undef);
	return FALSE;
    }

    len = vbp->region.r_size;
    vbp->region.r_size = 0;

    *svp = sv = newSV(len + 1);	/* + 1 for \0 */
    sv_setpvn(sv, "", (size_t) 0);

    if (vbp->inplace_edit)
	vbp->ndel += len;

    lp = DOT.l;
    off = DOT.o;
    while (len > 0) {
	STRLEN clen = (STRLEN) (llength(lp) - off);

	if (clen > len)
	    clen = len;

	if (clen > 0) {
	    sv_catpvn(sv, lvalue(lp) + off, clen);
	    len -= clen;
	    off += (C_NUM) clen;
	}

	if (len > 0) {
	    if (lforw(lp) != buf_head(curbp) || b_val(curbp, MDNEWLINE))
		sv_catpvn(sv, ending, len_rs);
	    len -= (B_COUNT) len_rs;
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
svgettors(SV **svp, VileBuf *vbp, char *rsstr, STRLEN rslen)
{
    B_COUNT len, reglen;
    SV *sv;
    int rs1;
    STRLEN orig_rslen = rslen;
    LINE *lp;
    C_NUM off;
    const char *ending = get_record_sep(curbp);
    STRLEN len_rs = (STRLEN) len_record_sep(curbp);
    char temp[10];

    /* See if we're already at the end of the region and have nothing
       to do. */
    if (is_header_line(DOT, curbp) || vbp->region.r_size <= 0) {
	*svp = newSVsv(&PL_sv_undef);
	return FALSE;
    }

    /* Adjust rsstr if need be */
    if (rslen == 0) {
	rslen = strlen(ending);
	if (rslen < (sizeof(temp) / 2) - 1) {
	    rsstr = strcat(strcpy(temp, ending), ending);
	    rslen = strlen(rsstr);
	} else {	/* should never happen, but analyzers do not know */
	    rsstr = temp;
	    *rsstr = '\0';
	    rslen = 0;
	}
    }

    /* Get first separator character */
    rs1 = *rsstr;

    /* Compute length of region up to record separator or til
       end of region, whichever comes first */
    lp  = DOT.l;
    off = DOT.o;
    len = 0;
    reglen = vbp->region.r_size;
    for (;;) {
	C_NUM loff;
	int cont_off;
	LINE *cont_lp;
	int fidx;
	STRLEN rsidx;

	if (off > llength(lp)) {
	    off = 0;
	    lp = lforw(lp);
	}

	if (lp == buf_head(curbp) || len >= reglen)
	    goto have_length;

	/* loff is the last offset that we'll do our initial search
	   to on this line */
	loff = llength(lp);
	if ((B_COUNT) (loff - off) > (reglen - len)) {
	    loff = off;
	    if (reglen > len)
		loff +=  (C_NUM) (reglen - len);
	}

	/* Try to find the first record separator character */
	if (rs1 == '\n') {
	    /* newline; no searching needed, must be at end of line */
	    if (loff < llength(lp)) {
		len += (B_COUNT) loff;
		goto have_length;
	    }
	    else
		fidx = loff;
	}
	else {
	    /* Gotta search */
	    for (fidx = off; fidx < loff && lvalue(lp)[fidx] != rs1; fidx++)
		;
	    if (fidx >= loff) {
		if (loff < llength(lp)) {
		    len += (B_COUNT) loff;
		    goto have_length;
		}
		len += (B_COUNT) (loff - off + 1);
		off = loff + 1;
		continue;
	    }
	}

	/* If we get to this point, fidx points at first character in
	   the record separator. */
	len += (B_COUNT) (fidx - off + 1);
	cont_lp = lp;
	cont_off = fidx + 1;

	/* Attempt to match the rest of the record separator */
	for (rsidx = 1; rsidx < rslen; rsidx++) {
	    fidx++;
	    if (fidx >= llength(lp)) {
		lp = lforw(lp);
		fidx = 0;
	    }
	    if (lp == buf_head(curbp) || len + (B_COUNT) (rsidx - 1) >= reglen) {
		len += (B_COUNT) (rsidx - 1);
		goto have_length;
	    }
	    if (isreturn(rsstr[rsidx])) {
		if (fidx < llength(lp))
		    break;
	    }
	    else if (rsstr[rsidx] != lvalue(lp)[fidx])
		break;
	}

	if (rsidx >= rslen) {
	    len += (B_COUNT) (rslen - 1);
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
	while (   ! (lp == buf_head(curbp) || len >= reglen)
	       && llength(lp) == 0)
	{
	    len += len_rs;
	    lp = lforw(lp);
	}
    }

    /* Make sure there's still something left to return */
    if (len <= 0) {
	*svp = newSVsv(&PL_sv_undef);
	return FALSE;
    }

    vbp->region.r_size -= len;

    /* Now copy the region over to the SV... */
    *svp = sv = newSV(len + 1);	/* + 1 for \0 */
    sv_setpvn(sv, "", (size_t) 0);

    if (vbp->inplace_edit)
	vbp->ndel += len;

    lp = DOT.l;
    off = DOT.o;
    while (len > 0) {
	if (llength(lp) > off) {
	    B_COUNT clen = (B_COUNT) (llength(lp) - off);

	    if (clen > len)
		clen = len;

	    if (clen > 0) {
		sv_catpvn(sv, lvalue(lp) + off, clen);
		len -= clen;
		off += (C_NUM) clen;
	    }
	}

	if (len > 0) {
	    if (lforw(lp) != buf_head(curbp) || b_val(curbp, MDNEWLINE))
		sv_catpvn(sv, ending, len_rs);
	    len -= len_rs;
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

static char *
FindMode(char *mode, int isglobal, VALARGS *args)
{
    int status = FALSE;
    int literal = (toktyp(mode) == TOK_LITSTR);
    const char *value;
    char *result = NULL;
    TBUFF *temp = NULL;
    char new_mode[NLINE];

    if (literal)
	status = find_mode(curbp, mode, isglobal, args);

    if (status == TRUE) {
	value = string_mode_val(args);
    } else {
	if (literal) {
	    new_mode[0] = '$';
	    vl_strncpy(new_mode+1, mode, sizeof(new_mode)-1);
	    value = tokval(new_mode);
	} else {
	    /*
	     * A function should be legal anywhere a variable value is.
	     */
	    if (toktyp(mode) == TOK_FUNCTION) {
		char *save_str = execstr;
		int save_flag = clexec;

		if ((temp = tb_scopy(&temp, mode)) != NULL
		    && tb_length(temp) != 0) {

		    execstr = skip_text(tb_values(temp));
		    clexec = FALSE;
		    if (isSpace(*execstr)) {
			*execstr++ = 0;
		    }
		    tb_scopy(&temp, tokval(tb_values(temp)));
		    tb_dequote(&temp);
		    value = tb_values(temp);

		    execstr = save_str;
		    clexec = save_flag;
		} else {
		    value = "";
		}
	    } else {
		value = tokval(mode);
	    }
	}
    }

    if (isErrorVal(value)) {
	result = error_val;
    } else if (value != NULL) {
	if ((result = strmalloc(value)) == NULL)
	    value = "";
    } else {
	value = "";
    }
    TRACE(("value of %s(%s) = %s\n", status ? "mode" : "", mode, value));
    tb_free(&temp);
    return result;
}

static void
FreeMode(char *value)
{
    /* we made a copy to avoid possibly writing on const (perl 5.005_03) */
    if (isLegalVal(value))
	free(value);
}

/* A version of set_curwp that does nothing but the bare essentials. We
   use it in this file when we want to temporarily change the current
   window/buffer being acted upon.  It's important to do it this way so
   that the buffer tracking list is not affected.  Also, we absolutely
   don't want to run any of the hooks. */
static WINDOW *
set_curwp0(WINDOW *wp)
{
    WINDOW *oldwp = curwp;

    curwp = wp;
    curbp = wp->w_bufp;
    return oldwp;
}


MODULE = Vile	PACKAGE = Vile

PROTOTYPES: DISABLE

  #
  # =back
  #
  # =head2 Loading Perl Modules from Vile
  #
  # A perl module that is usable by vile should probably be
  # located some place on the @INC path.  For vile, the @INC
  # array has been augmented to include $HOME/.vile/perl and
  # /usr/local/share/vile.  (This latter path may differ depending
  # upon your machine and configuration options.)  If you want to
  # see what exactly what these paths are, just issue the following
  # command from within vile:
  #
  #     :perl print join ':', @INC[0,1]
  #
  # Let us suppose that the following script is stored in
  # $HOME/.vile/perl/number_lines.pl.
  #
  #     sub number_lines {
  #         my ($lnum, $width) = @_;
  #
  #         $lnum = 1 unless defined($lnum);
  #         $width = 4 unless defined($width);
  #
  #         $Vile::current_buffer->inplace_edit(1);
  #
  #         while (<$Vile::current_buffer>) {
  #             print $Vile::current_buffer
  #                   ' ' x ($width - length($lnum) - 1),
  #                   $lnum, ' ', $_;
  #             $lnum++;
  #         }
  #     }
  #
  #     1;
  #
  # Note the trailing "1;" at the end.  The reason for this is so
  # that I<true> is returned as the result of the script.  If things
  # are not done this way, the loading mechanism might flag an
  # error.  (All it needs to do is return a true value somehow.)
  #
  # Assuming the above code has been placed in the file
  # 'number_lines.pl', the following vile command may be used
  # to load it:
  #
  #         :perl require 'number_lines.pl'
  #
  # When writing a new script, I will often test it in the same
  # editor session that I've created the script in.  My script
  # may have a bug in it and I'll fix it.  In order to reload
  # the script, you can do the following:
  #
  #         :perl do 'number_lines.pl'
  #
  # Perl's builtin 'require' function wouldn't have worked to
  # reload the file because it keeps track of files that have
  # been loaded via this facility and refuses to load a file
  # twice.  The 'do' function on the other hand is a more general
  # facility for executing the contents of a file and doesn't
  # care how often it's called.
  #
  # Caveat: Sometimes it's better to start with a clean slate,
  # particularly if you've renamed your subroutines or if there
  # are global variables involved.  Just start a fresh copy of
  # vile and start over.
  #
  # Now to invoke our number_lines program, we do it as follows:
  #
  #         :perl number_lines(1)
  #
  # It is also possible to use vile's builtin macro language to
  # load perl modules and call them.  The hgrep.pl module comes
  # with the I<vile> distribution.  You may want to put the following
  # line in your F<.vilerc> file:
  #
  #	perl "use hgrep"
  #
  # See also the Vile::C<register> functions.
  #


MODULE = Vile	PACKAGE = Vile

  #
  # =head2 Package Vile
  #
  # The B<Vile> package contains subroutines and methods of a
  # general nature.  They range from providing an interface to
  # I<vile's> modes to providing facilities for obtaining user input.
  #
  # =head2 Package Vile Subroutines and Methods
  #
  # =over 4
  #
  #
  # =item Warn
  #
  # Print a warning message
  #
  # =for html <br><br>
  #

void
Warn(warning)
    char *warning

    CODE:
	write_message("perl warn:", GvSV(PL_errgv));
	sv_catpv(GvSV(PL_errgv),warning);

  #
  # =item beep
  #
  # Rings terminal bell (or equivalent).
  #
  # =for html <br><br>
  #

void
beep()
    CODE:
	kbd_alarm();

  #
  # =item buffers
  #
  # Returns a list of the editor's buffers.
  #
  # =for html <br><br>
  #

void
buffers(...)

    PREINIT:
	BUFFER *bp;

    PPCODE:

	if (! (items == 0
	       || (items == 1 && strcmp(SvPV(ST(0), PL_na), "Vile") == 0)) )
	{
	    /* Must be called as either Vile::buffers() or Vile->buffers() */
	    croak("buffers: called with too many arguments");
	}

	for_each_buffer(bp) {
	    XPUSHs(sv_2mortal(newVBrv(newSV((size_t) 0), api_bp2vbp(bp))));
	}

  #
  # =item command CMDLINE
  #
  # executes the given vile command line (as if it were typed on the :
  # line).
  #
  # This is not exactly safe in all contexts.  (It is easy to cause
  # seg faults.) If you need access to some portion of vile that would
  # lead you to want to use this subroutine, let me know and I will
  # work on suitable facilities.
  #
  # =for html <br><br>
  #

int
command(cline)
    char *cline

    PREINIT:
	int save_vl_msgs;

    CODE:
	save_vl_msgs = vl_msgs;
	vl_msgs = FALSE;
	RETVAL = docmd(cline, TRUE, FALSE, 1);
	vl_msgs = save_vl_msgs;

    OUTPUT:
	RETVAL

  #
  # =item keystroke
  #
  # =item keystroke WAITVAL
  #
  # Returns a single, fairly raw keystroke from the keyboard.
  #
  # The optional WAITVAL indicates if the editor should wait for the
  # next keystroke.  When WAITVAL is false, undef will
  # be returned if no input is ready.
  #
  # =for html <br><br>
  #

void
keystroke(...)

    PREINIT:
	int noget;
    PPCODE:
	if (items > 1)
	    croak("Too many arguments to keystroke");

	curwp = curwp_visible ? curwp_visible : curwp;
	curbp = curwp->w_bufp;

	noget = FALSE;

	if (items == 1 && SvTRUE(ST(0)) == 0) {
	    if (!sysmapped_c_avail()) {
		XPUSHs(&PL_sv_undef);
		noget = TRUE;
	    }
	}

	if (!noget)
	    XPUSHs(sv_2mortal(newSViv((IV)sysmapped_c())));

	curwp_visible = curwp;

  #
  # =item mlreply PROMPT
  #
  # =item mlreply PROMPT, INITIALVALUE
  #
  # Prompts the user with the given prompt and (optional) supplied
  # initial value.  Certain characters that the user may input have
  # special meanings to mlreply and may have to be escaped by the
  # user to be input.  If this is unacceptable, use mlreply_no_opts
  # instead.
  #
  # Returns the user's response string.  If the user aborts
  # (via the use of the escape key) the query, an undef is
  # returned.
  #
  # =for html <br><br>
  #
  # =item mlreply_no_opts PROMPT
  #
  # =item mlreply_no_opts PROMPT, INITIALVALUE
  #
  # Prompts the user with the given prompt and (optional) supplied
  # initial value.  All printable characters may be entered by the
  # user without any special escapes.
  #
  # Returns the user's response string.  If the user aborts
  # (via the use of the escape key) the query, an undef is
  # returned.
  #
  # =for html <br><br>
  #
  # =item mlreply_shell PROMPT
  #
  # =item mlreply_shell PROMPT, INITIALVALUE
  #
  # Like mlreply, but provide completions suitable for fetching shell
  # commands.
  #
  # =for html <br><br>
  #

void
mlreply(prompt, ...)
    char *prompt

    ALIAS:
	Vile::mlreply_no_opts = 1
	Vile::mlreply_shell = 2

    PREINIT:
	char buf[NLINE];
	int status;

    PPCODE:
	if (items == 2)
	    vl_strncpy(buf, SvPV(ST(1),PL_na), sizeof(buf));
	else if (items > 2)
	    croak("Too many arguments to mlreply");
	else
	    buf[0] = EOS;

	switch (ix) {
	    case 1:
		status = mlreply_no_opts(prompt, buf, (UINT) sizeof(buf));
		break;
	    case 2:
		status = mlreply_no_bs(prompt, buf, (UINT) sizeof(buf));
		break;
	    default:
		status = mlreply(prompt, buf, (UINT) sizeof(buf));
		break;
	}
#if OPT_HISTORY
	if (status == TRUE)
	    hst_glue('\r');
#endif
	XPUSHs((status == TRUE || status == FALSE)
		 ? sv_2mortal(newSVpv(buf, (size_t) 0))
		 : &PL_sv_undef);


  #
  # =item mlreply_dir PROMPT
  #
  # =item mlreply_dir PROMPT, INITIALVALUE
  #
  # Prompts the user for a directory name with the given prompt
  # and (optional) initial value.  Filename completion (via the
  # TAB key, if enabled) may be used to assist in entering
  # the directory name.
  #
  # Returns the user's response string.  If the user aborts
  # (via the use of the escape key) the query, an undef is
  # returned.
  #
  # =for html <br><br>
  #

void
mlreply_dir(prompt, ...)
    char *prompt

    PREINIT:
	char buf[NFILEN];
	static TBUFF *last;
	int status;

    PPCODE:
	if (items == 2) {
	    tb_scopy(&last, SvPV(ST(1),PL_na));
	}
	else if (items > 2) {
	    croak("Too many arguments to mlreply_dir");
	}

	buf[0] = EOS;
	status = mlreply_dir(prompt, &last, buf);
#if OPT_HISTORY
	if (status == TRUE)
	    hst_glue('\r');
#endif
	XPUSHs((status == TRUE || status == FALSE)
		 ? sv_2mortal(newSVpv(buf, (size_t) 0))
		 : &PL_sv_undef);


  #
  # =item mlreply_file PROMPT
  #
  # =item mlreply_file PROMPT, INITIALVALUE
  #
  # Prompts the user for a filename with the given prompt and
  # (optional) initial value.  Filename completion (via the
  # TAB key, if enabled) may be used to assist in entering
  # the filename.
  #
  # Returns the user's response string.  If the user aborts
  # (via the use of the escape key) the query, an undef is
  # returned.
  #
  # =for html <br><br>
  #

void
mlreply_file(prompt, ...)
    char *prompt

    PREINIT:
	char buf[NFILEN];
	static TBUFF *last;
	int status;

    PPCODE:
	if (items == 2) {
	    tb_scopy(&last, SvPV(ST(1),PL_na));
	}
	else if (items > 2) {
	    croak("Too many arguments to mlreply_file");
	}

	buf[0] = EOS;
	status = mlreply_file(prompt, &last, FILEC_UNKNOWN, buf);
#if OPT_HISTORY
	if (status == TRUE)
	    hst_glue('\r');
#endif
	XPUSHs((status == TRUE || status == FALSE)
		 ? sv_2mortal(newSVpv(buf, (size_t) 0))
		 : &PL_sv_undef);

  #
  # =item selection_buffer
  #
  # =item selection_buffer BUFOBJ
  #
  # =item Vile::Buffer::set_selection BUFOBJ
  #
  # Gets or sets the buffer associated with the current selection.
  #
  # When getting the selection, the buffer object that has the current
  # selection is returned and its region is set to be the same region
  # as is occupied by the selection.  If there is no current selection, undef
  # is returned.
  #
  # When setting the selection, a buffer object must be passed in.  The
  # editor's selection is set to the region associated with the buffer object.
  # If successful, the buffer object is returned; otherwise undef will be
  # returned.
  #
  # Examples:
  #
  #	$sel = Vile->selection_buffer->fetch;
  #                                     # Put the current selection in $sel
  #
  #     Vile->selection_buffer($Vile::current_buffer);
  #                                     # Set the selection to the region
  #                                     # contained in the current buffer
  #
  # Vile::Buffer::set_selection is an alias for Vile::selection_buffer, but
  # can only function as a setter.  It may be used like this:
  #
  #     Vile->current_buffer->set_region('w')->set_selection;
  #                                     # set the selection to be the word
  #                                     # starting at the current position
  #                                     # in the current buffer
  #
  #     Vile->current_buffer->motion('?\/\*' . "\n")
  #                         ->set_region('%')
  #                         ->set_selection();
  #                                     # set the selection to be the nearest
  #                                     # C-style comment above or at the
  #                                     # current position.
  #
  # =for html <br><br>
  #

void
selection_buffer(...)

    ALIAS:
	Vile::Buffer::set_selection = 1

    PREINIT:
	int argno;

    PPCODE:
#if OPT_SELECTIONS
	argno = 0;

	if (strcmp(SvPV(ST(argno), PL_na), "Vile") == 0)
	    argno++;

	if (items - argno == 0) { /* getter */
	    BUFFER *bp;
	    AREGION aregion;

	    bp = get_selection_buffer_and_region(&aregion);
	    if (bp != NULL) {
		VileBuf *vbp = api_bp2vbp(bp);
		if (vbp != NULL) {
		    vbp->region = aregion.ar_region;
		    vbp->regionshape =  aregion.ar_shape;
		    XPUSHs(sv_2mortal(newVBrv(newSV((size_t) 0), vbp)));
		}
	    }
	    else {
		XPUSHs(&PL_sv_undef);
	    }
	}
	else if (items - argno == 1) { /* setter */
	    VileBuf *vbp;
	    const char *croakmess;
	    /* Need a buffer object */
	    vbp = getVB(ST(argno), &croakmess);

	    if (vbp == NULL)
		croak("Vile::%s: %s",
		      ix == 1 ? "Buffer::set_selection" : "selection_buffer",
		      croakmess);
	    api_setup_fake_win(vbp, TRUE);
	    DOT = vbp->region.r_orig;
	    sel_begin();
	    DOT = vbp->region.r_end;
	    if (sel_extend(FALSE, FALSE) == TRUE) {
		XPUSHs(ST(argno));
	    }
	    else {
		XPUSHs(&PL_sv_undef);
	    }
	}
	else {
	    croak("Vile::selection_buffer: Incorrect number of arguments");
	}
#else
	croak("%s requires vile to be compiled with OPT_SELECTIONS",
	      GvNAME(CvGV(cv)));
#endif

  #
  # =item set PAIRLIST
  #
  # =item get LIST
  #
  # =item Vile::Buffer::set BUFOBJ PAIRLIST
  #
  # =item Vile::Buffer::get BUFOBJ LIST
  #
  # Provides access to Vile's various modes, buffer and otherwise.
  #
  # For the set methods, PAIRLIST should be a list of key => value
  # pairs, where key is a mode name and value is an appropriate value
  # for that mode.  When used in an array context, the resulting key =>
  # value pairs are returned.  (The value may be a different, but
  # equivalent string than originally specified.) When used in a scalar
  # context, either the package name or buffer object is returned
  # (depending on how it was invoked) in order that the result may be
  # used as the target of further method calls.
  #
  # When one of the get forms is used, a list of modes should be
  # supplied.  When used in an array context, a list of key => value
  # pairs is returned.  When used in a scalar context, only one mode
  # name may be supplied and the value associated with this mode is
  # returned.
  #
  # The methods in Vile::Buffer attempt to get the local modes
  # associated with the buffer (falling back to the global ones if no
  # specific local mode has been specified up to this point).
  #
  # Note:  Access to certain internal attributes such as the buffer
  # name and file name are not provided via this mechanism yet.  There
  # is no good reason for this other than that vile does not provide
  # access to these attributes via its set command.
  #
  # =for html <br><br>
  #

void
set(...)

    ALIAS:
	Vile::get = 1
	Vile::Buffer::set = 2
	Vile::Buffer::get = 3

    PREINIT:
	int argno;
	int isglobal;
	int issetter;
	char *mode;
	char *value;
	int status;
	VALARGS args;
	I32 gimme;
	char **modenames;
	int nmodenames = 0;

    PPCODE:
#if OPT_EVAL
	argno    = 0;
	isglobal = (ix == 0 || ix == 1);
	issetter = (ix == 0 || ix == 2);
	gimme    = GIMME_V;
	mode     = NULL;		/* just in case it never gets set */

	if (!isglobal /* one of the Vile::Buffer methods */) {
	    const char *croakmess;
	    VileBuf *vbp;

	    /* Need a buffer object */
	    vbp = getVB(ST(argno), &croakmess);
	    argno++;

	    if (vbp == NULL)
		croak("Vile::Buffer::set: %s", croakmess);

	    isglobal = 0;
	    api_setup_fake_win(vbp, TRUE);
	}
	else {
	    /* We're in the Vile package.  See if we're called via
	       Vile->set */
	    if (strcmp(SvPV(ST(argno), PL_na), "Vile") == 0)
		argno++;
	}

	nmodenames = 0;
	modenames = NULL;
	if (gimme == G_ARRAY) {
	    int n = items - argno + 1;		/* +1 in case of odd set */
	    if (!issetter)
		n *= 2;
	    if (n > 0) {
		modenames = typeallocn(char *, (unsigned) n);
		if (modenames == NULL)
		    croak("Can't allocate space");
	    }
	}

	while (argno < items) {
	    mode = SvPV(ST(argno), PL_na);
	    argno++;

	    TRACE(("Vile::%s(%d:%s)\n", issetter ? "set" : "get", argno, mode));

	    /* Look for a mode first */
	    status = FALSE;
	    memset(&args, 0, sizeof(args));
	    if (toktyp(mode) == TOK_LITSTR)
		status = find_mode(curbp, mode, isglobal, &args);

	    if (status == TRUE) {
		if (modenames)
		    modenames[nmodenames++] = mode;

		if (issetter) {
		    const char *val;
		    val = NULL;
		    if (argno >= items) {
			if (args.names->type == VALTYPE_BOOL) {
			    val = "1";
			}
			else {
			    if (modenames) free(modenames);
			    croak("set: value required for %s", mode);
			}
		    }
		    else {
			val = SvPV(ST(argno), PL_na);
			argno++;
		    }

		    if (set_mode_value(curbp, mode, FALSE, TRUE, isglobal, &args, val) != TRUE) {
			if (modenames) free(modenames);
			croak("set: Invalid value %s for mode %s", val, mode);
		    }
		}
	    } else {
		const char *val = NULL;

		if (modenames)
		    modenames[nmodenames++] = mode;

		if (issetter) {
		    if (argno >= items) {
			if (modenames) free(modenames);
			croak("set: value required for %s", mode);
		    }
		    else {
			val = SvPV(ST(argno), PL_na);
			argno++;
		    }
		    status = set_state_variable(mode, val);

		    if (status != TRUE) {
			if (modenames) free(modenames);
			croak("set: Unable to set variable %s to value %s",
			      mode, val);
		    }
		}
	    }
	}

	if (modenames == NULL) {
	    if (issetter) {
		if (isglobal)
		    XPUSHs(sv_2mortal(newSVpv("Vile", (size_t) 0)));
		else
		    XPUSHs(ST(0));	/* Buffer object */
	    }
	    else {
		if (mode != NULL) {
		    value = FindMode(mode, isglobal, &args);
		    XPUSHs(sv_2mortal(newSVpv(value, (size_t) 0)));
		    FreeMode(value);
		}
	    }
	}
	else {
	    int i;
	    for (i = 0; i < nmodenames; i++) {
		mode = modenames[i];
		value = FindMode(mode, isglobal, &args);
		XPUSHs(sv_2mortal(newSVpv(mode, (size_t) 0)));
		XPUSHs(sv_2mortal(newSVpv(value, (size_t) 0)));
		FreeMode(value);
	    }
	    free(modenames);
	}
#else
	croak("%s requires vile to be compiled with OPT_EVAL",
	      GvNAME(CvGV(cv)));
#endif

  #
  # =item update
  #
  # Update the editor's display.  It is usually not necessary to
  # call this if you're returning to the editor in fairly short
  # order.  It will be necessary to call this, for example, if
  # you write an input loop in perl which writes things to some
  # on-screen buffers, but does not return to the editor immediately.
  #
  # =for html <br><br>
  #

void
update()
    PPCODE:
	api_update();

  #
  # =item working
  #
  # =item working VAL
  #
  # Returns value 1 if working message will be printed during
  # substantial pauses, 0 if disabled.
  #
  # When passed an argument, modifies value of the working message.
  #
  # =for html <br><br>
  #

int
working(...)

    CODE:
#if OPT_WORKING
	RETVAL = !vile_is_busy;
#else
	RETVAL = 0;
#endif
	if (items > 1)
	    croak("Too many arguments to working");
	else if (items == 1) {
#if OPT_WORKING
	    vile_is_busy = !SvIV(ST(0));
#endif
	}
    OUTPUT:
	RETVAL

  #
  # =item register NAME, [SUB, HELP, REQUIRE]
  #
  # Register a subroutine SUB as Vile function NAME.  Once registered,
  # the subroutine may then be invoked as a named command and bound to
  # keystrokes.
  #
  # SUB may be given either as a string to eval, or a reference to a
  # subroutine.  If omitted, SUB defaults to NAME.
  #
  # HELP provides a description of the subroutine for the [Binding
  # List] functions.
  #
  # An optional file to require may be given.
  #
  # Example:
  #
  #     Vile::register grep => 'hgrep', 'recursive grep', 'hgrep.pl';
  #
  # or
  #
  #     require 'hgrep.pl';
  #     Vile::register grep => \&hgrep, 'recursive grep';
  #
  # also
  #
  #     sub foo { print "foo" }
  #     Vile::register 'foo';
  #     Vile::register bar => 'print "bar"';
  #     Vile::register quux => sub { print "quux" };
  #
  # =item register_motion NAME, [SUB, HELP, REQUIRE]
  #
  # =item register_oper NAME, [SUB, HELP, REQUIRE]
  #
  # These synonyms for Vile::C<register> allow perl subroutines to
  # behave as motions and operators.  For example, these subroutines
  # behave like their builtin counterparts:
  #
  #     *cb = \$Vile::current_buffer;
  #     Vile::register_motion 'my-forward-line-at-bol' => sub {
  #         $cb->dot((scalar $cb->dot) + 1, 0);
  #     };
  #
  #     Vile::register_oper 'my-delete-til' => sub { $cb->delete };
  #
  # =for html <br><br>
  #

void
register(name, ...)
    char *name

    ALIAS:
	register_motion = MOTION
	register_oper = OPER

    PREINIT:
	CMDFUNC *cmd;
	AV *av;
	char *p;

    PPCODE:
	TRACE(("Vile::register %s\n", name));
#if OPT_NAMEBST
	if (items > 4)
	    croak("Too many arguments to %s", GvNAME(CvGV(cv)));

	for (p = name; *p; p++)
	    if (!isAlnum(*p) && *p != '-' && *p != '_')
		croak("invalid subroutine name");

	if ((cmd = typecalloc(CMDFUNC)) == NULL)
	    croak("Can't allocate space");

	ix |= (I32) ((ix == OPER) ? RANGE : VIEWOK);
	cmd->cu.c_perl = av = newAV();
	cmd->c_flags = (REDO | UNDO | CMD_PERL | (CMDFLAGS) ix);
#if OPT_ONLINEHELP
	cmd->c_help = strmalloc((items > 2 && SvTRUE(ST(2)))
				? SvPV(ST(2), PL_na)
				: "Perl subroutine");
#endif

	if (insert_namebst(name, cmd, FALSE, 0) != TRUE)
	{
#if OPT_ONLINEHELP
	    free((char *) cmd->c_help);
#endif
	    free(cmd);
	    av_undef(av);
	}
	else
	{
	    /* push the name */
	    av_push(av, newSVpv(name, (size_t) 0));

	    /* push the subroutine */
	    if (items > 1 && SvTRUE(ST(1)))
	    {
		SvREFCNT_inc(ST(1));
		av_push(av, ST(1));

		/* push the require */
		if (items > 3 && SvTRUE(ST(3)))
		{
		    SvREFCNT_inc(ST(3));
		    av_push(av, ST(3));
		}
	    }
	    else /* sub = name */
		av_push(av, newSVpv(name, (size_t) 0));
	}
#else
	croak("%s requires vile to be compiled with OPT_NAMBST",
	      GvNAME(CvGV(cv)));
#endif

  #
  # =item watchfd FD, WATCHTYPE, CALLBACK
  #
  # Adds a callback so that when the file descriptor FD is available
  # for a particular type of I/O activity (specified by WATCHTYPE),
  # the callback associated with CALLBACK is called.
  #
  # WATCHTYPE must be one of 'read', 'write', or 'except' and have
  # the obvious meanings.
  #
  # The callback should either be a string representing a vile
  # command to execute (good luck) or (more usefully) a Perl subroutine
  # reference.
  #
  # =for html <br><br>
  #

void
watchfd(fd, watchtypestr, ...)
    int fd
    char *watchtypestr

    PREINIT:
	char *cmd;
	WATCHTYPE watchtype = 0;


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
	    cmd = strdup(SvPV(ST(2),PL_na));
	}
	TRACE(("Vile::watchfd(fd=%d, watchtype=%d, cmd=%s)\n", fd, watchtype, cmd));
	watchfd(fd, watchtype, cmd);

  #
  # =item unwatchfd FD
  #
  # Removes the callback associated with FD and frees up the
  # associated data structures.
  #
  # =for html <br><br>
  #

void
unwatchfd(fd)
    int fd

    PPCODE:
	TRACE(("Vile::unwatchfd(fd=%d)\n", fd));
	unwatchfd(fd);


MODULE = Vile	PACKAGE = Vile::Buffer

  #
  # =back
  #
  # =head2 Package Vile::Buffer
  #
  # The Vile::Buffer package contains methods for creating new buffers
  # and for accessing already existing buffers in various ways.
  #
  # A Vile::Buffer object may be viewed as a filehandle.  Therefore,
  # the usual sorts of methods for reading from and writing to
  # filehandles will work as expected.
  #
  # Example:
  #
  # A word count program that you might invoke from your favorite
  # shell could be written as follows:
  #
  #     #!/usr/local/bin/perl -w
  #
  #     my $words;
  #     while (<>) {
  #         $words += split;
  #     }
  #     print "$words words\n";
  #
  # A programmer accustomed to the above, will find Vile's perl
  # interface to be a comfortable one.  Here is the above script
  # modified slightly to count the words in the current buffer:
  #
  #     sub wc {
  #         my $words;
  #         while (<$Vile::current_buffer>) {
  #             $words+=split;
  #         }
  #         print "$words words";
  #     }
  #
  # =head2 Package Vile::Buffer Methods
  #
  # =over 4
  #

  #
  # =item <BUFOBJ>
  #
  # When used in a scalar context, returns the next line or portion of
  # thereof in the current region.
  #
  # When used in an array context, returns the rest of the lines (or
  # portions thereof) in the current region.
  #
  # The current region is either set with set_region or set by default
  # for you when perl is invoked from vile.  This region will either
  # be the region that the user specified or the whole buffer if not
  # user specified.  Unless you know for sure that the region is set
  # properly, it is probably best to set it explicitly.
  #
  # After a line is read, DOT is left at the next location in the
  # buffer at which to start reading.  Note, however, that the value
  # of DOT (which a convenient name for the current position in the
  # buffer) is not propagated back to any of the user's windows unless
  # it has been explicitly set by calling dot (the method).
  #
  # When the I<inplace_edit> flag has been set via the C<inplace_edit>
  # method, text that is retrieved from the buffer is deleted
  # immediately after retrieval.
  #
  # Examples:
  #
  #     # Example 1: Put all lines of the current buffer into
  #     #            an array
  #
  #     $Vile::current_buffer->set_region(1,'$$');
  #                                     # Set the region to be the
  #                                     # entire buffer.
  #     my @lines = <$Vile::current_buffer>;
  #                                     # Fetch all lines and put them
  #                                     # in the @lines array.
  #     print $lines[$#lines/2] if @lines;
  #                                     # Print the middle line to
  #                                     # the status line
  #
  #
  #     # Example 2: Selectively delete lines from a buffer
  #
  #     my $curbuf = $Vile::current_buffer;
  #                                     # get an easier to type handle
  #                                     # for the current buffer
  #     $curbuf->inplace_edit(1);       # set the inplace_edit flag
  #                                     # so that lines will be deleted
  #                                     # as they are read
  #
  #     while (<$curbuf>) {             # fetch line into $_
  #         unless (/MUST\s+DELETE/) {  # see if we should keep the line
  #             print $curbuf $_;       # put it back if we should keep it
  #         }
  #     }
  #
  # =for html <br><br>
  #

void
READLINE(vbp)
    VileBuf * vbp

    PPCODE:
	if (is_delinked_bp(vbp2bp(vbp))) {
	    int status;
	    char buf[NLINE];
	    char prompt[NLINE];
	    buf[0] = EOS;
	    strcpy(prompt, "(perl input) ");
	    if (use_ml_as_prompt && !is_empty_buf(bminip)) {
		LINE *lp = lback(buf_head(bminip));
		if (lisreal(lp)) {
		    size_t len = (size_t) llength(lp);
		    if (len > sizeof(prompt)-1)
			len = sizeof(prompt)-1;
		    (void) memcpy(prompt, lvalue(lp), len);
		    prompt[len] = EOS;
		}
	    }
	    status = mlreply_no_opts(prompt, buf, (UINT) sizeof(buf));
#if OPT_HISTORY
	    if (status == TRUE)
		hst_glue('\r');
#endif
	    EXTEND(sp,1);
	    if (status != TRUE && status != FALSE) {
		PUSHs(&PL_sv_undef);
	    }
	    else {
		use_ml_as_prompt = 0;
		PUSHs(sv_2mortal(newSVpv(buf,(size_t) 0)));
	    }
	}
	else {
	    I32 gimme = GIMME_V;
	    struct vile_MARK old_DOT;
	    int (*fnc)(SV**, VileBuf*, char*, STRLEN);
	    char *rsstr;
	    STRLEN rslen;
#ifdef HAVE_BROKEN_PERL_RS
	    /* The input record separator, or $/ Normally, this is
	     * available via the PL_rs macro, but apparently perl5.00402
	     * on win32 systems don't export the necessary symbol from
	     * the DLL.  So we have our own...  */
	    SV *svrs = perl_get_sv("main::/", FALSE);
#else
#           define svrs PL_rs
#endif

	    if (RsSNARF(svrs)) {
		fnc = svgetregion;
		rsstr = NULL;
		rslen = 0;
	    }
	    else {
		rsstr = SvPV(svrs, rslen);
		if (rslen == 1 && isreturn(*rsstr))
		    fnc = svgetline;
		else
		    fnc = svgettors;
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
		if (fnc(&sv, vbp, rsstr, rslen))
		    IoLINES(GvIO((GV*)vbp->perl_handle))++; /* increment $. */

		if (gimme == G_SCALAR) {
		    XPUSHs(sv_2mortal(sv));
		}
	    }
	    else { /* wants an array */
		SV *sv;
		int lines = 0;

		while (fnc(&sv, vbp, rsstr, rslen)) {
		    XPUSHs(sv_2mortal(sv));
		    lines++;
		}
		IoLINES(GvIO((GV*)vbp->perl_handle)) = lines; /* set $. */
	    }
	    if (vbp->inplace_edit) {
		DOT = old_DOT;
	    }
	}

  #
  # =item attribute BUFOBJ LIST
  #
  # Attach an attributed region to the region associated with BUFOBJ
  # with the attributes found in LIST.
  #
  # These attributes may be any of the following:
  #
  #     'color' => NUM          (where NUM is the color number
  #                              from 0 to 15)
  #     'underline'
  #     'bold'
  #     'reverse'
  #     'italic'
  #     'hyper' => HYPERCMD     (where HYPERCMD is a string
  #                             representing a vile command to
  #                             execute.  It may also be a
  #                             (perl) subroutine reference.
  #     'normal'
  #
  # Normal is a special case.  It will override any other arguments
  # passed in and remove all attributes associated with the region.
  #
  # =for html <br><br>
  #

void
attribute(vbp, ...)
    VileBuf *vbp

    PPCODE:
#if OPT_SELECTIONS
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
	    char *hypercmd = NULL;
	    int status;

	    for (i = 1; i < items; i++) {
		atname = SvPV(ST(i), PL_na);
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
			    hypercmd = strdup(SvPV(ST(i),PL_na));
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
		FreeAndNull(hypercmd);
	    }

	    status = attributeregion_in_region(
			&vbp->region, vbp->regionshape, vattr, hypercmd);

	    if (status == TRUE)		/* not the same as "if (status)" */
		XPUSHs(ST(0));		/* return buffer object */
	    else
		XPUSHs(&PL_sv_undef);	/* else return undef */
	}
#else
	croak("%s requires vile to be compiled with OPT_SELECTIONS",
	      GvNAME(CvGV(cv)));
#endif

  #
  # =item attribute_cntl_a_sequences BUFOBJ
  #
  # Causes the editor to attach attributes to the <Ctrl>A
  # sequences found in the buffer for the current region (which
  # may be set via set_region).
  #
  # Returns the buffer object.
  #
  # =for html <br><br>
  #

VileBuf *
attribute_cntl_a_sequences(vbp)
    VileBuf *vbp

    CODE:
#if OPT_SELECTIONS
	api_setup_fake_win(vbp, TRUE);
	attribute_cntl_a_seqs_in_region(&vbp->region, vbp->regionshape);
	RETVAL = vbp;
#else
	croak("%s requires vile to be compiled with OPT_SELECTIONS",
	      GvNAME(CvGV(cv)));
#endif

    OUTPUT:
	RETVAL

  #
  # =item buffername BUFOBJ
  #
  # Returns the buffer name associated with BUFOBJ.
  #
  # =for html <br><br>
  #
  # =item buffername BUFOBJ BUFNAME
  #
  # Sets the buffer name associated with BUFOBJ to the string
  # given by BUFNAME.  This string must be unique.  If the name
  # given is already being used by another buffer, or if it's
  # malformed in some way, undef will be returned.  Otherwise
  # the name of the buffer will be returned.
  #
  # Note: The name of the buffer returned may be different than
  # that passed in due some adjustments that may be done on the
  # buffer name.  (It will be trimmed of spaces and a length limit
  # is imposed.)
  #
  # =for html <br><br>
  #
  # =item filename BUFOBJ
  #
  # Returns the file name associated with BUFOBJ.
  #
  # =for html <br><br>
  #
  # =item filename BUFOBJ FILENAME
  #
  # Sets the name of the file associated with BUFOBJ to the string
  # given by FILENAME.
  #
  # =for html <br><br>
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
		status = renamebuffer(curbp, SvPV(ST(1),PL_na));
	    else
		ch_fname(curbp, SvPV(ST(1),PL_na));
	}

	if (status == TRUE) {
	    XPUSHs(sv_2mortal(newSVpv((ix == 0
				       ? curbp->b_bname
				       : curbp->b_fname),
				      (size_t) 0)));
	}
	else {
	    XPUSHs(&PL_sv_undef);		/* return undef */
	}

  #
  # =item command BUFOBJ CMDLINE
  #
  # Executes the given vile command line (as if it were typed
  # on the : line) with BUFOBJ as the current buffer.
  #
  # Returns BUFOBJ if successful, otherwise returns undef.
  #
  # =for html <br><br>
  #

void
command(vbp,cline)
    VileBuf *vbp
    char *cline

    PREINIT:
	int status;
	int save_vl_msgs;
    PPCODE:
	save_vl_msgs = vl_msgs;
	vl_msgs = FALSE;
	api_setup_fake_win(vbp, TRUE);
	status = docmd(cline, TRUE, FALSE, 1);
	vl_msgs = save_vl_msgs;
	if (status) {
	    XPUSHs(ST(0));		/* return buffer object */
	}
	else {
	    XPUSHs(&PL_sv_undef);		/* return undef */
	}

  #
  # =item current_buffer
  #
  # =item current_buffer BUFOBJ
  #
  # =item current_buffer PKGNAME
  #
  # =item current_buffer BUFOBJ   NEWBUFOBJ
  #
  # =item current_buffer PKGNAME  NEWBUFOBJ
  #
  # Returns the current buffer.  When first entering perl from a vile
  # session, the current buffer is the one that the user is actively
  # editing.  Several buffers may be on the screen at once, but only one
  # of them is current.  The current one will be the one in which the
  # cursor appears.
  #
  # This method may also be used to set the current buffer.  When used in
  # the form
  #
  #     $oldbuf->current_buffer($newbuf)
  #
  # then $newbuf will replace $oldbuf in one of the visible windows.
  # (This only makes sense when $oldbuf was visible in some window on the
  # screen.  If it wasn't visible, it'll just replace whatever buffer was
  # last both current and visible.)
  #
  # When used as a setter, the current buffer is still returned.  In this
  # case it will be the new buffer object which becomes the current
  # buffer.
  #
  # Note also that the current_buffer method is in both the Vile package
  # and the Vile::Buffer package.  I couldn't decide which package it should
  # be in so I put it into both.  It seemed like a real hassle to have to
  # say
  #
  #     my $curbuf = Vile::Buffer->current_buffer
  #
  # So instead, you can just say
  #
  #     my $curbuf = Vile->current_buffer;
  #
  # current_buffer is also a variable, so you can also do it this way:
  #
  #     my $curbuf = $Vile::current_buffer;
  #
  # If you want $main::curbuf (or some other variable) to be an alias to
  # the current buffer, you can do it like this:
  #
  #     *main::curbuf = \$Vile::current_buffer;
  #
  # Put this in some bit of initialization code and then you'll never have
  # to call the current_buffer method at all.
  #
  # One more point, since $Vile::current_buffer is magical, the alias
  # above will be magical too, so you'll be able to do
  #
  #     $curbuf = $newbuf;
  #
  # in order to set the buffer.  (Yeah, this looks obvious, but realize
  # that doing the assignment actually causes some vile specific code to
  # run which will cause $newbuf to become the new current buffer upon
  # return.)
  #
  # =for html <br><br>
  #

VileBuf *
current_buffer(...)

    ALIAS:
	Vile::current_buffer = 1

    PREINIT:
	VileBuf *callbuf = NULL;
	VileBuf *newbuf = NULL;

    PPCODE:
	TRACE(("Vile::current_buffer\n"));
	if (items > 2)
	    croak("Too many arguments to current_buffer");
	else if (items == 2) {
	    if (sv_isa(ST(0), "Vile::Buffer")) {
		callbuf = INT2PTR(VileBuf *, SvIV((SV*)GvSV((GV*)SvRV(ST(0)))));
		if (callbuf == NULL) {
		    croak("buffer no longer exists");
		}
	    }
	    else
		callbuf = NULL;

	    if (sv_isa(ST(1), "Vile::Buffer")) {
		newbuf = INT2PTR(VileBuf *, SvIV((SV*)GvSV((GV*)SvRV(ST(1)))));
		if (newbuf == NULL) {
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
  # =item delete BUFOBJ
  #
  # Deletes the currently set region.
  #
  # Returns the buffer object if all went well, undef otherwise.
  #
  # =for html <br><br>
  #

VileBuf *
delete(vbp)
    VileBuf *vbp

    CODE:
	if (api_delregion(vbp))
	    RETVAL = vbp;
	else
	    RETVAL = NULL;	/* which gets turned into undef */
    OUTPUT:
	RETVAL

  #
  # =item dot BUFOBJ
  #
  # =item dot BUFOBJ LINENUM
  #
  # =item dot BUFOBJ LINENUM, OFFSET
  #
  # Returns the current value of dot (which represents the the current
  # position in the buffer).  When used in a scalar context, returns
  # the line number of dot.  When used in an array context, returns
  # the line number and position within the line.
  #
  # When supplied with one argument, the line number, dot is set to
  # the beginning of that line.  When supplied with two arguments,
  # both the line number and offset components are set.
  #
  # Either the line number or offset (or both) may be the special
  # string '$' which represents the last line in the buffer and the
  # last character on a line.
  #
  # Often times, however, the special string '$$' will be more useful.
  # It truly represents the farthest that it possible to go in both
  # the vertical and horizontal directions.  As a line number, this
  # represents the line beyond the last line of the buffer.
  # Characters inserted at this point will form a new line.  As an
  # offset, '$$' refers to the newline character at the end of a line.
  # Characters inserted at this point will be inserted before the
  # newline character.
  #
  #
  # Examples:
  #
  #     my $cb = $Vile::current_buffer; # Provide a convenient handle
  #                                     # for the current buffer.
  #
  #     $linenum = $cb->dot;            # Fetch the line number at which dot
  #                                     # is located.
  #
  #     $cb->dot($cb->dot+1);           # Advance dot by one line
  #     $cb->dot($cb->dot('$') - 1);
  #                                     # Set dot to the penultimate line of
  #                                     # the buffer.
  #
  #     $cb->dot(25, 6);                # Set dot to line 25, character 6
  #
  #     ($ln,$off) = $cb->dot;          # Fetch the current position
  #     $cb->dot($ln+1,$off-1);         # and advance one line, but
  #                                     # back one character.
  #
  #     $cb->inplace_edit(1);
  #     $cb->set_region(scalar($cb->dot), $cb->dot+5);
  #     @lines = <$cb>;
  #     $cb->dot($cb->dot - 1);
  #     print $cb @lines;
  #                                     # The above block takes (at
  #                                     # most) six lines starting at
  #                                     # the line DOT is on and moves
  #                                     # them before the previous
  #                                     # line.
  #
  # Note: current_position is an alias for dot.
  #
  # =for html <br><br>
  #
  # =item dotq BUFOBJ
  #
  # =item dotq BUFOBJ LINENUM
  #
  # =item dotq BUFOBJ LINENUM, OFFSET
  #
  # Like B<dot> except that it's "quiet" in its operation in the sense
  # that it doesn't attempt to propagate the API's concept of where the
  # current position is back to the editor when control is returned.
  #
  # This could be useful in situations where you want your Perl script
  # to quietly add some text to a buffer without disturbing any of the
  # user's windows into that buffer.
  #
  # =for html <br><br>
  #

void
dot(vbp, ...)
    VileBuf *vbp

    ALIAS:
	current_position = 1
	dotq = 2

    PREINIT:
	I32 gimme;

    PPCODE:
	api_setup_fake_win(vbp, TRUE);
	if (items > 3) {
	    croak("Vile::Buffer::%s Too many arguments",
		  ix == 1 ? "current_position" : "dot");
	}
	else if (items > 1) {
	    /* Expect a line number or '$' */

	    api_gotoline(vbp, sv2linenum(ST(1)));

	    if (items == 3)
		DOT.o = sv2offset(ST(2));

	    /* Don't allow api_dotgline to change dot if dot is explicitly
	       set.  OTOH, simply querying dot doesn't count. */
	    vbp->dot_inited = TRUE;
	    if (ix != 2) {
		/* Indicate that DOT has been explicitly changed which means
		   that changes to DOT will be propagated upon return to vile */
		vbp->dot_changed = TRUE;
	    }
	}
	gimme = GIMME_V;
	if (gimme == G_SCALAR) {
	    XPUSHs(sv_2mortal(newSViv((IV)line_no(curbp, DOT.l))));
	}
	else if (gimme == G_ARRAY) {
	    XPUSHs(sv_2mortal(newSViv((IV)line_no(curbp, DOT.l))));
	    XPUSHs(sv_2mortal(newSViv((IV)DOT.o)));
	}

  #
  # =item fetch BUFOBJ
  #
  # Returns the current region or remainder thereof.  The same effect
  # could be achieved by setting $/ to undef and then evaluating the
  # buffer object between angle brackets.
  #
  # Example:
  #
  #     $word = $Vile::current_buffer->set_region('w')->fetch;
  #                             # Fetch the next word and put it in $word
  #
  # =for html <br><br>
  #

void
fetch(vbp)
    VileBuf * vbp

    PREINIT:
	SV *sv;
	struct vile_MARK old_DOT;

    PPCODE:
	/* Set up the fake window */
	api_setup_fake_win(vbp, TRUE);
	if (!vbp->dot_inited) {
	    DOT = vbp->region.r_orig;	/* set DOT to beginning of region */
	    vbp->dot_inited = 1;
	}

	old_DOT = DOT;

	svgetregion(&sv, vbp, NULL, (STRLEN) 0);

	XPUSHs(sv_2mortal(sv));

	if (vbp->inplace_edit)
	    DOT = old_DOT;


  #
  # =item inplace_edit BUFOBJ
  #
  # =item inplace_edit BUFOBJ VALUE
  #
  # Sets the value of the "inplace edit" flag (either true of false).
  # Returns the old value.  When used without an argument, merely
  # returns current value without modifying the current value.
  #
  # This flag determines whether a line is deleted after being read.
  # E.g,
  #
  #     my $curbuf = $Vile::current_buffer;
  #     $curbuf->inplace_edit(1);
  #     while (<$curbuf>) {
  #         s/foo/bar/g;
  #         print;
  #     }
  #
  # The <$curbuf> operation will cause one line to be read and
  # deleted.  DOT will be left at the beginning of the next line.  The
  # print statement will cause $_ to get inserted prior the the next
  # line.
  #
  # Setting this flag to true is very similar to setting the
  # $INPLACE_EDIT flag (or $^I) for normal filehandles or using the B<-i>
  # switch from the command line.
  #
  # Setting it to false (which is its default value) will cause the
  # lines that are read to be left alone.
  #
  # =for html <br><br>
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
  # =item motion BUFOBJ MOTIONSTR
  #
  # Moves dot (the current position) by the given MOTIONSTR in
  # BUFOBJ.
  #
  # When used in an array context, returns a 4-tuple containing
  # the beginning and ending positions.  This 4-tuple is suitable
  # for passing to C<set_region>.
  #
  # When used in a scalar context, returns the buffer object that
  # it was called with.
  #
  # In either an array or scalar context, if the motion string was
  # bad, and undef is returned.  Motions that don't work are okay,
  # such as 'h' when you're already at the left edge of a line.  But
  # attempted "motions" like 'inewstring' will result in an error.
  #
  # Example:
  #
  #     # The following code deletes the previous 2 words and then
  #     # positions the cursor at the next occurrence of the word
  #     # "foo".
  #
  #     my $cb = $Vile::current_buffer;
  #     $cb->set_region($cb->motion("2b"))->delete;
  #                     # delete the previous two words
  #
  #     $cb->set_region("2b")->delete;
  #                     # another way to delete the previous
  #                     # two words
  #
  #     $cb->motion("/foo/");
  #                     # position DOT at the beginning of
  #                     # "foo".
  #
  #     $cb->dot($cb->dot);
  #                     # Make sure DOT gets propagated back.
  #                     # (It won't get propagated unless
  #                     # explicitly set.)
  #
  # =for html <br><br>
  #

void
motion(vbp,mstr)
    VileBuf *vbp
    char *mstr

    PREINIT:
	I32 gimme;
	struct vile_MARK old_DOT;
	int status;

    PPCODE:
	old_DOT = DOT;
	status = api_motion(vbp, mstr);

	gimme = GIMME_V;
	if (!status) {
	    XPUSHs(&PL_sv_undef);		/* return undef */
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
	    XPUSHs(sv_2mortal(newSViv((IV)sl)));
	    XPUSHs(sv_2mortal(newSViv((IV)so)));
	    XPUSHs(sv_2mortal(newSViv((IV)el)));
	    XPUSHs(sv_2mortal(newSViv((IV)eo)));
	}

  #
  # =item new BUFOBJ
  #
  # =item new PKGNAME
  #
  # =item new BUFOBJ  FILENAME
  #
  # =item new PKGNAME FILENAME
  #
  # =item edit BUFOBJ
  #
  # =item edit PKGNAME
  #
  # =item edit BUFOBJ  FILENAME
  #
  # =item edit PKGNAME FILENAME
  #
  # These methods create a new buffer and return it.
  #
  # When no filename is supplied, an anonymous buffer is created.
  # These buffer's will be named [unnamed-1], [unnamed-2], etc.  and
  # will not have a file name associated with them.
  #
  # When a name is supplied as an argument to new or edit, a check is
  # made to see if the name is the same as an already existing buffer.
  # If so, that buffer is returned.  Otherwise, the name is taken to
  # be a file name.  If the file exists, it is opened and read into
  # the newly created buffer.  If the file does not exist, a new
  # buffer will be created with the associated file name.  The name of
  # the buffer will be based on the file name.  The file will be
  # created when the buffer is first written out to disk.
  #
  # new and edit are synonyms.  In each case, PKGNAME is Vile::Buffer.
  # There is no difference between Vile::Buffer->new($fname) and
  # $buf->new($fname).  These two different forms are merely provided
  # for convenience.
  #
  # Example:
  #
  #     $Vile::current_buffer = new Vile::Buffer 'makefile';
  #                                     # open makefile and make it visible
  #                                     # on the screen.
  #
  #     $abuf = new Vile::Buffer;       # Create an anonymous buffer
  #     print $abuf "Hello";            # put something in it
  #     Vile->current_buffer($abuf);    # make the anonymous buffer current
  #                                     #   (viewable).
  #
  #     Vile->current_buffer($abuf->edit('makefile'));
  #                                     # Now makefile is the current
  #                                     #   buffer
  #     $abuf->current_buffer(Vile::Buffer->new('makefile'));
  #                                     # Same thing
  #
  # =for html <br><br>
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
	    croak("Too many arguments to %s", GvNAME(CvGV(cv)));

	name = (items == 1) ? NULL : (char *)SvPV(ST(1),PL_na);

	(void) api_edit(name, &newvbp);

	RETVAL = newvbp;

    OUTPUT:
	RETVAL


  #
  # =item print BUFOBJ STR1,..,STRN
  #
  # =item insert BUFOBJ STR1,...,STRN
  #
  # Inserts one or more strings the buffer object at the current
  # position of DOT.  DOT will be left at the end of the strings
  # just inserted.
  #
  # When STDERR or STDOUT are printed to, the output will be
  # directed to the message line.
  #
  # Examples:
  #
  #     print "Hello, world!";          # Print a well known greeting on
  #                                     # the message line.
  #     print $Vile::current_buffer "new text";
  #                                     # put some new text in the current
  #                                     # buffer.
  #
  #     my $passbuf = new Vile::Buffer '/etc/passwd';
  #                                     # Fetch the password file
  #     $passbuf->dot('$$');            # Set the position at the end
  #     print $passbuf "joeuser::1000:100:Joe User:/home/joeuser:/bin/bash
  #                                     # Add 'joeuser' to the this buffer
  #     Vile->current_buffer($passbuf); # Make it visible to the user.
  #
  # =for html <br><br>
  #

void
PRINT(vbp, ...)
    VileBuf *vbp

    ALIAS:
	insert = 1

    PREINIT:
	STRLEN ofs_len;
	STRLEN ors_len;
	char *ofs_str = SvPV(ofs_sv, ofs_len);
	char *ors_str = SvPV(ors_sv, ors_len);

    CODE:
	if (is_delinked_bp(vbp2bp(vbp))) {
	    if (items > 0) {
		SV *tmp = newSVsv(ST(1));
		int i;

		for (i = 2; i < items; i++) {
		    if (ofs_len > 0)
			sv_catpvn(tmp, ofs_str, ofs_len);

		    sv_catsv(tmp, ST(i));
		}

		if (write_message("", tmp))
		    use_ml_as_prompt = 1;

		SvREFCNT_dec(tmp);
	    }
	}
	else {
	    int i;
	    for (i = 1; i < items; ) {
		STRLEN len;
		char *arg = SvPV(ST(i), len);
		api_dotinsert(vbp, arg, (int) len);
		i++;
		if (i < items && ofs_len > 0)
		    api_dotinsert(vbp, ofs_str, (int) ofs_len);
	    }

	    if (ors_len)
		api_dotinsert(vbp, ors_str, (int) ors_len);
	}

  #
  # =item set_region BUFOBJ
  #
  # =item set_region BUFOBJ MOTIONSTR
  #
  # =item set_region BUFOBJ STARTLINE, ENDLINE
  #
  # =item set_region BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET
  #
  # =item set_region BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET, 'rectangle'
  #
  # =item set_region BUFOBJ STARTLINE, STARTOFFSET, ENDLINE, ENDOFFSET, 'exact'
  #
  # Sets the region upon which certain other methods will operate and
  # sets DOT to the beginning of the region.
  #
  # Either the line number or offset (or both) may be the special
  # string '$' which represents the last line in the buffer and the
  # last character on a line.
  #
  # Often times, however, the special string '$$' will be more useful.
  # It truly represents the farthest that it possible to go in both
  # the vertical and horizontal directions.  As a line number, this
  # represents the line beyond the last line of the buffer.
  # Characters inserted at this point will form a new line.  As an
  # offset, '$$' refers to the newline character at the end of a line.
  # Characters inserted at this point will be inserted before the
  # newline character.
  #
  # When used in an array context, returns a five element array with
  # the start line, start offset, end line, end offset, and a string
  # indicating the type of region (one of 'line', 'rectangle', or
  # 'exact').
  #
  # When used in a scalar context, returns the buffer object so that
  # cascading method calls may be performed, i.e,
  #
  #     $Vile::current_buffer->set_region(3,4)
  #                          ->attribute_cntl_a_sequences;
  #
  # There is a special form of set_region which may be used as follows:
  #
  #     $Vile::current_buffer->set_region('j2w');
  #
  # The above statement will set the region beginning at the current
  # location of DOT and ending at the location arrived at by moving
  # down one line and over two words.  This may be viewed as a
  # shorthand way of expressing the following (somewhat cumbersome)
  # statement:
  #
  #     $Vile::current_buffer->set_region(
  #             $Vile::current_buffer->motion('j2w'));
  #
  # Notes:
  #
  # =over 4
  #
  # =item *
  #
  # rectangular regions are not implemented yet.
  #
  # =item *
  #
  # setregion is an alias for set_region.
  #
  # =back
  #
  # =for html <br><br>
  #

void
set_region(vbp, ...)
    VileBuf *vbp

    ALIAS:
	setregion = 1

    PREINIT:
	I32 gimme;
	char *shapestr;

    PPCODE:
	api_setup_fake_win(vbp, TRUE);
	switch (items) {
	    case 1:
		/* set DOT and recompute region */
		DOT = vbp->region.r_orig;
		MK  = vbp->region.r_end;
		regionshape = vbp->regionshape;
		break;
	    case 2: {
		/* Set up a "motion" region */
		vbp->region.r_orig = DOT;	/* Remember DOT */
		if (api_motion(vbp, SvPV(ST(1), PL_na))) {
		    /* DOT is now at the other end of the motion */
		    MK = vbp->region.r_orig;	/* Put remembered DOT in MK */
		    regionshape = rgn_EXACT;
		}
		else {
		    croak("set_region: Invalid motion");
		}
		break;
	    }
	    case 3:
		/* Set up a full line region */
		regionshape = rgn_FULLLINE;
		api_gotoline(vbp, sv2linenum(ST(2)));
		MK = DOT;
		api_gotoline(vbp, sv2linenum(ST(1)));
		break;
	    case 5:
		/* Set up an exact region */
		regionshape = rgn_EXACT;
		goto set_region_common;
	    case 6:
		/* Set up any kind of region (exact, fullline, or rectangle) */
		shapestr = SvPV(ST(5), PL_na);
		if (strcmp(shapestr, "exact"))
		    regionshape = rgn_EXACT;
		else if (strcmp(shapestr, "rectangle"))
		    regionshape = rgn_RECTANGLE;
		else if (strcmp(shapestr, "fullline"))
		    regionshape = rgn_FULLLINE;
		else {
		    croak("Region shape argument not one of \"exact\", \"fullline\", or \"rectangle\"");
		}
	    set_region_common:
		api_gotoline(vbp, sv2linenum(ST(3)));
		DOT.o = sv2offset(ST(4));
		MK = DOT;
		api_gotoline(vbp, sv2linenum(ST(1)));
		DOT.o = sv2offset(ST(2));
		break;
	    default:
		croak("Invalid number of arguments to set_region");
		break;
	}
	haveregion = NULL;
	if (getregion(curbp, &vbp->region) != TRUE) {
	    croak("set_region: Unable to set the region");
	}
	if (is_header_line(vbp->region.r_end, curbp)
	    && !b_val(curbp, MDNEWLINE))
		vbp->region.r_size -= (B_COUNT) len_record_sep(curbp);
	IoLINES(GvIO((GV*)vbp->perl_handle)) = 0;  /* reset $. */
	vbp->regionshape = regionshape;
	DOT = vbp->region.r_orig;
	vbp->dot_inited = 1;
	gimme = GIMME_V;
	if (gimme == G_SCALAR) {
	    XPUSHs(ST(0));
	}
	else if (gimme == G_ARRAY) {
	    /* Return range information */
	    XPUSHs(sv_2mortal(newSViv((IV)line_no(curbp, vbp->region.r_orig.l))));
	    XPUSHs(sv_2mortal(newSViv((IV)vbp->region.r_orig.o)));
	    XPUSHs(sv_2mortal(newSViv((IV)line_no(curbp, vbp->region.r_end.l)
						     - (vbp->regionshape == rgn_FULLLINE))));
	    XPUSHs(sv_2mortal(newSViv((IV)vbp->region.r_end.o)));
	    XPUSHs(sv_2mortal(newSVpv(
		vbp->regionshape == rgn_FULLLINE ? "fullline" :
		vbp->regionshape == rgn_EXACT    ? "exact"
						 : "rectangle",
		(size_t) 0 )));
	}


  #
  # =item unmark
  #
  # Clear the "modified" status of the buffer.
  #
  # Returns the buffer object.
  #
  # =for html <br><br>
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

MODULE = Vile	PACKAGE = Vile::Window

  # =back
  #
  # =head2 Package Vile::Window
  #
  # The Vile::Window package contains methods for manipulating windows
  # in various ways.  For the purposes of this discussion, a window is
  # one of the areas of the screen in which a portion of a buffer may
  # be viewed by the user.
  #
  # This API allows you to do the following things to one of these
  # windows:
  #
  # =over 4
  #
  # =item *
  #
  # Create new windows (by splitting an existing window)
  #
  # =item *
  #
  # Delete windows
  #
  # =item *
  #
  # Obtain the buffer (Vile::Buffer) object associated
  # with a given window
  #
  # =item *
  #
  # Change the buffer associated with a window
  #
  # =item *
  #
  # Obtain list of all windows
  #
  # =item *
  #
  # Obtain window characteristics (width, height)
  #
  # =item *
  #
  # Change window characteristics (height)
  #
  # =item *
  #
  # Get/Set DOT for buffer associated with the window
  #
  # =item *
  #
  # Get/Set top line
  #
  # =back
  #
  # In the documentation below, WINOBJ refers to an object of
  # Vile::Window and BUFOBJ refers to an object of Vile::Buffer.
  #
  # =head2 Package Vile::Window Methods
  #
  # =over 4
  #

  #
  # =item buffer WINOBJ
  #
  # Returns the buffer associated with WINOBJ.
  #
  # E.g,
  #
  #     $buf = Vile::current_window->buffer
  #
  # would get you the buffer associated with the current window.
  #
  # =for html <br><br>
  #
  # =item buffer WINOBJ BUFOBJ
  #
  # Sets the buffer associated with WINOBJ to BUFOBJ.  Returns
  # BUFOBJ.
  #
  # =for html <br><br>
  #

VileBuf *
buffer(vw, ...)
    VileWin vw

    CODE:
	if (items > 2)
	    croak("Vile::Window::buffer: Too many arguments.");
	else if (items == 2) {
	    const char *croakmess;
	    VileBuf *vbp;
	    WINDOW *savewp = curwp;
	    vbp = getVB(ST(1), &croakmess);
	    if (vbp == NULL)
		croak("Vile::Window::buffer: %s", croakmess);
	    set_curwp(vw);
	    swbuffer_lfl(vbp2bp(vbp), FALSE, TRUE);
	    curwp = savewp;
	    curbp = curwp->w_bufp;
	}

	RETVAL = api_bp2vbp(vw->w_bufp);

    OUTPUT:
	RETVAL

  #
  # =item current_window
  #
  # Returns the Vile::Window object representing the current window.
  #
  # Note: This method is also in the Vile:: package.
  #
  # =for html <br><br>
  #
  # =item current_window WINOBJ
  #
  # Sets the current window (window with focus) to WINOBJ; Returns
  # WINOBJ.
  #
  # Note:  You'd say
  #
  #     $curwin = Vile::current_window;
  #
  # to retrieve the current window and
  #
  #     $mywin->current_window;
  #
  # to set it.
  #
  # =for html <br><br>
  #

VileWin
current_window(...)

    ALIAS:
	Vile::current_window = 1
    PREINIT:
	int argno;

    CODE:
	argno = 0;

	if (strcmp(SvPV(ST(argno), PL_na), "Vile") == 0)
	    argno++;

	if (items - argno == 0) { /* getter */
	    RETVAL = curwp_visible ? curwp_visible : curwp;
	}
	else if (items - argno == 1) { /* setter */
	    VileWin vw;
	    const char *croakmess;
	    /* Need a window object */
	    vw = getVW(ST(argno), &croakmess);

	    if (vw == NULL)
		croak("Vile::%scurrent_window: %s",
		      ix == 1 ? "Window::" : "",
		      croakmess);
	    set_curwp(vw);
	    curwp_visible = curwp;
	    RETVAL = curwp;
	}
	else {
	    croak("Vile::%scurrent_window:  Incorrect number of arguments",
		  ix == 1 ? "Window::" : "");
	}

    OUTPUT:
	RETVAL

  #
  # =item delete WINOBJ
  #
  # Removes the window in question.  Screen real estate allocated to
  # the window will be returned to the window from whence it came.
  #
  # Returns 1 if successful, undef otherwise.
  #
  # =for html <br><br>
  #

void
delete(vw)
    VileWin vw

    PREINIT:
	WINDOW *wp;
	int count;

    PPCODE:
	/* See how many visible windows remain.  We can't simply test to
	   see if wheadp->w_wndp is NULL because, there may be some fake
	   windows pushed for Perl's purposes.  So we actually have to
	   count them. */
	count = 0;
	for_each_visible_window(wp) {
	    count++;
	}
	if (count <= 1 || !delwp(vw)) {
	    XPUSHs(&PL_sv_undef);
	}
	else {
	    XPUSHs(sv_2mortal(newSViv((IV)1)));
	}

  #
  # =item dot WINOBJ
  #
  # =item current_position WINOBJ
  #
  # Retrieves DOT (the current position) for the current window.
  # In a scalar context, only the line number is returned.  In
  # an array context, a list containing both the line number and
  # offset within the line are returned.
  #
  # =for html <br><br>
  #
  # =item dot WINOBJ LINENUM, OFFSET
  #
  # =item current_position WINOBJ LINENUM, OFFSET
  #
  # Sets DOT (the current position) to the indicated values.
  #
  # When used in a scalar context, returns the line number.  When
  # used in a list context, returns both the line number and the
  # offset with in the line.
  #
  # Note: dot and current_position are aliases for each other.  Neither
  # provides any additional functionality over the other.
  #
  # =for html <br><br>
  #

void
dot(vw, ...)
    VileWin vw

    ALIAS:
	current_position = 1

    PREINIT:
	I32 gimme;

    PPCODE:
	if (items > 3) {
	    croak("Vile::Window::%s: Too many arguments",
		  ix == 1 ? "current_position" : "dot");
	}
	else if (items > 1) {
	    /* Setter: expect a line spec and possibly an offset */
	    WINDOW *savewp = set_curwp0(vw);
	    gotoline(TRUE, sv2linenum(ST(1)));

	    if (items == 3)
		DOT.o = sv2offset(ST(2));

	    set_curwp0(savewp);
	}
	gimme = GIMME_V;
	if (gimme == G_SCALAR || gimme == G_ARRAY) {
	    /* Return line number when in either a scalar or an array context */
	    XPUSHs(sv_2mortal(newSViv(
		(IV)line_no(vw->w_bufp, vw->w_traits.w_dt.l))));
	}
	if (gimme == G_ARRAY) {
	    /* When in an array context, also return the line offset */
	    XPUSHs(sv_2mortal(newSViv((IV)vw->w_traits.w_dt.o)));
	}

  #
  # =item index WINOBJ
  #
  # Returns the index of WINOBJ.  This will be a small integer, with
  # 0 representing the first (top-most) window, 1 representing the
  # window below it, and so on.
  #
  # =for html <br><br>
  #

int
index(win)
    VileWin win;

    CODE:
	RETVAL = win2index(win);

    OUTPUT:
	RETVAL

  #
  # =item new Vile::Window
  #
  # Allocates and returns a new window.  The editor will choose
  # where the window will be located.  (It will likely choose a
  # large window to split.)  If a new window could not be allocated,
  # return undef.
  #
  # =for html <br><br>
  #
  # =item new Vile::Window BUFOBJ
  #
  # Like above, but associate BUFOBJ with the new window.
  #
  # =for html <br><br>
  #
  # =item new WINOBJ
  #
  # Allocate and return a new window, using WINOBJ as the window
  # to split.  If this cannot be done, return undef.  If the split
  # is possible, the new window returned will be located below
  # WINOBJ.
  #
  # =for html <br><br>
  #
  # =item new WINOBJ BUFOBJ
  #
  # Like above, but associate BUFOBJ with the new window.
  #
  # =for html <br><br>
  #

VileWin
new(...)

    PREINIT:
	VileBuf *vbp;
	VileWin vw;

    CODE:
	if (items == 2) {
	    const char *croakmess;
	    vbp = getVB(ST(1), &croakmess);
	    if (!vbp)
		croak("Vile::Window::new: %s",croakmess);
	}
	else
	    vbp = NULL;

	if (items > 2)
	    croak("Vile::Window::new: Too many arguments.");
	else if (items < 1)
	    croak("Vile::Window::new: Too few arguments.");
	else if (strcmp(SvPV(ST(0),PL_na), "Vile::Window") == 0) {
	    vw = wpopup();
	}
	else if (sv_isa(ST(0), "Vile::Window")) {
	    const char *croakmess;
	    vw = getVW(ST(0), &croakmess);	/* Fetch window object */
	    if (vw == NULL)
		croak("Vile::Window::new: %s", croakmess);

	    if (vw->w_ntrows < MINWLNS)
		vw = NULL;		/* Can't split */
	    else {
		WINDOW *save_wp = set_curwp0(vw);
		splitwind(TRUE, 1);	/* split window forcing new
					   window to be on bottom */
		vw = vw->w_wndp;
		set_curwp0(save_wp);
	    }
	}
	else
	    croak("Vile::Window::new: Incorrect type for first argument");

	if (vw && vbp) {
	    WINDOW *savewp = set_curwp0(vw);
	    swbuffer_lfl(vbp2bp(vbp), FALSE, TRUE);
	    set_curwp0(savewp);
	}

	RETVAL = vw;

    OUTPUT:
	RETVAL

  #
  # =item size WINOBJ
  #
  # In a scalar context, return the height of a window.  In a list
  # context, return both the height and width.
  #
  # =for html <br><br>
  #
  # =item size WINOBJ HEIGHT
  #
  # Set the height of a window.  It will attempt to change a windows
  # size by either adding or stealing lines from the window below.
  # (This means that the bottommost window can't be directly changed
  # since it doesn't have a window below it.)
  #
  # Returns the new size of the window.
  #
  # =for html <br><br>
  #
  # =item size WINOBJ HEIGHT WIDTH
  #
  # Unimplemented.  It is not possible to change the width at the
  # moment, but if the feature ever becomes available, this method
  # will do it.
  #
  # =for html <br><br>
  #

void
size(vw, ...)
    VileWin vw

    PREINIT:
	I32 gimme;

    PPCODE:
	if (items > 2) {
	    croak("Vile::Window::size: Invalid number of arguments");
	}
	else if (items == 2) {
	    WINDOW *savewp;
	    int newheight = SvIV(ST(1));
	    int maxheight;

	    if (newheight < 1)
		croak("Vile::Window::size: New height must be 1 or larger");
	    if (vw->w_wndp == NULL)
		croak("Vile::Window::size: Can't change bottom window");
	    maxheight = vw->w_ntrows + vw->w_wndp->w_ntrows - 1;
	    if (newheight > maxheight)
		newheight = maxheight;

	    savewp = set_curwp0(vw);
	    resize(TRUE, newheight);
	    set_curwp0(savewp);
	}
	gimme = GIMME_V;
	if (gimme == G_SCALAR) {
	    XPUSHs(sv_2mortal(newSViv((IV)vw->w_ntrows)));
	}
	else if (gimme == G_ARRAY) {
	    XPUSHs(sv_2mortal(newSViv((IV)vw->w_ntrows)));
	    XPUSHs(sv_2mortal(newSViv((IV)term.cols)));
	}

  #
  # =item topline WINOBJ
  #
  # Returns the line number of the top line in the window.
  #
  # =for html <br><br>
  #

int
topline(vw, ...)
    VileWin vw

    CODE:
	if (items > 2) {
	    croak("Vile::Window::topline: Too many arguments");
	}
	else if (items > 1) {
	    /* Setter: expect a line spec */
	    WINDOW *savewp = set_curwp0(vw);
	    int lcur;
	    lcur = line_no(curwp->w_bufp, curwp->w_line.l);
	    mvupwind(TRUE, lcur - sv2linenum(ST(1)));
	    set_curwp0(savewp);
	}
	RETVAL = line_no(vw->w_bufp, vw->w_line.l);

    OUTPUT:
	RETVAL

  #
  # =item window_at N
  #
  # Returns the Nth (starting at 0 from the top-most window) Vile::Window
  # object.  If there is no Nth window, undef is returned instead.
  #
  # Note: This method also appears in the Vile:: package.
  #
  # =for html <br><br>
  #

VileWin
window_at(idx)
    int idx

    ALIAS:
	Vile::window_at = 1

    CODE:
	RETVAL = index2win(idx);

    OUTPUT:
	RETVAL

  #
  # =item window_count
  #
  # Returns number of (visible) windows.
  #
  # Note:  Non-visible windows are used to represent buffers
  # for the perl API.  They are also used for other purposes in
  # which modification of a buffer is desired, but disturbing
  # the position of the buffer within one of its windows is
  # not.
  #
  # Note: This method also appears in the Vile:: package.
  #
  # =for html <br><br>
  #
  # =back

int
window_count()

    ALIAS:
	Vile::window_count = 1

    PREINIT:
	int count;
	WINDOW *wp;

    CODE:
	count = 0;
	for_each_visible_window(wp)
	    count++;
	RETVAL = count;

    OUTPUT:
	RETVAL
