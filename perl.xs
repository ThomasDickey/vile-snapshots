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

static PerlInterpreter *perl_interp;

static void xs_init(void);

int
perl(int f GCC_UNUSED, int n GCC_UNUSED)
{
    register int status;
    char buf[NLINE];	/* buffer to receive command into */

    buf[0] = EOS;
    if ((status = mlreply_no_opts("Perl command: ", buf, sizeof(buf))) != TRUE)
	    return(status);

    if (perl_interp || perl_init()) {
	SV *sv = newSVpv(buf, 0);
	perl_eval_sv(sv, G_DISCARD | G_NOARGS);
	SvREFCNT_dec(sv);
	return TRUE;
    }
    else
	return FALSE;
}

int perl_init(void)
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
    //perl_run(perl_interp);

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

// typedef void *	VI;

MODULE = VI	PACKAGE = VI

# msg --
#	Set the message line to text.
#
# Perl Command: VI::Msg
# Usage: VI::Msg screenId text

void
Msg(text)
	char *      text
 
	ALIAS:
	PRINT = 1

	CODE:
	mlforce("%s", text);


void
hello()
	CODE:
	printf("Hello, world!\n");
