/*
 * The functions in this file negotiate with the operating system for
 * characters, and write characters in a barely buffered fashion on the display.
 * All operating systems.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/termio.c,v 1.152 1997/12/01 23:44:03 tom Exp $
 *
 */
#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#if CC_DJGPP
# include <pc.h>   /* for kbhit() */
#endif

#if SYS_VMS
#include <starlet.h>
#include <lib$routines.h>
#endif

#if SYS_UNIX

static void ttmiscinit (void);

/* there are three copies of the tt...() routines here -- one each for
	POSIX termios, traditional termio, and sgtty.  If you have a
	choice, I recommend them in that order. */

/* ttopen() and ttclose() are responsible for putting the terminal in raw
	mode, setting up terminal signals, etc.
   ttclean() prepares the terminal for shell escapes, and ttunclean() gets
	us back into vile's mode
*/

/* I suppose this config stuff should move to estruct.h */

#if HAVE_TERMIOS_H && HAVE_TCGETATTR
/* Note: <termios.h> is available on some systems, but in order to use it
 * a special library needs to be linked in.  This is the case on the NeXT
 * where libposix.a needs to be linked in.  Unfortunately libposix.a is buggy.
 * So we have the configuration script test to make sure that tcgetattr is
 * available through the standard set of libraries in order to help us
 * determine whether or not to use <termios.h>.
 */
# define USE_POSIX_TERMIOS 1
# define USE_FCNTL 1
#else
# if HAVE_TERMIO_H
#  define USE_TERMIO 1
#  define USE_FCNTL 1
# else
#  if HAVE_SGTTY_H
#   define USE_SGTTY 1
#   define USE_FIONREAD 1
#  else
 huh?
#  endif
# endif
#endif

/* FIXME: There used to be code here which dealt with OSF1 not working right
 * with termios.  We will have to determine if this is still the case and
 * add code here to deal with it if so.
 */

#if !defined(FIONREAD)
/* try harder to get it */
# if HAVE_SYS_FILIO_H
#  include "sys/filio.h"
# else /* if you have trouble including ioctl.h, try "sys/ioctl.h" instead */
#  if HAVE_IOCTL_H
#   include <ioctl.h>
#  else
#   if HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#   endif
#  endif
# endif
#endif

#if DISP_X11	/* don't use either one */
# undef USE_FCNTL
# undef USE_FIONREAD
#else
# if defined(FIONREAD)
  /* there seems to be a bug in someone's implementation of fcntl -- it
   * causes output to be flushed if you change to ndelay input while output
   * is pending.  for these systems, we use FIONREAD instead, if possible.
   * In fact, if try and use FIONREAD in any case if present.  If you have
   * the problem with fcntl, you'll notice that the screen doesn't always
   * refresh correctly, as if the fcntl is interfering with the output drain
   */
#  undef USE_FCNTL
#  define USE_FIONREAD 1
# endif
#endif

#if USE_FCNTL
/* this is used to determine whether input is pending from the user */
#include	<fcntl.h>
int kbd_flags;			/* saved keyboard fcntl flags	*/
int kbd_is_polled;		/* in O_NDELAY mode?		*/
int kbd_char_present;		/* there is a char in kbd_char	*/
char kbd_char;			/* the char we've already read	*/
#endif

#define SMALL_STDOUT 1

#if defined(SMALL_STDOUT) && (defined (USE_FCNTL) || defined(USE_FIONREAD))
#define TBUFSIZ 128 /* Provide a smaller terminal output buffer so that
	   the type-ahead detection works better (more often).
	   That is, we overlap screen writing with more keyboard polling */
#else
#define TBUFSIZ 1024	/* reduces the number of writes */
#endif

#if USE_POSIX_TERMIOS

#include <termios.h>

#if SYSTEM_LOOKS_LIKE_SCO
/* they neglected to define struct winsize in termios.h -- it's only
   in termio.h	*/
#include	<sys/stream.h>
#include	<sys/ptem.h>
#endif

#ifndef VDISABLE
# ifdef	_POSIX_VDISABLE
#  define VDISABLE _POSIX_VDISABLE
# else
#  define VDISABLE '\0'
# endif
#endif

struct termios otermios, ntermios;

char tobuf[TBUFSIZ];		/* terminal output buffer */

void
ttopen(void)
{
	int s;
	s = tcgetattr(0, &otermios);
	if (s < 0) {
		perror("ttopen tcgetattr");
		ExitProgram(BADEXIT);
	}
#if !DISP_X11
#if HAVE_SETVBUF
# if SETVBUF_REVERSED
	setvbuf(stdout, _IOFBF, tobuf, TBUFSIZ);
# else
	setvbuf(stdout, tobuf, _IOFBF, TBUFSIZ);
# endif
#else /* !HAVE_SETVBUF */
	setbuffer(stdout, tobuf, TBUFSIZ);
#endif /* !HAVE_SETVBUF */
#endif /* !DISP_X11 */

	suspc =   otermios.c_cc[VSUSP];
	intrc =   otermios.c_cc[VINTR];
	killc =   otermios.c_cc[VKILL];
	startc =  otermios.c_cc[VSTART];
	stopc =   otermios.c_cc[VSTOP];
	backspc = otermios.c_cc[VERASE];
#ifdef VWERASE	/* Sun has it.	any others? */
	wkillc = otermios.c_cc[VWERASE];
#else
	wkillc =  tocntrl('W');
#endif

	/* this could probably be done more POSIX'ish? */
#if OPT_SHELL && defined(SIGTSTP) && defined(SIGCONT)
	setup_handler(SIGTSTP,SIG_DFL); 	/* set signals so that we can */
	setup_handler(SIGCONT,rtfrmshell);	/* suspend & restart */
#endif
#ifdef SIGTTOU
	setup_handler(SIGTTOU,SIG_IGN); 	/* ignore output prevention */
#endif

#if USE_FCNTL
	kbd_flags = fcntl( 0, F_GETFL, 0 );
	kbd_is_polled = FALSE;
#endif

#if ! DISP_X11
	ntermios = otermios;

	/* new input settings: turn off crnl mapping, cr-ignoring,
	 * case-conversion, and allow BREAK
	 */
	ntermios.c_iflag = BRKINT | (otermios.c_iflag &
		(unsigned long) ~(INLCR | IGNCR | ICRNL
#ifdef IUCLC
					| IUCLC
#endif
				 ));

	ntermios.c_oflag = 0;
	ntermios.c_lflag = ISIG;
	ntermios.c_cc[VMIN] = 1;
	ntermios.c_cc[VTIME] = 0;
#ifdef	VSWTCH
	ntermios.c_cc[VSWTCH] = VDISABLE;
#endif
	ntermios.c_cc[VSUSP]  = VDISABLE;
#if defined (VDSUSP) && defined(NCCS) && VDSUSP < NCCS
	ntermios.c_cc[VDSUSP]  = VDISABLE;
#endif
	ntermios.c_cc[VSTART] = VDISABLE;
	ntermios.c_cc[VSTOP]  = VDISABLE;
#endif /* ! DISP_X11 */

	ttmiscinit();

	ttunclean();
}

/* we disable the flow control chars so we can use ^S as a command, but
  some folks still need it.  they can put "flow-control-enable" in their
  .vilerc
  no argument:	re-enable ^S/^Q processing in the driver
  with arg:	disable it
*/
/* ARGSUSED */
int
flow_control_enable(int f, int n GCC_UNUSED)
{
#if !DISP_X11
	if (!f) {
		ntermios.c_cc[VSTART] = (char)startc;
		ntermios.c_cc[VSTOP]  = (char)stopc;
	} else {
		ntermios.c_cc[VSTART] = VDISABLE;
		ntermios.c_cc[VSTOP]  = VDISABLE;
	}
	ttunclean();
#endif
	return TRUE;
}

void
ttclose(void)
{
	ttclean(TRUE);
}

/*
 * Clean up in anticipation for a return to the
 * operating system. Move down to the last line and advance, to make room
 * for the system prompt. Shut down the channel to the
 * terminal.
 */
/*ARGSUSED*/
void
ttclean(int f)
{
#if !DISP_X11
	if (f)
		kbd_openup();

	(void)fflush(stdout);
	tcdrain(1);
	tcsetattr(0, TCSADRAIN, &otermios);
	TTflush();
	TTclose();
	TTkclose();
#if USE_FCNTL
	fcntl(0, F_SETFL, kbd_flags);
	kbd_is_polled = FALSE;
#endif
#endif
}

void
ttunclean(void)
{
#if ! DISP_X11
	tcdrain(1);
	tcsetattr(0, TCSADRAIN, &ntermios);
#endif
}


#endif /* USE_POSIX_TERMIOS */

#if USE_TERMIO

#include	<termio.h>

/* original terminal characteristics and characteristics to use inside */
struct	termio	otermio, ntermio;

#ifdef AVAILABLE  /* setbuffer() isn't on most termio systems */
char tobuf[TBUFSIZ];		/* terminal output buffer */
#endif

void
ttopen(void)
{

	ioctl(0, TCGETA, (char *)&otermio);	/* save old settings */
#if defined(AVAILABLE) && !DISP_X11
	setbuffer(stdout, tobuf, TBUFSIZ);
#endif

	intrc =   otermio.c_cc[VINTR];
	killc =   otermio.c_cc[VKILL];
	startc =  tocntrl('Q');
	stopc =   tocntrl('S');
	backspc = otermio.c_cc[VERASE];
	wkillc =  tocntrl('W');

#if USE_FCNTL
	kbd_flags = fcntl( 0, F_GETFL, 0 );
	kbd_is_polled = FALSE;
#endif

#if SIGTSTP
/* be careful here -- VSUSP is sometimes out of the range of the c_cc array */
#ifdef V_SUSP
	ntermio.c_cc[V_SUSP] = -1;
	suspc = otermio.c_cc[V_SUSP];
#else
	suspc = -1;
#endif
#ifdef V_DSUSP
	ntermio.c_cc[V_DSUSP] = -1;
#endif

#if OPT_SHELL
	setup_handler(SIGTSTP,SIG_DFL); 	/* set signals so that we can */
	setup_handler(SIGCONT,rtfrmshell);	/* suspend & restart */
#endif
	setup_handler(SIGTTOU,SIG_IGN); 	/* ignore output prevention */

#else /* no SIGTSTP */
	suspc =   tocntrl('Z');
#endif

#if ! DISP_X11
	ntermio = otermio;

	/* setup new settings, allow BREAK */
	ntermio.c_iflag = BRKINT;
	ntermio.c_oflag = 0;
	ntermio.c_lflag = ISIG;
	ntermio.c_cc[VMIN] = 1;
	ntermio.c_cc[VTIME] = 0;
#ifdef	VSWTCH
	ntermio.c_cc[VSWTCH] = -1;
#endif
#endif

	ttmiscinit();
	ttunclean();

}

/* we disable the flow control chars so we can use ^S as a command, but
  some folks still need it.  they can put "flow-control-enable" in their
  .vilerc */
/* ARGSUSED */
int
flow_control_enable(int f, int n)
{
#if !DISP_X11
	ntermio.c_iflag &= ~(IXON|IXANY|IXOFF);
	if (!f)
		ntermio.c_iflag |= otermio.c_iflag & (IXON|IXANY|IXOFF);
	ttunclean();
#endif
	return TRUE;
}

void
ttclose(void)
{
	ttclean(TRUE);
}

void
ttclean(int f)
{
#if ! DISP_X11
	if (f)
		kbd_openup();

	(void)fflush(stdout);
	TTflush();
	TTclose();
	TTkclose();	/* xterm */
	ioctl(0, TCSETAF, (char *)&otermio);
#if USE_FCNTL
	fcntl(0, F_SETFL, kbd_flags);
	kbd_is_polled = FALSE;
#endif
#endif	/* DISP_X11 */
}

void
ttunclean(void)
{
#if ! DISP_X11
	ioctl(0, TCSETAW, (char *)&ntermio);
#endif
}

#endif /* USE_TERMIO */

#if USE_SGTTY

#if USE_FIONREAD
char tobuf[TBUFSIZ];		/* terminal output buffer */
#endif

#undef	CTRL
#include	<sgtty.h>	 /* for stty/gtty functions */
struct	sgttyb	ostate; 	 /* saved tty state */
struct	sgttyb	nstate; 	 /* values for editor mode */
struct	sgttyb	rnstate;	  /* values for raw editor mode */
int olstate;		/* Saved local mode values */
int nlstate;		/* new local mode values */
struct ltchars	oltchars;	/* Saved terminal special character set */
struct ltchars	nltchars = { -1, -1, -1, -1, -1, -1 }; /* a lot of nothing */
struct tchars	otchars;	/* Saved terminal special character set */
struct tchars	ntchars; /*  = { -1, -1, -1, -1, -1, -1 }; */

void
ttopen(void)
{
	ioctl(0,TIOCGETP,(char *)&ostate); /* save old state */
	killc = ostate.sg_kill;
	backspc = ostate.sg_erase;

#if ! DISP_X11
	nstate = ostate;
	nstate.sg_flags |= CBREAK;
	nstate.sg_flags &= ~(ECHO|CRMOD);	/* no echo for now... */
	ioctl(0,TIOCSETN,(char *)&nstate); /* set new state */
#endif

	rnstate = nstate;
	rnstate.sg_flags &= ~CBREAK;
	rnstate.sg_flags |= RAW;

	ioctl(0, TIOCGETC, (char *)&otchars);	/* Save old characters */
	intrc =  otchars.t_intrc;
	startc = otchars.t_startc;
	stopc =  otchars.t_stopc;

#if ! DISP_X11
	ntchars = otchars;
	ntchars.t_brkc = -1;
	ntchars.t_eofc = -1;
	ntchars.t_startc = -1;
	ntchars.t_stopc = -1;
	ioctl(0, TIOCSETC, (char *)&ntchars);	/* Place new character into K */
#endif

	ioctl(0, TIOCGLTC, (char *)&oltchars);	/* Save old characters */
	wkillc = oltchars.t_werasc;
	suspc = oltchars.t_suspc;
#if ! DISP_X11
	ioctl(0, TIOCSLTC, (char *)&nltchars);	/* Place new character into K */
#endif

#ifdef	TIOCLGET
	ioctl(0, TIOCLGET, (char *)&olstate);
#if ! DISP_X11
	nlstate = olstate;
	nlstate |= LLITOUT;
	ioctl(0, TIOCLSET, (char *)&nlstate);
#endif
#endif
#if USE_FIONREAD
	setbuffer(stdout, tobuf, TBUFSIZ);
#endif
#if ! DISP_X11

#if OPT_SHELL
	setup_handler(SIGTSTP,SIG_DFL); 	/* set signals so that we can */
	setup_handler(SIGCONT,rtfrmshell);	/* suspend & restart */
#endif
	setup_handler(SIGTTOU,SIG_IGN); 	/* ignore output prevention */

#endif

	ttmiscinit();
}

/* we disable the flow control chars so we can use ^S as a command, but
  some folks still need it.  they can put "flow-control-enable" in their
  .vilerc */
/* ARGSUSED */
int
flow_control_enable(int f, int n)
{
#if !DISP_X11
	if (!f) {
		ntchars.t_startc = startc;
		ntchars.t_stopc = stopc;
	} else {
		ntchars.t_startc = -1;
		ntchars.t_stopc = -1;
	}
	ttunclean();
#endif
	return TRUE;
}

void
ttclose(void)
{
	ttclean(TRUE);
}

void
ttclean(int f)
{
#if ! DISP_X11
	if (f)
		kbd_openup();

	TTflush();
	TTclose();
	TTkclose();	/* xterm */
	ioctl(0, TIOCSETN, (char *)&ostate);
	ioctl(0, TIOCSETC, (char *)&otchars);
	ioctl(0, TIOCSLTC, (char *)&oltchars);
#ifdef	TIOCLSET
	ioctl(0, TIOCLSET, (char *)&olstate);
#endif
#if SYS_APOLLO
	TTflush();
#endif
#endif
}

void
ttunclean(void)
{
#if ! DISP_X11
#if SYS_APOLLO
	int literal = LLITOUT;

	(void)fflush(stdout);
	ioctl(0, TIOCLSET, (caddr_t)&olstate);
	ioctl(0, TIOCSETP, (caddr_t)&nstate);	/* setting nlstate changes sb_flags */
	TTflush();
	ioctl(0, TIOCLBIS, (caddr_t)&literal);	/* set this before nltchars! */
	ioctl(0, TIOCSETC, (caddr_t)&ntchars);
	ioctl(0, TIOCSLTC, (caddr_t)&nltchars);

#else

	ioctl(0, TIOCSETN, (char *)&nstate);
	ioctl(0, TIOCSETC, (char *)&ntchars);
	ioctl(0, TIOCSLTC, (char *)&nltchars);
#ifdef	TIOCLSET
	ioctl(0, TIOCLSET, (char *)&nlstate);
#endif

#endif	/* SYS_APOLLO */
#endif	/* !DISP_X11 */
}

#endif /* USE_SGTTY */

#if !DISP_X11
OUTC_DCL
ttputc(OUTC_ARGS)
{
	OUTC_RET putchar((char)c);
}

void
ttflush(void)
{
	(void)fflush(stdout);
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all.
 */
int
ttgetc(void)
{
#if	USE_FCNTL
	int n;
	if( kbd_char_present ) {
		kbd_char_present = FALSE;
	} else {
		if( kbd_is_polled && fcntl( 0, F_SETFL, kbd_flags ) < 0 )
			imdying(SIGINT);
		kbd_is_polled = FALSE;
		n = read(0, &kbd_char, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				return -1;
			imdying(SIGINT);
		}
	}
	return ( kbd_char );
#else /* USE_FCNTL */
#if SYS_APOLLO
	/*
	 * If we try to read a ^C in cooked mode it will echo anyway.  Also,
	 * the 'getchar()' won't be interruptable.  Setting raw-mode
	 * temporarily here still allows the program to be interrupted when we
	 * are not actually looking for a character.
	 */
	int	c;
	ioctl(0, TIOCSETN, (char *)&rnstate);
	c = getchar();
	ioctl(0, TIOCSETN, (char *)&nstate);
#else
	int c;
	c = getchar();
#endif
	if (c == EOF) {
#ifdef linux
		/*
		 * The command "^X!vile -V" makes vile receive an EOF at this
		 * point.
		 */
		return -1;
#else
		if (errno == EINTR)
			return -1;
		imdying(SIGINT);
#endif
	}
	return c;
#endif
}
#endif /* !DISP_X11 */

/* tttypahead:	Check to see if any characters are already in the
		keyboard buffer
*/
int
tttypahead(void)
{

#if DISP_X11
	return x_typahead(0);
#else

# if	USE_FIONREAD
	{
	long x;
	return((ioctl(0,FIONREAD,(caddr_t)&x) < 0) ? 0 : (int)x);
	}
# else
#  if	USE_FCNTL
	if( !kbd_char_present )
	{
		if( !kbd_is_polled &&
				fcntl(0, F_SETFL, kbd_flags|O_NDELAY ) < 0 )
			return(FALSE);
		kbd_is_polled = TRUE;  /* I think */
		kbd_char_present = (1 == read( 0, &kbd_char, 1 ));
	}
	return ( kbd_char_present );
#  else
	return FALSE;
#  endif/* USE_FCNTL */
# endif/* USE_FIONREAD */
#endif	/* DISP_X11 */
}

/* this takes care of some stuff that's common across all ttopen's.  Some of
	it should arguably be somewhere else, but... */
static void
ttmiscinit(void)
{
	/* make sure backspace is bound to backspace */
	asciitbl[backspc] = &f_backchar_to_bol;

#if !DISP_X11
	/* no buffering on input */
	setbuf(stdin, (char *)0);
#endif
}

#else /* not SYS_UNIX */

#if SYS_MSDOS || SYS_OS2 || SYS_WINNT
# if CC_DJGPP
#  include <gppconio.h>
# else
#  if CC_NEWDOSCC
#   include <conio.h>
#  endif
# endif
#endif



#if	SYS_VMS
#include	<stsdef.h>
#include	<ssdef.h>
#include	<descrip.h>
#include	<iodef.h>
#include	<ttdef.h>
#include	<tt2def.h>

typedef struct	{
	unsigned short int status;	/* I/O completion status */
	unsigned short int count;	/* byte transfer count	 */
	int dev_dep_data;		/* device dependant data */
	} QIO_SB;			/* This is a QIO I/O Status Block */

#define NIBUF	1024			/* Input buffer size		*/
#define NOBUF	1024			/* MM says big buffers win!	*/
#define EFN	0			/* Event flag			*/

char	obuf[NOBUF];			/* Output buffer		*/
int	nobuf;				/* # of bytes in above		*/
char	ibuf[NIBUF];			/* Input buffer 		*/
int	nibuf;				/* # of bytes in above		*/
int	ibufi;				/* Read index			*/
int	oldmode[3];			/* Old TTY mode bits		*/
int	newmode[3];			/* New TTY mode bits		*/
short	iochan; 			/* TTY I/O channel		*/
#endif


#if	SYS_MSDOS && CC_NEWDOSCC && !CC_MSC
union REGS rg;		/* cpu register for use of DOS calls (ibmpc.c) */
int nxtchar = -1;	/* character held from type ahead    */
#endif

void
ttopen(void)
{
#if	SYS_VMS
	struct	dsc$descriptor	idsc;
	struct	dsc$descriptor	odsc;
	char	oname[40];
	QIO_SB	iosb;
	int	status;

	odsc.dsc$a_pointer = "TT";
	odsc.dsc$w_length  = strlen(odsc.dsc$a_pointer);
	odsc.dsc$b_dtype	= DSC$K_DTYPE_T;
	odsc.dsc$b_class	= DSC$K_CLASS_S;
	idsc.dsc$b_dtype	= DSC$K_DTYPE_T;
	idsc.dsc$b_class	= DSC$K_CLASS_S;
	do {
		idsc.dsc$a_pointer = odsc.dsc$a_pointer;
		idsc.dsc$w_length  = odsc.dsc$w_length;
		odsc.dsc$a_pointer = &oname[0];
		odsc.dsc$w_length  = sizeof(oname);
		status = lib$sys_trnlog(&idsc, &odsc.dsc$w_length, &odsc);
		if (status!=SS$_NORMAL && status!=SS$_NOTRAN)
			tidy_exit(status);
		if (oname[0] == 0x1B) {
			odsc.dsc$a_pointer += 4;
			odsc.dsc$w_length  -= 4;
		}
	} while (status == SS$_NORMAL);
	status = sys$assign(&odsc, &iochan, 0, 0);
	if (status != SS$_NORMAL)
		tidy_exit(status);
	status = sys$qiow(EFN, iochan, IO$_SENSEMODE, &iosb, 0, 0,
			  oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL
	 || iosb.status != SS$_NORMAL)
		tidy_exit(status);
	newmode[0] = oldmode[0];
	newmode[1] = oldmode[1];
	newmode[1] &= ~(TT$M_TTSYNC|TT$M_HOSTSYNC);
	newmode[2] = oldmode[2] | TT2$M_PASTHRU;
	status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
			  newmode, sizeof(newmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL
	 || iosb.status != SS$_NORMAL)
		tidy_exit(status);
	term.t_nrow = (newmode[1]>>24);
	term.t_ncol = newmode[0]>>16;

#endif

	/* make sure backspace is bound to backspace */
	asciitbl[backspc] = &f_backchar_to_bol;

}

void
ttclose(void)
{

#if	SYS_VMS
	/*
	 * Note: this code used to check for errors when closing the output,
	 * but it didn't work properly (left the screen set in 1-line mode)
	 * when I was running as system manager, so I took out the error
	 * checking -- T.Dickey 94/7/15.
	 */
	int	status;
	QIO_SB	iosb;

	ttflush();
	status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
		 oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status == SS$_IVCHAN)
		return; /* already closed it */
	(void) sys$dassgn(iochan);
#endif
#if	!SYS_VMS
	ttclean(TRUE);
#endif
}

void
ttclean(int f)
{
	if (f)
		kbd_openup();

	TTflush();
	TTclose();
	TTkclose();
}

void
ttunclean(void)
{
}

/*
 * Write a character to the display. On VMS, terminal output is buffered, and
 * we just put the characters in the big array, after checking for overflow.
 * On CPM terminal I/O unbuffered, so we just write the byte out. Ditto on
 * MS-DOS (use the very very raw console output routine).
 */
OUTC_DCL
ttputc(OUTC_ARGS)
{
#if	SYS_VMS
	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
	OUTC_RET c;
#endif
#if	SYS_OS2 && !DISP_VIO
	OUTC_RET putch(c);
#endif
#if	SYS_MSDOS
# if DISP_IBMPC
	/* unneeded currently -- output is memory-mapped */
# endif
# if DISP_ANSI
	OUTC_RET putchar(c);
# endif
#endif
}


/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
void
ttflush(void)
{
#if	SYS_VMS
	QIO_SB	iosb;

	if (nobuf != 0) {
		(void) sys$qiow(EFN, iochan, IO$_WRITELBLK|IO$M_NOFORMAT,
			 &iosb, 0, 0, obuf, nobuf, 0, 0, 0, 0);
		nobuf = 0;
	}
#endif

#if	SYS_MSDOS
# if DISP_ANSI
	fflush(stdout);
# endif
#endif
}

#if SYS_VMS
static void
read_vms_tty(int length)
{
	int	status;
	QIO_SB	iosb;
	int	term[2] = {0,0};

	status = sys$qiow(EFN, iochan,
		(length == 1)
			? IO$_READLBLK|IO$M_NOECHO|IO$M_NOFILTR
			: IO$_READLBLK|IO$M_NOECHO|IO$M_NOFILTR|IO$M_TIMED,
		&iosb, 0, 0, ibuf, length, 0, term, 0, 0);

	if (status != SS$_NORMAL)
		tidy_exit(status);
	if (iosb.status == SS$_ENDOFFILE)
		tidy_exit(status);

		/* FIXME: check documentation */
	nibuf = iosb.count + (iosb.dev_dep_data >> 16);
	ibufi = 0;
}
#endif

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all. More complex in VMS that almost anyplace else, which figures. Very
 * simple on CPM, because the system can do exactly what you want.
 * This should be a terminal dispatch function.
 */
int
ttgetc(void)
{
#if	SYS_VMS
	while (ibufi >= nibuf) {
		if (!tttypahead())
			read_vms_tty(1);
	}
	return (ibuf[ibufi++] & 0xFF);	  /* Allow multinational  */
#else
#if SYS_MSDOS || SYS_OS2
	/*
	 * If we've got a mouse, poll waiting for mouse movement and mouse
	 * clicks until we've got a character to return.
	 */
# if OPT_MS_MOUSE
	if (ms_exists()) {
		for_ever {
			if (tttypahead())
				break;
			ms_processing();
		}
	}
# endif /* OPT_MS_MOUSE */
#if	CC_MSC || CC_TURBO || SYS_OS2
	return getch();
#endif
#if	CC_NEWDOSCC && !(CC_MSC||CC_TURBO||SYS_OS2||SYS_WINNT)
	{
	int c;

	/* if a char already is ready, return it */
	if (nxtchar >= 0) {
		c = nxtchar;
		nxtchar = -1;
		return(c);
	}

	/* call the dos to get a char */
	rg.h.ah = 7;		/* dos Direct Console Input call */
	intdos(&rg, &rg);
	c = rg.h.al;		/* grab the char */
	return(c & 0xff);
	}
#endif
#else /* ! (SYS_MSDOS || SYS_OS2) */
	/* Not used. */
	return 0;
#endif
#endif

}


/* tttypahead:	Check to see if any characters are already in the
		keyboard buffer
*/
#if	! SYS_WINNT
int
tttypahead(void)
{

#if	DISP_X11
	return x_typahead(0);
#endif

#if	DISP_VMSVT
	if (ibufi >= nibuf) {
		read_vms_tty(NIBUF);
		return (nibuf > 0);
	}
	return TRUE;
#endif

#if	SYS_MSDOS || SYS_OS2
	return (kbhit() != 0);
#endif

}
#endif

#endif /* not SYS_UNIX */


/* Get terminal size from system, first trying the driver, and then
 * the environment.  Store number of lines into *heightp and width
 * into *widthp.  If zero or a negative number is stored, the value
 * is not valid.  This may be fixed (in the tcap.c case) by the TERM
 * variable.
 */
#if ! DISP_X11
void
getscreensize (int *widthp, int *heightp)
{
	char *e;
#ifdef TIOCGWINSZ
	struct winsize size;
#endif
	*widthp = 0;
	*heightp = 0;
#ifdef TIOCGWINSZ
	if (ioctl (0, TIOCGWINSZ, (caddr_t)&size) == 0) {
		if ((int)(size.ws_row) > 0)
			*heightp = size.ws_row;
		if ((int)(size.ws_col) > 0)
			*widthp = size.ws_col;
	}
	if (*widthp <= 0) {
		e = getenv("COLUMNS");
		if (e)
			*widthp = atoi(e);
	}
	if (*heightp <= 0) {
		e = getenv("LINES");
		if (e)
			*heightp = atoi(e);
	}
#else
	e = getenv("COLUMNS");
	if (e)
		*widthp = atoi(e);
	e = getenv("LINES");
	if (e)
		*heightp = atoi(e);
#endif
}
#endif

/******************************************************************************/

/*
 * Define an empty terminal type for machines where we cannot use 'dumb_term',
 * so that command-line prompting will have something to talk to.
 */

static int  null_cres     (char *res);
static int  null_getc     (void);
static OUTC_DCL null_putc (OUTC_ARGS);
static int  null_typahead (void);
static void null_beep     (void);
static void null_close    (void);
static void null_eeol     (void);
static void null_eeop     (void);
static void null_flush    (void);
static void null_kclose   (void);
static void null_kopen    (void);
static void null_move     (int row, int col);
static void null_open     (void);
static void null_rev      (UINT state);

TERM null_term = {
	1,
	1,
	80,
	80,
	0/*MARGIN*/,
	0/*SCRSIZ*/,
	0/*NPAUSE*/,
	null_open,
	null_close,
	null_kopen,
	null_kclose,
	null_getc,
	null_putc,
	null_typahead,
	null_flush,
	null_move,
	null_eeol,
	null_eeop,
	null_beep,
	null_rev,
	null_cres,
	null_t_setfor,
	null_t_setback,
	null_t_setpal,
	null_t_scroll,
	null_t_pflush,
	null_t_icursor,
	null_t_title,
};

static void null_open(void)	{ }
static void null_close(void)	{ }
static void null_kopen(void)	{ }
static void null_kclose(void)	{ }
static int  null_getc(void)	{ return abortc; }
/*ARGSUSED*/
static OUTC_DCL null_putc(OUTC_ARGS) { OUTC_RET c; }
static int  null_typahead(void)	{ return FALSE; }
static void null_flush(void)	{ }
/*ARGSUSED*/
static void null_move(int row GCC_UNUSED, int col GCC_UNUSED) { }
static void null_eeol(void)	{ }
static void null_eeop(void)	{ }
static void null_beep(void)	{ }
/*ARGSUSED*/
static void null_rev(UINT state GCC_UNUSED) { }
/*ARGSUSED*/
static int null_cres(char *res GCC_UNUSED) { return(FALSE); }

/*
 * These are public, since we'll use them as placeholders for unimplemented
 * device methods.
 */
/*ARGSUSED*/ void null_t_setfor (int f GCC_UNUSED) { }
/*ARGSUSED*/ void null_t_setback (int b GCC_UNUSED) { }
/*ARGSUSED*/ void null_t_setpal (const char *p GCC_UNUSED) { }
/*ARGSUSED*/ void null_t_scroll (int f GCC_UNUSED, int t GCC_UNUSED, int n GCC_UNUSED) { }
/*ARGSUSED*/ void null_t_pflush (void) { }
/*ARGSUSED*/ void null_t_icursor (int c GCC_UNUSED) { }
/*ARGSUSED*/ void null_t_title (char *t GCC_UNUSED) { }

/******************************************************************************/

/*
 * This function is used during terminal initialization to allow us to setup
 * either a dumb or null terminal driver to handle stray command-line and other
 * debris, then (in the second call), open the screen driver.
 */
int
open_terminal(TERM *termp)
{
	static	TERM	save_term;
	static int	initialized;

	if (!initialized++) {

		/*
		 * If the open and/or close slots are empty, fill them in with
		 * the screen driver's functions.
		 */
		if (termp->t_open == 0)
			termp->t_open = term.t_open;

		if (termp->t_close == 0)
			termp->t_close = term.t_close;

		/*
		 * If the command-line driver is the same as the screen driver,
		 * then all we're really doing is opening the terminal in raw
		 * mode (e.g., the termcap driver), but with restrictions on
		 * cursor movement, etc.  We do a bit of juggling, because the
		 * screen driver typically sets entries (such as the screen
		 * size) in the 'term' struct.
		 */
		if (term.t_open == termp->t_open) {
			TTopen();
			save_term = term;
			term = *termp;
		} else {
			save_term = term;
			term = *termp;
			TTopen();
		}
	} else {
		/*
		 * If the command-line driver isn't the same as the screen
		 * driver, reopen the terminal with the screen driver.
		 */
		if (save_term.t_open != term.t_open) {
			TTclose();
			term = save_term;
			TTopen();
		} else {
			term = save_term;
		}
	}
	return TRUE;
}
