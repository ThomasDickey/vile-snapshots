/*	npopen:  like popen, but grabs stderr, too
 *		written by John Hutchinson, heavily modified by Paul Fox
 *
 * $Id: npopen.c,v 1.103 2025/01/26 14:38:58 tom Exp $
 *
 */

#include "estruct.h"
#include "edef.h"

#if ! TEST_DOS_PIPES

#if SYS_UNIX || SYS_OS2

/*
 * For OS/2 implementations of inout_popen(), npflush(), and npclose(),
 * see "os2pipe.c".
 *
 * For Win32 implementations of inout_popen(), npflush(), and npclose(),
 * see "w32pipe.c".
 */

#if SYS_OS2
#include <process.h>
#endif

#if OPT_EVAL
#include "nevars.h"
#define	user_SHELL()	get_shell()
#else
#define	user_SHELL()	getenv("SHELL")
#endif

#if SYS_OS2_EMX
#define SHELL_C "/c"
#else
#define SHELL_C "-c"
#endif

#define R 0
#define W 1

#if SYS_UNIX
static int pipe_pid;
static int pipe_pid2;
#endif

static void exec_sh_c(char *cmd);

FILE *
npopen(char *cmd, const char *type)
{
    FILE *ff = NULL;

    if (*type != 'r' && *type != 'w')
	return NULL;

    if (*type == 'r') {
	if (inout_popen(&ff, (FILE **) 0, cmd) != TRUE)
	    return NULL;
	return ff;
    } else {
	if (inout_popen((FILE **) 0, &ff, cmd) != TRUE)
	    return NULL;
	return ff;
    }
}
#endif /* SYS_UNIX || SYS_OS2 */

#if SYS_UNIX
int
inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    int rp[2];
    int wp[2];

    if (pipe(rp))
	return FALSE;
    if (pipe(wp))
	return FALSE;

    pipe_pid = softfork();
    if (pipe_pid < 0)
	return FALSE;

    ffstatus = file_is_pipe;
    fileeof = FALSE;
    if (pipe_pid) {		/* parent */

	if (fr) {
	    *fr = fdopen(rp[0], "r");
	    if (*fr == NULL) {
		(void) fprintf(stderr, "fdopen r failed\n");
		abort();
	    }
	} else {
	    (void) close(rp[0]);
	}
	(void) close(rp[1]);

	if (fw) {
	    *fw = fdopen(wp[1], "w");
	    if (*fw == NULL) {
		(void) fprintf(stderr, "fdopen w failed\n");
		abort();
	    }
	} else {
	    (void) close(wp[1]);
	}
	(void) close(wp[0]);
	return TRUE;

    } else {			/* child */

	beginDisplay();
	append_libdir_to_path();
	if (fw) {
	    (void) close(0);
	    if (dup(wp[0]) != 0) {
		IGNORE_RC(write(2, "dup 0 failed\r\n", (size_t) 15));
		exit(-1);
	    }
	}
	(void) close(wp[1]);
	if (fr) {
	    (void) close(1);
	    if (dup(rp[1]) != 1) {
		IGNORE_RC(write(2, "dup 1 failed\r\n", (size_t) 15));
		exit(-1);
	    }
	    (void) close(2);
	    if (dup(rp[1]) != 2) {
		IGNORE_RC(write(1, "dup 2 failed\r\n", (size_t) 15));
		exit(-1);
	    }
	} else {
	    (void) close(rp[1]);
	}
	(void) close(rp[0]);
	exec_sh_c(cmd);
	endofDisplay();

    }
    return TRUE;
}

void
npclose(FILE *fp)
{
    int child;

    if (fp != NULL) {
	(void) fclose(fp);

	if (pipe_pid == pipe_pid2)
	    pipe_pid2 = -1;

	while (pipe_pid >= 0 || pipe_pid2 >= 0) {
	    child = wait((int *) 0);
	    if (child < 0 && errno == EINTR) {
		if (pipe_pid >= 0)
		    (void) kill(SIGKILL, pipe_pid);
		if (pipe_pid2 >= 0)
		    (void) kill(SIGKILL, pipe_pid2);
	    } else {
		if (pipe_pid == child)
		    pipe_pid = -1;
		if (pipe_pid2 == child)
		    pipe_pid2 = -1;
	    }
	}
    }
}

static void
exec_sh_c(char *cmd)
{
    static char bin_sh[] = "/bin/sh";
    char *sh, *shname;
    int i;

#ifndef NOFILE
# define NOFILE 20
#endif
    /* Make sure there are no upper inherited file descriptors */
    for (i = 3; i < NOFILE; i++)
	(void) close(i);

    sh = user_SHELL();
    if (isEmpty(sh)) {
	sh = bin_sh;
	shname = pathleaf(sh);
    } else {
	shname = last_slash(sh);
	if (shname == NULL) {
	    shname = sh;
	} else {
	    shname++;
	    if (*shname == EOS)
		shname = sh;
	}
    }

    if (cmd) {
#if SYS_OS2_EMX
	/*
	 * OS/2 EMX accepts forward and backward slashes
	 * interchangeably except in one special case -
	 * invoking the OS/2 cmd.exe program for syntax
	 * filters.  That accepts only backslashes if we put a
	 * ".exe" suffix on the pathname.
	 */
	for (i = 0; ispath(cmd[i]); i++)
	    if (cmd[i] == '/')
		cmd[i] = '\\';
#endif
	(void) execlp(sh, shname, SHELL_C, cmd, (void *) 0);
    } else {
	(void) execlp(sh, shname, (void *) 0);
    }
    IGNORE_RC(write(2, "exec failed\r\n", (size_t) 14));
    exit(-1);
}

#if VILE_SOMEDAY

int shellstatus;

static int
process_exit_status(int status)
{
    if (WIFSIGNALED(status))
	return (128 + WTERMSIG(status));
    else if (!WIFSTOPPED(status))
	return (WEXITSTATUS(status));
    else
	return (EXECUTION_SUCCESS);
}

#endif /* VILE_SOMEDAY */

int
system_SHELL(char *cmd)
{
    int cpid;

    TRACE(("system_SHELL(%s)\n", NONNULL(cmd)));

    cpid = softfork();
    if (cpid < 0) {
	IGNORE_RC(write(2, "cannot fork\n", (size_t) 13));
	return cpid;
    }

    if (cpid) {			/* parent */
	int child;
	int status;

	beginDisplay();
	while ((child = wait(&status)) != cpid) {
	    if (child < 0 && errno == EINTR) {
		(void) kill(SIGKILL, cpid);
	    }
	}
	endofDisplay();
#if VILE_SOMEDAY
	shellstatus = process_exit_status(status);
#endif
	return 0;
    } else {
	beginDisplay();
	exec_sh_c(cmd);
	IGNORE_RC(write(2, "cannot exec\n", (size_t) 13));
	endofDisplay();
	return -1;
    }

}

int
softfork(void)
{
    /* Try & fork 5 times, backing off 1, 2, 4 .. seconds each try */
    int fpid;
    int tries = 5;
    int slp = 1;

    while ((fpid = fork()) < 0) {
	if (--tries == 0)
	    return -1;
	(void) catnap(slp * 1000, FALSE);
	slp <<= 1;
    }
    pipe_pid2 = fpid;		/* For calls to softfork which don't
				   remember the pid */
    return fpid;
}

#endif /* SYS_UNIX */
#endif /* TEST_DOS_PIPES */

#if SYS_MSDOS || TEST_DOS_PIPES
#include <fcntl.h>		/* defines O_RDWR */
#if ! TEST_DOS_PIPES
#include <io.h>			/* defines 'dup2()', etc. */
#endif

static void deleteTemp(void);

static FILE **myPipe;		/* current pipe-file pointer */
static FILE **myWrtr;		/* write-pipe pointer */
static FILE **myRead;		/* read-pipe pointer */
static char *myName[2];		/* name of temporary file for pipe */
static char *myCmds;		/* command to execute on read-pipe */
static int myRval;		/* return-value of 'system()' */

static int
createTemp(char *type)
{
    register int n = (*type == 'r');
    register int fd;

#if CC_WATCOM || CC_TURBO
    myName[n] = tmpnam((char *) 0);
#else
    myName[n] = tempnam(TMPDIR, type);
#endif
    if (myName[n] == 0)
	return -1;
    (void) close(creat(myName[n], 0666));
    if ((fd = open(myName[n], O_RDWR)) < 0) {
	deleteTemp();
	return -1;
    }
    return fd;
}

static void
deleteTemp(void)
{
    register int n;

    for (n = 0; n < 2; n++) {
	if (myName[n] != 0) {
	    TRACE(("deleteTemp #%d '%s'\n", n, myName[n]));
	    (void) unlink(myName[n]);
	    FreeAndNull(myName[n]);
	}
    }
}

static void
closePipe(FILE ***pp)
{
    if (*pp != 0) {
	if (**pp != 0) {
	    TRACE(("closePipe fd=%d\n", fileno(**pp)));
	    (void) fclose(**pp);
	    **pp = 0;
	}
	*pp = 0;
    }
}

static FILE *
readPipe(const char *cmd, int in, int out)
{
    int old0, old1, old2;

    TRACE(("readPipe(cmd='%s', in=%d, out=%d)\n", cmd, in, out));

    term.kclose();		/* close the keyboard in case of error */

    /* save and redirect stdin, stdout, and stderr */
    old0 = -1;
    old1 = dup(1);
    old2 = dup(2);

    if (in >= 0) {
	old0 = dup(0);
	dup2(in, 0);
    }
    dup2(out, 1);
    dup2(out, 2);

    myRval = system(cmd);

    /* restore old std... */
    if (in >= 0) {
	dup2(old0, 0);
	close(old0);
    }
    dup2(old1, 1);
    close(old1);
    dup2(old2, 2);
    close(old2);

    term.kopen();		/* reopen the keyboard */

    /* rewind command output */
    lseek(out, 0L, 0);
    return fdopen(out, "r");
}

#if SYS_MSDOS
static void
writePipe(const char *cmd)
{
    int old0;

    TRACE(("writePipe(cmd='%s')\n", cmd));

    term.kclose();		/* close the keyboard in case of error */

    (void) fclose(*myWrtr);
    *myWrtr = fopen(myName[0], "r");

    old0 = dup(0);
    dup2(fileno(*myWrtr), 0);

    myRval = system(cmd);

    /* restore old std... */
    dup2(old0, 0);
    close(old0);

    term.kopen();		/* reopen the keyboard */
}
#endif

FILE *
npopen(char *cmd, const char *type)
{
    FILE *ff = 0;

    if (*type == 'r') {
	(void) inout_popen(&ff, (FILE **) 0, cmd);
    } else if (*type == 'w') {
	(void) inout_popen((FILE **) 0, &ff, cmd);
    }
    return ff;
}

/*
 * Create pipe with either write- _or_ read-semantics.  Fortunately for us,
 * on SYS_MSDOS, we don't need both at the same instant.
 */
int
inout_popen(FILE **fr, FILE **fw, char *cmd)
{
    char *type = (fw != 0) ? "w" : "r";
    static FILE *pp[2] =
    {0, 0};
    int fd;

    TRACE(("inout_popen(fr=%p, fw=%p, cmd='%s')\n", fr, fw, cmd));

    ffstatus = file_is_pipe;
    fileeof = FALSE;
    append_libdir_to_path();

    /* Create the file that will hold the pipe's content */
    if ((fd = createTemp(type)) >= 0) {
	if (fw == 0) {
	    *fr = pp[0] = readPipe(cmd, -1, fd);
	    myWrtr = 0;
	    myPipe = &pp[0];	/* "fr" may be stack-based. */
	    myCmds = 0;
	} else {
	    *fw = pp[1] = fdopen(fd, type);
	    myPipe = fr;
	    myWrtr = &pp[1];	/* "fw" may be stack-based. */
	    myCmds = strmalloc(cmd);
	}
    }
    return TRUE;
}

/*
 * If we were writing to a pipe, invoke the read-process with stdin set to the
 * temporary-file.  This is used in the filter-buffer code, which needs both
 * read- and write-pipes.
 */
void
npflush(void)
{
    if (myCmds != 0) {
	if (myWrtr != 0) {
	    int fd;
	    static FILE *pp;

	    (void) fflush(*myWrtr);
#if 0
	    (void) fclose(*myWrtr);
	    *myWrtr = fopen(myName[0], "r");
#else
	    rewind(*myWrtr);
#endif
	    fd = createTemp("r");
	    pp = fdopen(fd, "r");
	    myRead = &pp;
	    *myPipe = readPipe(myCmds, fileno(*myWrtr), fd);
	}
	FreeAndNull(myCmds);
    }
}

void
npclose(FILE *fp)
{
#if SYS_MSDOS
    if (myWrtr != 0 && myPipe == 0)
	writePipe(myCmds);
#endif
    closePipe(&myRead);
    closePipe(&myWrtr);
    closePipe(&myPipe);
    deleteTemp();
}

int
softfork(void)			/* dummy function to make filter-region work */
{
    return 0;
}

#if TEST_DOS_PIPES
int
system_SHELL(char *cmd)		/* dummy function */
{
    return 0;
}
#endif /* TEST_DOS_PIPES */
#endif /* SYS_MSDOS */
