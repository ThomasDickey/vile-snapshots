/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/fltstack.h,v 1.7 2003/05/20 19:19:57 tom Exp $
 * A simple stack for lex states
 */
typedef struct {
    int state;
} STACK;
static STACK *stk_state = 0;

static int stk_limit = 0;
static int stk_level = -1;

#define FLT_STATE stk_state[stk_level].state

/*
 * Record the given state at the current stack level, and tell lex about it.
 */
static void
new_state(int code)
{
    if (stk_level >= 0)
	FLT_STATE = code;
    BEGIN(code);
}

static void
pop_state(void)
{
#ifdef INITIAL
    int state = INITIAL;
#else
    int state = 0;	/* cater to broken "new" flex */
#endif
    if (--stk_level >= 0 && stk_level < stk_limit)
	state = FLT_STATE;
    new_state(state);
}

static void
push_state(int state)
{
    if (++stk_level >= stk_limit) {
	unsigned have = sizeof(STACK) * stk_limit;
	unsigned want = sizeof(STACK) * (stk_limit += (20 + stk_level));
	stk_state = type_alloc(STACK, (void *) stk_state, want, &have);
    }
    new_state(state);
}

/*
 * Call this from do_filter(), to set the initial state and store that value
 * on the stack.
 */
static void
begin_state(int code)
{
    stk_level = -1;
    push_state(code);
}

/*
 * Cleanup
 */
static void
end_state(void)
{
#if NO_LEAKS
    if (stk_state != 0) {
	free(stk_state);
	stk_state = 0;
    }
#endif
}
