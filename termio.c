/*
 * The functions in this file negotiate with the operating system for
 * characters, and write characters in a barely buffered fashion on the display.
 * All operating systems.
 *
 * $Id: termio.c,v 1.226 2025/01/26 14:33:43 tom Exp $
 */

#include	"estruct.h"
#include	"edef.h"
#include	"nefunc.h"

#if CC_DJGPP
# include <pc.h>		/* for kbhit() */
#endif

#if SYS_UNIX

static void ttmiscinit(void);

/* there are three copies of the tt...() routines here -- one each for
	POSIX termios, traditional termio, and sgtty.  If you have a
	choice, I recommend them in that order. */

/* ttopen() and ttclose() are responsible for putting the terminal in raw
	mode, setting up terminal signals, etc.
   ttclean() prepares the terminal for shell escapes, and ttunclean() gets
	us back into vile's mode
*/

/* I suppose this config stuff should move to estruct.h */

#if defined(HAVE_TERMIOS_H) && defined(HAVE_TCGETATTR)
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
# define USE_POSIX_TERMIOS 0
# ifdef HAVE_TERMIO_H
#  define USE_TERMIO 1
#  define USE_FCNTL 1
# else
#  ifdef HAVE_SGTTY_H
#   define USE_SGTTY 1
#   define USE_FIONREAD 1
#  else
error "No termios or sgtty"
#  endif
# endif
#endif

/* FIXME: There used to be code here which dealt with OSF1 not working right
 * with termios.  We will have to determine if this is still the case and
 * add code here to deal with it if so.
 */

#if !defined(FIONREAD)
/* try harder to get it */
# ifdef HAVE_SYS_FILIO_H
#  include <sys/filio.h>
# else /* if you have trouble including ioctl.h, try "sys/ioctl.h" instead */
#  ifdef HAVE_IOCTL_H
#   include <ioctl.h>
#  else
#   ifdef HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#    define VL_sys_ioctl
#   endif
#  endif
# endif
#endif

#if DISP_X11			/* don't use either one */
# undef USE_FCNTL
# undef USE_FIONREAD
#else
# if defined(FIONREAD)
  /* there seems to be a bug in someone's implementation of fcntl -- it causes
   * output to be flushed if you change to ndelay input while output is
   * pending.  (In some instances, the fcntl sets O_NDELAY on the output!).
   *
   * For these systems, we use FIONREAD instead, if possible.
   * In fact, try to use FIONREAD in any case if present.  If you have
   * the problem with fcntl, you'll notice that the screen doesn't always
   * refresh correctly, as if the fcntl is interfering with the output drain.
   */
#  undef USE_FCNTL
#  define USE_FIONREAD 1
# endif
# if defined(HAVE_SELECT) && defined(HAVE_TYPE_FD_SET)
   /* Attempt to use the select call if possible to find out if input is
    * ready.  This will allow us to use the file descriptor watching
    * facilities.
    */
#  define USE_SELECT 1
#  undef USE_FCNTL
#  undef USE_FIONREAD
# else
#  if defined(HAVE_POLL) && defined(HAVE_POLL_H)
#   define USE_POLL 1
#  endif
# endif
#endif

#ifndef USE_FIONREAD
#define USE_FIONREAD 0
#endif

#ifndef USE_FCNTL
#define USE_FCNTL 0
#endif

#ifndef USE_POLL
#define USE_POLL 0
#endif

#ifndef USE_SELECT
#define USE_SELECT 0
#endif

#ifndef USE_SGTTY
#define USE_SGTTY 0
#endif

#ifndef USE_TERMIO
#define USE_TERMIO 0
#endif

#if USE_POLL
#include <poll.h>
#endif

#if !defined(O_NDELAY) && defined(O_NONBLOCK)
#define O_NDELAY O_NONBLOCK
#endif

#ifndef O_NDELAY
#undef USE_FCNTL
#define USE_FCNTL 0
#endif

#if USE_FCNTL

#include	<fcntl.h>
static char kbd_char;		/* one char of typeahead, gotten during poll    */
static int kbd_char_good;	/* is a char in kbd_char?                       */

/*
 * putting the input tty in polling mode lets us check for
 * user typeahead
 */
static void
set_kbd_polling(int yes)
{
    static int kbd_flags = -1;	/* initial keyboard flags       */
    static int kbd_is_polled;	/* are we in O_NDELAY mode?     */

    if (kbd_flags == -1) {
	kbd_flags = fcntl(0, F_GETFL, 0);
	if (kbd_flags == -1)
	    imdying(SIGINT);
	kbd_is_polled = FALSE;
    }

    if (yes) {			/* turn polling on -- put us in NDELAY mode */
	if (!kbd_is_polled) {
	    if (fcntl(0, F_SETFL, kbd_flags | O_NDELAY) < 0)
		imdying(SIGINT);
	}
	kbd_is_polled = TRUE;	/* I think */
    } else {			/* turn polling off -- clear NDELAY mode */
	if (kbd_is_polled) {
	    if (fcntl(0, F_SETFL, kbd_flags) < 0)
		imdying(SIGINT);
	}
	kbd_is_polled = FALSE;
    }
}

#endif

#define SMALL_STDOUT 1

#if defined(SMALL_STDOUT) && (defined (USE_FCNTL) || defined(USE_FIONREAD))
#define TBUFSIZ 128		/* Provide a smaller terminal output buffer so that
				   the type-ahead detection works better (more often).
				   That is, we overlap screen writing with more keyboard polling */
#else
#define TBUFSIZ 1024		/* reduces the number of writes */
#endif

#if USE_POSIX_TERMIOS

#include <termios.h>

#ifdef NEED_PTEM_H
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

static int saved_tty = FALSE;
static struct termios otermios;

#if !DISP_X11
static struct termios ntermios;
static char tobuf[TBUFSIZ];	/* terminal output buffer */
#endif

void
vl_save_tty(void)
{
    int s;

    s = tcgetattr(0, &otermios);
    if (s < 0) {
	perror("vl_save_tty tcgetattr");
	ExitProgram(BADEXIT);
    }

    suspc = otermios.c_cc[VSUSP];
    intrc = otermios.c_cc[VINTR];
    killc = otermios.c_cc[VKILL];
    startc = otermios.c_cc[VSTART];
    stopc = otermios.c_cc[VSTOP];
    backspc = otermios.c_cc[VERASE];
#ifdef VWERASE			/* Sun has it.  any others? */
    wkillc = otermios.c_cc[VWERASE];
#else
    wkillc = tocntrl('W');
#endif
    saved_tty = TRUE;
}

void
vl_restore_tty(void)
{
    if (saved_tty) {
	tcdrain(1);
	tcsetattr(0, TCSADRAIN, &otermios);
    }
}

void
ttopen(void)
{
    vl_save_tty();

#if !DISP_X11
#ifdef HAVE_SETVBUF
# ifdef SETVBUF_REVERSED
    setvbuf(stdout, _IOFBF, tobuf, TBUFSIZ);
# else
    setvbuf(stdout, tobuf, _IOFBF, (size_t) TBUFSIZ);
# endif
#else /* !HAVE_SETVBUF */
    setbuffer(stdout, tobuf, TBUFSIZ);
#endif /* !HAVE_SETVBUF */
#endif /* !DISP_X11 */

    /* this could probably be done more POSIX'ish? */
#if OPT_SHELL && defined(SIGTSTP) && defined(SIGCONT)
    setup_handler(SIGTSTP, SIG_DFL);	/* set signals so that we can */
    setup_handler(SIGCONT, rtfrmshell);		/* suspend & restart */
#endif
#ifdef SIGTTOU
    setup_handler(SIGTTOU, SIG_IGN);	/* ignore output prevention */
#endif

#if ! DISP_X11
    ntermios = otermios;

    /* new input settings: turn off crnl mapping, cr-ignoring,
     * case-conversion, and allow BREAK
     */
    ntermios.c_iflag = (tcflag_t) (BRKINT | (otermios.c_iflag &
					     (ULONG) ~ (INLCR | IGNCR | ICRNL
#ifdef IUCLC
							| IUCLC
#endif
					     )));

    ntermios.c_oflag = 0;
    ntermios.c_lflag = ISIG;
    ntermios.c_cc[VMIN] = 1;
    ntermios.c_cc[VTIME] = 0;
#ifdef	VSWTCH
    ntermios.c_cc[VSWTCH] = VDISABLE;
#endif
    ntermios.c_cc[VSUSP] = VDISABLE;
#if defined (VDSUSP) && defined(NCCS) && VDSUSP < NCCS
    ntermios.c_cc[VDSUSP] = VDISABLE;
#endif
    ntermios.c_cc[VSTART] = VDISABLE;
    ntermios.c_cc[VSTOP] = VDISABLE;
#endif /* ! DISP_X11 */

    ttmiscinit();

    term.unclean();
}

/* we disable the flow control chars so we can use ^S as a command, but
  some folks still need it.  they can put "flow-control-enable" in their
  .vilerc
  no argument:	re-enable ^S/^Q processing in the driver
  with arg:	disable it
*/
/* ARGSUSED */
int
flow_control_enable(int f GCC_UNUSED, int n GCC_UNUSED)
{
#if !DISP_X11
    if (!f) {
	ntermios.c_cc[VSTART] = (cc_t) startc;
	ntermios.c_cc[VSTOP] = (cc_t) stopc;
    } else {
	ntermios.c_cc[VSTART] = VDISABLE;
	ntermios.c_cc[VSTOP] = VDISABLE;
    }
    term.unclean();
#endif
    return TRUE;
}

void
ttclose(void)
{
    term.clean(TRUE);
}

/*
 * Clean up in anticipation for a return to the
 * operating system. Move down to the last line and advance, to make room
 * for the system prompt. Shut down the channel to the
 * terminal.
 */
/*ARGSUSED*/
void
ttclean(int f GCC_UNUSED)
{
#if !DISP_X11
    if (f)
	term.openup();

    (void) fflush(stdout);
    vl_restore_tty();
    term.flush();
    term.close();
    term.kclose();
#if USE_FCNTL
    set_kbd_polling(FALSE);
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
struct termio otermio, ntermio;
static int saved_tty = FALSE;

#ifdef HAVE_SETBUFFER		/* setbuffer() isn't on most termio systems */
char tobuf[TBUFSIZ];		/* terminal output buffer */
#endif

void
vl_save_tty(void)
{
    ioctl(0, TCGETA, (char *) &otermio);	/* save old settings */

    intrc = otermio.c_cc[VINTR];
    killc = otermio.c_cc[VKILL];
    startc = tocntrl('Q');
    stopc = tocntrl('S');
    backspc = otermio.c_cc[VERASE];
    wkillc = tocntrl('W');

#if SIGTSTP
#ifdef V_SUSP
    suspc = otermio.c_cc[V_SUSP];
#else
    suspc = -1;
#endif
#else /* no SIGTSTP */
    suspc = tocntrl('Z');
#endif
    saved_tty = TRUE;
}

void
vl_restore_tty(void)
{
    if (saved_tty)
	ioctl(0, TCSETAF, (char *) &otermio);
}

void
ttopen(void)
{
    vl_save_tty();

#if defined(HAVE_SETBUFFER) && !DISP_X11
    setbuffer(stdout, tobuf, TBUFSIZ);
#endif

#if SIGTSTP
/* be careful here -- VSUSP is sometimes out of the range of the c_cc array */
#ifdef V_SUSP
    ntermio.c_cc[V_SUSP] = -1;
#endif
#ifdef V_DSUSP
    ntermio.c_cc[V_DSUSP] = -1;
#endif

#if OPT_SHELL
    setup_handler(SIGTSTP, SIG_DFL);	/* set signals so that we can */
    setup_handler(SIGCONT, rtfrmshell);		/* suspend & restart */
#endif
    setup_handler(SIGTTOU, SIG_IGN);	/* ignore output prevention */
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
    term.unclean();
}

/* we disable the flow control chars so we can use ^S as a command, but
  some folks still need it.  they can put "flow-control-enable" in their
  .vilerc */
/* ARGSUSED */
int
flow_control_enable(int f, int n)
{
#if !DISP_X11
    ntermio.c_iflag &= ~(IXON | IXANY | IXOFF);
    if (!f)
	ntermio.c_iflag |= otermio.c_iflag & (IXON | IXANY | IXOFF);
    term.unclean();
#endif
    return TRUE;
}

void
ttclose(void)
{
    term.clean(TRUE);
}

void
ttclean(int f)
{
#if ! DISP_X11
    if (f)
	term.openup();

    (void) fflush(stdout);
    term.flush();
    term.close();
    term.kclose();		/* xterm */
    vl_restore_tty();
#if USE_FCNTL
    set_kbd_polling(FALSE);
#endif
#endif /* DISP_X11 */
}

void
ttunclean(void)
{
#if ! DISP_X11
    ioctl(0, TCSETAW, (char *) &ntermio);
#endif
}

#endif /* USE_TERMIO */

#if USE_SGTTY

#if USE_FIONREAD
char tobuf[TBUFSIZ];		/* terminal output buffer */
#endif

#undef	CTRL
#include	<sgtty.h>	/* for stty/gtty functions */

static int saved_tty = FALSE;

struct sgttyb ostate;		/* saved tty state */
struct sgttyb nstate;		/* values for editor mode */
struct sgttyb rnstate;		/* values for raw editor mode */

int olstate;			/* Saved local mode values */
int nlstate;			/* new local mode values */

struct ltchars oltchars;	/* Saved terminal special character set */
struct ltchars nltchars =
{-1, -1, -1, -1, -1, -1};	/* a lot of nothing */
struct tchars otchars;		/* Saved terminal special character set */
struct tchars ntchars;		/*  = { -1, -1, -1, -1, -1, -1 }; */

void
vl_save_tty(void)
{
    ioctl(0, TIOCGETP, (char *) &ostate);	/* save old state */
    killc = ostate.sg_kill;
    backspc = ostate.sg_erase;

    ioctl(0, TIOCGETC, (char *) &otchars);	/* Save old characters */
    intrc = otchars.t_intrc;
    startc = otchars.t_startc;
    stopc = otchars.t_stopc;

    ioctl(0, TIOCGLTC, (char *) &oltchars);	/* Save old characters */
    wkillc = oltchars.t_werasc;
    suspc = oltchars.t_suspc;

#ifdef	TIOCLGET
    ioctl(0, TIOCLGET, (char *) &olstate);
#endif
    saved_tty = TRUE;
}

void
vl_restore_tty(void)
{
    if (saved_tty) {
	ioctl(0, TIOCSETN, (char *) &ostate);
	ioctl(0, TIOCSETC, (char *) &otchars);
	ioctl(0, TIOCSLTC, (char *) &oltchars);
#ifdef	TIOCLSET
	ioctl(0, TIOCLSET, (char *) &olstate);
#endif
    }
}

void
ttopen(void)
{
    vl_save_tty();

#if ! DISP_X11
    nstate = ostate;
    nstate.sg_flags |= CBREAK;
    nstate.sg_flags &= ~(ECHO | CRMOD);		/* no echo for now... */
    ioctl(0, TIOCSETN, (char *) &nstate);	/* set new state */
#endif

    rnstate = nstate;
    rnstate.sg_flags &= ~CBREAK;
    rnstate.sg_flags |= RAW;

#if ! DISP_X11
    ntchars = otchars;
    ntchars.t_brkc = -1;
    ntchars.t_eofc = -1;
    ntchars.t_startc = -1;
    ntchars.t_stopc = -1;
    ioctl(0, TIOCSETC, (char *) &ntchars);	/* Place new character into K */
    ioctl(0, TIOCSLTC, (char *) &nltchars);	/* Place new character into K */

#ifdef	TIOCLGET
    nlstate = olstate;
    nlstate |= LLITOUT;
    ioctl(0, TIOCLSET, (char *) &nlstate);
#endif

#if OPT_SHELL
    setup_handler(SIGTSTP, SIG_DFL);	/* set signals so that we can */
    setup_handler(SIGCONT, rtfrmshell);		/* suspend & restart */
#endif
    setup_handler(SIGTTOU, SIG_IGN);	/* ignore output prevention */
#endif

#if USE_FIONREAD
    setbuffer(stdout, tobuf, TBUFSIZ);
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
    term.unclean();
#endif
    return TRUE;
}

void
ttclose(void)
{
    term.clean(TRUE);
}

void
ttclean(int f)
{
#if ! DISP_X11
    if (f)
	term.openup();

    term.flush();
    term.close();
    term.kclose();		/* xterm */
    vl_restore_tty();
#endif
}

void
ttunclean(void)
{
#if ! DISP_X11
    ioctl(0, TIOCSETN, (char *) &nstate);
    ioctl(0, TIOCSETC, (char *) &ntchars);
    ioctl(0, TIOCSLTC, (char *) &nltchars);
#ifdef	TIOCLSET
    ioctl(0, TIOCLSET, (char *) &nlstate);
#endif

#endif /* !DISP_X11 */
}

#endif /* USE_SGTTY */

#if !DISP_X11
OUTC_DCL
ttputc(OUTC_ARGS)
{
    OUTC_RET vl_ttputc(c);
}

OUTC_DCL
vl_ttputc(int c)
{
    OUTC_RET putchar((char) c);
}

void
ttflush(void)
{
    (void) fflush(stdout);
}

#if USE_SELECT
static fd_set watchfd_read_fds;
static fd_set watchfd_write_fds;
static int watchfd_maxfd;
#else
#if USE_POLL
static struct pollfd watch_fds[256];
static int watch_max;
#else
#ifdef __BEOS__
static UCHAR watch_fds[256];
static int watch_max;
#endif /* __BEOS__ */
#endif /* USE_POLL */
#endif /* USE_SELECT */

int
ttwatchfd(int fd, WATCHTYPE type, long *idp)
{
#if USE_SELECT
    *idp = (long) fd;
    if (fd > watchfd_maxfd)
	watchfd_maxfd = fd;
    if (type & WATCHREAD)
	FD_SET(fd, &watchfd_read_fds);
    if (type & WATCHWRITE)
	FD_SET(fd, &watchfd_write_fds);
#else /* USE_SELECT */
#if USE_POLL
    int n;
    for (n = 0; n < watch_max; n++) {
	if (watch_fds[n].fd == fd)
	    break;
    }
    if (n < (int) TABLESIZE(watch_fds) - 1) {
	*idp = (long) fd;
	if (n >= watch_max)
	    watch_max = n + 1;

	watch_fds[n].fd = fd;
	if (type & WATCHREAD)
	    watch_fds[n].events |= POLLIN;
	if (type & WATCHWRITE)
	    watch_fds[n].events |= POLLOUT;

    } else {
	return FALSE;
    }
#else
#ifdef __BEOS__
    if (fd < (int) sizeof(watch_fds)) {
	*idp = (long) fd;
	if (fd > watch_max)
	    watch_max = fd;
	watch_fds[fd] |= type;
    }
#endif /* __BEOS__ */
#endif /* USE_POLL */
#endif /* USE_SELECT */
    return TRUE;
}

/* ARGSUSED */
void
ttunwatchfd(int fd, long id GCC_UNUSED)
{
#if USE_SELECT
    FD_CLR(fd, &watchfd_read_fds);
    FD_CLR(fd, &watchfd_write_fds);
    while (watchfd_maxfd != 0
	   && !FD_ISSET(watchfd_maxfd - 1, &watchfd_read_fds)
	   && !FD_ISSET(watchfd_maxfd - 1, &watchfd_write_fds))
	watchfd_maxfd--;
#else /* USE_SELECT */
#if USE_POLL
    int n;
    for (n = 0; n < watch_max; n++) {
	if (watch_fds[n].fd == fd) {
	    watch_max--;
	    while (n < watch_max) {
		watch_fds[n] = watch_fds[n + 1];
		n++;
	    }
	    break;
	}
    }
#else
#ifdef __BEOS__
    if (fd < (int) sizeof(watch_fds)) {
	watch_fds[fd] = 0;
	while (watch_max != 0
	       && watch_fds[watch_max - 1] == 0) {
	    watch_max--;
	}
    }
#endif /* __BEOS__ */
#endif /* USE_POLL */
#endif /* USE_SELECT */
}

#ifdef VAL_AUTOCOLOR
#define val_autocolor() global_b_val(VAL_AUTOCOLOR)
#else
#define val_autocolor() 0
#endif

#if USE_SELECT || USE_POLL
static int
vl_getchar(void)
{
    char c;
    int n;

    n = (int) read(0, &c, (size_t) 1);
    if (n <= 0) {
	if (n < 0 && errno == EINTR)
	    return -1;
	imdying(SIGINT);
    }
    return (c & 0xff);
}
#endif

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all.
 */
int
ttgetc(void)
{
#if USE_SELECT
    for_ever {
	fd_set read_fds;
	fd_set write_fds;
	fd_set except_fds;
	int fd, status;
	struct timeval tval;
	int acmilli = val_autocolor();

	read_fds = watchfd_read_fds;
	write_fds = watchfd_write_fds;
	except_fds = watchfd_read_fds;

	tval.tv_sec = acmilli / 1000;
	tval.tv_usec = (acmilli % 1000) * 1000;
	FD_SET(0, &read_fds);	/* add stdin to the set */
	FD_SET(0, &except_fds);

	status = select(watchfd_maxfd + 1,
			&read_fds, &write_fds, &except_fds,
			acmilli > 0 ? &tval : NULL);
	if (status < 0) {	/* Error */
	    if (errno == EINTR)
		continue;
	    else
		return -1;
	} else if (status > 0) {	/* process descriptors */
	    for (fd = watchfd_maxfd; fd > 0; fd--) {
		if (FD_ISSET(fd, &read_fds)
		    || FD_ISSET(fd, &write_fds)
		    || FD_ISSET(fd, &except_fds))
		    dowatchcallback(fd);
	    }
	    if (FD_ISSET(0, &read_fds) || FD_ISSET(0, &except_fds))
		break;
	} else {		/* Timeout */
	    autocolor();
	}
    }
    return vl_getchar();
#else /* !USE_SELECT */
#if USE_POLL
    int acmilli = val_autocolor();

    if (acmilli != 0
	|| watch_max != 0) {
	int n;

	for_ever {
	    watch_fds[watch_max].fd = 0;
	    watch_fds[watch_max].events = POLLIN;
	    watch_fds[watch_max].revents = 0;
	    if ((n = poll(watch_fds, watch_max + 1, acmilli)) > 0) {
		for (n = 0; n < watch_max; n++) {
		    if (watch_fds[n].revents & (POLLIN | POLLOUT | POLLERR)) {
			dowatchcallback(watch_fds[n].fd);
		    }
		}
		if (watch_fds[watch_max].revents & (POLLIN | POLLERR))
		    break;
	    } else if (n == 0 && acmilli != 0) {
		autocolor();
		if (watch_max == 0)
		    break;	/* revert to blocking input */
	    }
	}
    }
    return vl_getchar();
#else
#if USE_FCNTL
    if (!kbd_char_good) {
	int n;
	set_kbd_polling(FALSE);
	n = read(0, &kbd_char, 1);
	if (n <= 0) {
	    if (n < 0 && errno == EINTR)
		return -1;
	    imdying(SIGINT);
	}
    }
    kbd_char_good = FALSE;
    return (kbd_char);
#else /* USE_FCNTL */
#ifdef __BEOS__
    int fd;
    int acmilli = val_autocolor();
    for_ever {
	for (fd = 1; fd < watch_max; fd++) {
	    if (((watch_fds[fd] & WATCHREAD) != 0 && beos_has_input(fd))
		|| ((watch_fds[fd] & WATCHWRITE) != 0 && beos_can_output(fd))) {
		dowatchcallback(fd);
	    }
	}
	if (beos_has_input(0))
	    break;
	if (acmilli > 0) {
	    beos_napms(5);
	    if ((acmilli -= 5) <= 0) {
		autocolor();
	    }
	}
    }
    return getchar();
#else /* __BEOS__ */
    int c;
    c = getchar();
    if (c == EOF) {
#ifdef linux
	/*
	 * The command "^X!vile -V" makes vile receive an EOF at this
	 * point.  In fact, I've seen as many as 20,000 to 35,000 EOF's
	 * in a row at this point, presumably while the processes are
	 * fighting over /dev/tty.  The corresponding errno is ESPIPE
	 * (invalid seek).
	 *
	 * (tested with Linux 2.0.36 and 2.2.5-15)
	 */
	return -1;
#else
	if (errno == EINTR)
	    return -1;
	imdying(SIGINT);
#endif
    }
    return c;
#endif /* __BEOS__ */
#endif /* USE_FCNTL */
#endif /* USE_POLL */
#endif /* USE_SELECT */
}
#endif /* !DISP_X11 */

/* tttypahead:	Check to see if any characters are already in the
		keyboard buffer
*/
int
tttypahead(void)
{

#if DISP_X11
    return x_milli_sleep(0);
#else

# if USE_SELECT || USE_POLL || defined(__BEOS__)
    /* use the watchinput part of catnap if it's useful */
    return catnap(0, TRUE);
# else
#  if	USE_FIONREAD
    {
	long x;
	return ((ioctl(0, FIONREAD, (void *) &x) < 0) ? 0 : (int) x);
    }
#  else
#   if	USE_FCNTL
    if (!kbd_char_good) {
	set_kbd_polling(TRUE);
	if (read(0, &kbd_char, 1) == 1)
	    kbd_char_good = TRUE;
    }
    return (kbd_char_good);
#   else
    /* otherwise give up */
    return FALSE;
#   endif /* USE_FCNTL */
#  endif /* USE_FIONREAD */
# endif	/* using catnap */
#endif /* DISP_X11 */
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
    setbuf(stdin, (char *) 0);
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

void
ttopen(void)
{
    /* make sure backspace is bound to backspace */
    asciitbl[backspc] = &f_backchar_to_bol;
}

void
ttclose(void)
{
    term.clean(TRUE);
}

void
ttclean(int f)
{
#if !DISP_X11
    if (f)
	term.openup();

    term.flush();
    term.close();
    term.kclose();
#endif
}

void
ttunclean(void)
{
}

/*
 * Write a character to the display.
 * On CPM terminal I/O unbuffered, so we just write the byte out. Ditto on
 * MS-DOS (use the very very raw console output routine).
 */
OUTC_DCL
ttputc(OUTC_ARGS)
{
    (void) c;
#if SYS_OS2 && !DISP_VIO
    OUTC_RET putch(c);
#endif
#if SYS_MSDOS
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
#if SYS_MSDOS
# if DISP_ANSI
    fflush(stdout);
# endif
#endif
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all.  Very simple on CPM, because the system can do exactly what you
 * want.
 * This should be a terminal dispatch function.
 */
int
ttgetc(void)
{
#if SYS_MSDOS || SYS_OS2
    /*
     * If we've got a mouse, poll waiting for mouse movement and mouse
     * clicks until we've got a character to return.
     */
#if CC_MSC || CC_TURBO || SYS_OS2
    return getch();
#endif
#if CC_NEWDOSCC && !(CC_MSC||CC_TURBO||SYS_OS2||SYS_WINNT)
    {
	int c;
	union REGS rg;		/* cpu registers for DOS calls */
	static int nxtchar = -1;

	/* if a char already is ready, return it */
	if (nxtchar >= 0) {
	    c = nxtchar;
	    nxtchar = -1;
	    return (c);
	}

	/* call the dos to get a char */
	rg.h.ah = 7;		/* dos Direct Console Input call */
	intdos(&rg, &rg);
	c = rg.h.al;		/* grab the char */
	return (c & 0xff);
    }
#endif
#else /* ! (SYS_MSDOS || SYS_OS2) */
    /* Not used. */
    return 0;
#endif
}

/* tttypahead:	See if the user has more characters waiting in the
		keyboard buffer
*/
#if !  SYS_WINNT && !SYS_VMS
int
tttypahead(void)
{

#if DISP_X11
    return x_milli_sleep(0);
#endif

#if SYS_MSDOS || SYS_OS2
    return (kbhit() != 0);
#endif

}
#endif

#endif /* not SYS_UNIX */

#ifdef HAVE_SIZECHANGE

#if !defined(VL_sys_ioctl)
#if !defined(sun) || !defined(HAVE_TERMIOS_H)
#include <sys/ioctl.h>
#endif
#endif

/*
 * SCO defines TIOCGSIZE and the corresponding struct.  Other systems (SunOS,
 * Solaris, IRIX) define TIOCGWINSZ and struct winsize.
 */
#ifdef TIOCGSIZE
# define IOCTL_GET_WINSIZE TIOCGSIZE
# define IOCTL_SET_WINSIZE TIOCSSIZE
# define STRUCT_WINSIZE struct ttysize
# define WINSIZE_ROWS(n) n.ts_lines
# define WINSIZE_COLS(n) n.ts_cols
#else
# ifdef TIOCGWINSZ
#  define IOCTL_GET_WINSIZE TIOCGWINSZ
#  define IOCTL_SET_WINSIZE TIOCSWINSZ
#  define STRUCT_WINSIZE struct winsize
#  define WINSIZE_ROWS(n) n.ws_row
#  define WINSIZE_COLS(n) n.ws_col
# endif
#endif

#endif /* HAVE_SIZECHANGE */
/* Get terminal size from system, first trying the driver, and then
 * the environment.  Store number of lines into *heightp and width
 * into *widthp.  If zero or a negative number is stored, the value
 * is not valid.  This may be fixed (in the tcap.c case) by the TERM
 * variable.
 */
#if ! DISP_X11
void
getscreensize(int *widthp, int *heightp)
{
    char *e;
#ifdef HAVE_SIZECHANGE
    STRUCT_WINSIZE size;
#endif
    *widthp = 0;
    *heightp = 0;
#ifdef HAVE_SIZECHANGE
    if (ioctl(0, (long) IOCTL_GET_WINSIZE, (void *) &size) == 0) {
	if ((int) (WINSIZE_ROWS(size)) > 0)
	    *heightp = WINSIZE_ROWS(size);
	if ((int) (WINSIZE_COLS(size)) > 0)
	    *widthp = WINSIZE_COLS(size);
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
 * This function is used during terminal initialization to allow us to setup
 * either a dumb or null terminal driver to handle stray command-line and other
 * debris, then (in the second call), open the screen driver.
 */
int
open_terminal(TERM * termp)
{
    static TERM save_term;
    static int initialized;

    if (!initialized++) {

	/*
	 * Help separate dumb_term from termio.c
	 */
	if (termp != &null_term) {
	    if (termp->clean == nullterm_clean)
		termp->clean = ttclean;
	    if (termp->unclean == nullterm_unclean)
		termp->unclean = ttunclean;
	    if (termp->openup == nullterm_openup)
		termp->openup = kbd_openup;
	}

	/*
	 * If the open and/or close slots are empty, fill them in with
	 * the screen driver's functions.
	 */
	if (termp->open == NULL)
	    termp->open = term.open;

	if (termp->close == NULL)
	    termp->close = term.close;

	/*
	 * If the command-line driver is the same as the screen driver,
	 * then all we're really doing is opening the terminal in raw
	 * mode (e.g., the termcap driver), but with restrictions on
	 * cursor movement, etc.  We do a bit of juggling, because the
	 * screen driver typically sets entries (such as the screen
	 * size) in the 'term' struct.
	 */
	if (term.open == termp->open) {
	    term.open();
	    save_term = term;
	    term = *termp;
	} else {
	    save_term = term;
	    term = *termp;
	    term.open();
	}
    } else {
	/*
	 * If the command-line driver isn't the same as the screen
	 * driver, reopen the terminal with the screen driver.
	 */
	if (save_term.open != term.open) {
	    term.close();
	    term = save_term;
	    term.open();
	} else {
	    term = save_term;
	}
    }
    return TRUE;
}
