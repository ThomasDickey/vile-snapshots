/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/fltstack.h,v 1.5 2002/12/17 01:57:07 tom Exp $
 * A simple stack for lex states
 */
typedef struct {
    int state;
} STACK;
static STACK *stk_state = 0;

static int stk_limit = 0;
static int stk_level = -1;

/*
 * Record the given state at the current stack level, and tell lex about it.
 */
static void
new_state(int code)
{
    if (stk_level >= 0)
	stk_state[stk_level].state = code;
    BEGIN(code);
}

static void
pop_state(void)
{
    int state = INITIAL;
    if (--stk_level >= 0 && stk_level < stk_limit)
	state = stk_state[stk_level].state;
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
