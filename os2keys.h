/*
 * $Header: /users/source/archives/vile.vcs/RCS/os2keys.h,v 1.1 1997/11/30 23:51:25 tom Exp $
 *
 * Function-key definitions used for OS/2 VIO driver, as well as OS/2 EMX
 * driver.
 */

/* Extended key prefix macros. */
#define	KE0(code)		{ 0xe0, code }
#define	K00(code)		{ 0x00, code }

static struct
{
	char seq[2];
	int code;
}
VIO_KeyMap[] =
{
	{ KE0('H'), KEY_Up },
	{ KE0('P'), KEY_Down },
	{ KE0('K'), KEY_Left },
	{ KE0('M'), KEY_Right },
	{ KE0('R'), KEY_Insert },
	{ KE0('S'), KEY_Delete },
	{ KE0('G'), KEY_Home },
	{ KE0('O'), KEY_End },
	{ KE0('I'), KEY_Prior },
	{ KE0('Q'), KEY_Next },

	/*
	 * Unshifted function keys.  The VIO console driver generates
	 * different scan codes when these keys are pressed with Shift,
	 * Ctrl, and Alt; those codes are presently unsupported.
	 */
	{ K00(';'), KEY_F1 },
	{ K00('<'), KEY_F2 },
	{ K00('='), KEY_F3 },
	{ K00('>'), KEY_F4 },
	{ K00('?'), KEY_F5 },
	{ K00('@'), KEY_F6 },
	{ K00('A'), KEY_F7 },
	{ K00('B'), KEY_F8 },
	{ K00('C'), KEY_F9 },
	{ K00('D'), KEY_F10 },
	{ K00(133), KEY_F11 },
	{ K00(134), KEY_F12 },

	/* Keypad codes (with Num Lock off): */
	{ K00('G'), KEY_Home },
	{ K00('H'), KEY_Up },
	{ K00('I'), KEY_Prior },
	{ K00('K'), KEY_Left },
	{ K00('L'), KEY_Select },
	{ K00('M'), KEY_Right },
	{ K00('O'), KEY_End },
	{ K00('P'), KEY_Down },
	{ K00('Q'), KEY_Next },
	{ K00('R'), KEY_Insert },
	{ K00('S'), KEY_Delete }
};
