/*
 * watch.c	-- generic data structures and routines for watching
 *		   file descriptors and timers (eventually)
 */

#include "estruct.h"
#include "edef.h"

typedef struct {
    char     *callback;	/* a vile command to run... */
    long      otherid;	/* e.g, the XtInputId is stored here for x11. */
    WATCHTYPE type;	/* one of WATCHINPUT, WATCHOUTPUT, or WATCHERROR */
} watchrec;

#define NWATCHFDS 256

static watchrec *watchfds[NWATCHFDS];


static void unwatch_dealloc(int fd);
static void unwatch_free_callback(char *callback);

int
watchfd(int fd, WATCHTYPE type, char *callback)
{
    long otherid;
    int status;

    if (watchfds[fd]) {
	/* Already allocated/watched, so deallocate/unwatch */
	unwatchfd(fd);
    }

    watchfds[fd] = typealloc(watchrec);

    if (watchfds[fd] == NULL) {
	unwatch_free_callback(callback);
	return FALSE;
    }

    status = term.watchfd(fd, type, &otherid);

    watchfds[fd]->callback = callback;
    watchfds[fd]->type     = type;
    watchfds[fd]->otherid  = otherid;

    if (status != TRUE) {
	unwatch_dealloc(fd);
    }

    return status;
}

void
unwatchfd(int fd)
{
    if (watchfds[fd] == NULL)
	return;

    term.unwatchfd(fd, watchfds[fd]->otherid);
    unwatch_dealloc(fd);
}

void
dowatchcallback(int fd)
{
    /* Not safe to do one of these callbacks when the user is
       typing on the message line.  FIXME. */
    if (reading_msg_line)
	return;

    if (watchfds[fd] == NULL || watchfds[fd]->callback == NULL)
	return;

    (void) docmd(watchfds[fd]->callback, TRUE, FALSE, 1);
}

static void
unwatch_dealloc(int fd)
{
    if (watchfds[fd] == NULL)
	return;

    unwatch_free_callback(watchfds[fd]->callback);
    FreeAndNull(watchfds[fd]);
}

static void
unwatch_free_callback(char *callback)
{
    if (callback == NULL)
	return;

#if OPT_PERL
    if (strncmp("perl", callback, 4) == 0)
	perl_free_callback(callback);
#endif

    free(callback);
}
