/*
 * $Id: fltstack.h,v 1.23 2025/01/27 23:33:59 tom Exp $
 * A simple stack for lex states
 */

#ifndef FLTSTACK_H
#define FLTSTACK_H 1
/*
 * Redefine since filters.h uses BEGIN(), which confuses our notion of current
 * state.
 */
#undef BeginQuote
#define BeginQuote(state, attr) \
			new_state(state); \
			flt_bfr_begin(attr); \
			flt_bfr_append(yytext, yyleng)

#undef FinishQuote
#define FinishQuote(state) \
			flt_bfr_append(yytext, yyleng);\
			flt_bfr_finish();\
			new_state(state)

typedef struct {
    int state;
#ifdef FLTSTACK_EXTRA
      FLTSTACK_EXTRA
#endif
} STACK;
static STACK *stk_state = NULL;
static int cur_state;

static int stk_limit = 0;
static int stk_level = -1;

#define FLTSTACK_OK   (stk_level >= 0 && stk_level < stk_limit)
#define FLTSTACK_USED (stk_level >= 1 && stk_level < stk_limit)
#define FLTSTACK_THIS stk_state[stk_level]
#define FLT_STATE     FLTSTACK_THIS.state

static void new_state(int);
static void push_state(int);
static void begin_state(int);
static void end_state(void);

#ifndef UNUSED_POP_STATE
static void pop_state(void);
#endif

#else

/*
 * Record the given state at the current stack level, and tell lex about it.
 */
static void
new_state(int code)
{
    FLEX_PRINTF((stderr, "new_state(%d)\n", code));
    if (FLTSTACK_OK && stk_state != NULL)
	FLT_STATE = code;
    BEGIN(code);
    cur_state = code;		/* antique lex's have no valid YYSTATE */
}

#ifndef UNUSED_POP_STATE
static void
pop_state(void)
{
#ifdef INITIAL
    int state = INITIAL;
#else
    int state = 0;		/* cater to broken "new" flex */
#endif
    FLEX_PRINTF((stderr, "pop_state() level %d\n", stk_level));
    --stk_level;
    if (FLTSTACK_OK)
	state = FLT_STATE;
    new_state(state);
}
#endif

static void
push_state(int state)
{
    ++stk_level;
    FLEX_PRINTF((stderr, "push_state(%d) level %d\n", state, stk_level));
    if ((stk_level >= stk_limit) || (stk_state == NULL)) {
	size_t have = sizeof(STACK) * (unsigned) stk_limit;
	size_t want = sizeof(STACK) * (unsigned) (stk_limit += (20 + stk_level));
	stk_state = type_alloc(STACK, (void *) stk_state, want, &have);
    }
#ifdef FLTSTACK_EXTRA_PUSH
    if (FLTSTACK_OK) {
	FLTSTACK_EXTRA_PUSH
    }
#endif
    new_state(state);
}

/*
 * Call this from do_filter(), to set the initial state and store that value
 * on the stack.
 */
static void
begin_state(int code)
{
    FLEX_PRINTF((stderr, "begin_state(%d)\n", code));
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
    if (stk_state != NULL) {
	free(stk_state);
	stk_state = NULL;
    }
#endif
}
#endif /* FLTSTACK_H */
