
/* pipe routines for OS/2
 *  lee johnson, spring 1995
 */
#define INCL_DOSFILEMGR
#define INCL_DOSQUEUES
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include "estruct.h"

#include <process.h>

#ifndef EOS2ERR
#define EOS2ERR EINVAL
#endif

#ifndef EOUTOFMEM
#define EOUTOFMEM ENOMEM
#endif

#if CC_WATCOM
#define _fdopen fdopen
#define _cwait  cwait
#endif

/* We have one pipe running from the parent to the child... */
static HFILE pipe_p_to_c = NULLHANDLE, pipe_c_from_p = NULLHANDLE;

/* ...and another running from the child to the parent. */
static HFILE pipe_c_to_p = NULLHANDLE, pipe_p_from_c = NULLHANDLE;

static FILE *vile_in  = NULL;	/* stream attached to pipe_p_from_c */
static FILE *vile_out = NULL;	/* stream attached to pipe_p_to_c */

static int pid = -1;

static void
DupError(APIRET rc)
{
	switch(rc)
	{
	case ERROR_TOO_MANY_OPEN_FILES:
		errno = EMFILE;
		break;

	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_TARGET_HANDLE:
		errno = EBADF;
		break;

	default:
		errno = EOS2ERR;
		break;
	}
}

int
_os2_command(char *command)
{
	char buffer[1024];
	char *shell;

	if ((shell = getenv("COMSPEC")) != NULL)
	{
		sprintf(buffer, "/C %s", command);
		return spawnlp(P_NOWAIT, shell, shell, buffer, NULL);
	}
	else
	{
		return -1;
	}
}

static void
close_pipes(void)
{
	if (vile_in != NULL)
	{
		fclose(vile_in);
		vile_in = NULL;
		pipe_p_from_c = NULLHANDLE;
	}
	else if (pipe_p_from_c != NULLHANDLE)
	{
		DosClose(pipe_p_from_c);
		pipe_p_from_c = NULLHANDLE;
	}

	if (vile_out != NULL)
	{
		fclose(vile_out);
		vile_out = NULL;
		pipe_p_to_c = NULLHANDLE;
	}
	else if (pipe_p_to_c != NULLHANDLE)
	{
		DosClose(pipe_p_to_c);
		pipe_p_to_c = NULLHANDLE;
	}

	if (pipe_c_to_p != NULLHANDLE)
	{
		DosClose(pipe_c_to_p);
		pipe_c_to_p = NULLHANDLE;
	}
	if (pipe_c_from_p != NULLHANDLE)
	{
		DosClose(pipe_c_from_p);
		pipe_c_from_p = NULLHANDLE;
	}
}

int
inout_popen(FILE **infile, FILE **outfile, const char *command)
{
	HFILE save_in, save_out, save_err;
	HFILE fd;
	APIRET rc;

	save_in = save_out = save_err = NULLHANDLE;

	/* We have one pipe running from the parent to the child... */
	pipe_p_to_c = pipe_c_from_p = NULLHANDLE;

	/* ...and another running from the child to the parent. */
	pipe_c_to_p = pipe_p_from_c = NULLHANDLE;

	vile_in = vile_out = NULL;

	/* Create the pipes we'll use for IPC. */
	rc = DosCreatePipe(&pipe_p_from_c, &pipe_c_to_p, 4096);
	if (rc != NO_ERROR)
	{
		errno = EOUTOFMEM;
		goto Error;
	}
	rc = DosCreatePipe(&pipe_c_from_p, &pipe_p_to_c, 4096);
	if (rc != NO_ERROR)
	{
		errno = EOUTOFMEM;
		goto Error;
	}

	/* 
	 * Save standard I/O handles.
	 */
	save_in = ~0;
	if (DosDupHandle(0, &save_in) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	save_out = ~0;
	if (DosDupHandle(1, &save_out) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	save_err = ~0;
	if (DosDupHandle(2, &save_err) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	/*
	 * Redirect standard file handles for the CHILD PROCESS.
	 */

	/* Get standard input from the read end of the p_to_c pipe. */
	fd = 0;
	if (DosDupHandle(pipe_c_from_p, &fd) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	/* Send standard output to the write end of the c_to_p pipe. */
	fd = 1;
	if (DosDupHandle(pipe_c_to_p, &fd) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	/* Send error output to the write end of the c_to_p pipe. */
	fd = 2;
	if (DosDupHandle(pipe_c_to_p, &fd) != NO_ERROR)
	{
		DupError(rc);
		goto Error;
	}

	/*
	 * Ensure that the p_to_c pipe will close cleanly when the parent
	 * process (vile) is done with it.
	 */
	if (DosSetFHState(pipe_p_to_c, OPEN_FLAGS_NOINHERIT) != NO_ERROR)
		goto Error;

	/* Launch the command. */
	if ((pid = _os2_command((char *)command)) < 0)
		goto Error;

	/*
	 * Ensure that the c_to_p pipe will close cleanly when the child
	 * process is done with it.
	 */
	DosClose(pipe_c_to_p);
	pipe_c_to_p = NULLHANDLE;

	/* Connect the read end of the c_to_p pipe to a stream. */
	if (infile != NULL 
		&& (vile_in = *infile = _fdopen(pipe_p_from_c, "r")) == NULL)
		goto Error;

	/* Connect the write end of the p_to_c pipe to a stream. */
	if (outfile != NULL 
		&& (vile_out = *outfile = _fdopen(pipe_p_to_c, "w")) == NULL)
		goto Error;

	/* Restore redirected file handles. */
	if (save_in != NULLHANDLE)
	{
		fd = 0;
		DosDupHandle(save_in, &fd);
	}
	if (save_out != NULLHANDLE)
	{
		fd = 1;
		DosDupHandle(save_out, &fd);
	}
	if (save_err != NULLHANDLE)
	{
		fd = 2;
		DosDupHandle(save_err, &fd);
	}

	return TRUE;

Error:
	/* Restore redirected file handles. */
	if (save_in != NULLHANDLE)
	{
		fd = 0;
		DosDupHandle(save_in, &fd);
	}
	if (save_out != NULLHANDLE)
	{
		fd = 1;
		DosDupHandle(save_out, &fd);
	}
	if (save_err != NULLHANDLE)
	{
		fd = 2;
		DosDupHandle(save_err, &fd);
	}

	close_pipes();
	return FALSE;
}

int
softfork(void)
{
	return FALSE;
}

void
npflush(void)
{
	if (vile_out != NULL)
	{
		fflush(vile_out);
		fclose(vile_out);
		vile_out = NULL;
		pipe_p_to_c = NULLHANDLE;
	}
}

void
npclose(FILE *pipe_file)
{
	int wait_status;

	npflush();
	_cwait(&wait_status, pid, WAIT_CHILD);
	close_pipes();
}
