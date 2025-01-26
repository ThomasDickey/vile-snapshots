/*
 *	control-c and critical error handlers for DJGPP environment
 *
 *	control-c code based exactly on the equivalent handler
 *		for control-break in the go32 libraries
 *	critical error handling from Bob Babcock.
 *
 * $Id: djhandl.c,v 1.9 2025/01/26 21:38:18 tom Exp $
 *
 *
 */

#include <stdlib.h>
#include <go32.h>
#include <dpmi.h>
#include <dos.h>
#include <stdio.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static _go32_dpmi_registers regs;
static volatile unsigned long ctrl_c_count = 0;
static int ctrl_c_hooked = 0;
static _go32_dpmi_seginfo old_vector;
static _go32_dpmi_seginfo new_vector;

unsigned long was_ctrl_c_hit();
void want_ctrl_c(int yes);	/* auto-yes if call above function */

static void
ctrl_c_isr(_go32_dpmi_registers * regs)
{
    ctrl_c_count++;
}

unsigned long
was_ctrl_c_hit(void)
{
    unsigned long cnt;
    want_ctrl_c(1);
    cnt = ctrl_c_count;
    ctrl_c_count = 0;
    return cnt;
}

void
want_ctrl_c(int yes)
{
    if (yes) {
	if (ctrl_c_hooked)
	    return;
	_go32_dpmi_get_real_mode_interrupt_vector(0x23, &old_vector);

	new_vector.pm_offset = (int) ctrl_c_isr;
	_go32_dpmi_allocate_real_mode_callback_iret(&new_vector, &regs);
	_go32_dpmi_set_real_mode_interrupt_vector(0x23, &new_vector);
	ctrl_c_count = 0;
	ctrl_c_hooked = 1;
    } else {
	if (!ctrl_c_hooked)
	    return;
	_go32_dpmi_set_real_mode_interrupt_vector(0x23, &old_vector);
	_go32_dpmi_free_real_mode_callback(&new_vector);
	ctrl_c_count = 0;
	ctrl_c_hooked = 0;
    }
}

/* hardgcc - critical error handler for gcc
 */

int hard_error_occurred = FALSE;

static char handler[16] =
{
    0x81, 0xe7, 0x7f, 0,	/* and di,7f         */
    0x47,			/* inc di            */
    0x2e, 0x89, 0x3e, 0xe, 0,	/* mov cs:[0eh],di   */
    0xb8, 3, 0,			/* mov ax,3          */
    0xcf,			/* iret              */
    0, 0			/* storage for error code */
};
static int handler_installed = FALSE;
static _go32_dpmi_seginfo new_handler_info, old_handler_info;

/* clear_hard_error - clear any indication of a critical error
 */
void
clear_hard_error(void)
{
    short zero = 0;

    if (handler_installed)
	dosmemput(&zero, 2, new_handler_info.rm_segment * 16 + 14);
    hard_error_occurred = 0;
    return;
}

/* install the real-mode critical error handler
 */
void
hard_error_catch_setup(void)
{
/* On first call, allocate some DOS memory and copy the handler into it
 */
    if (!handler_installed) {
	handler_installed = TRUE;
	new_handler_info.size = old_handler_info.size = 1;
	if (_go32_dpmi_allocate_dos_memory(&new_handler_info) != 0) {
	    fprintf(stderr, "Couldn't allocate handler memory\n");
	    exit(1);
	}
	dosmemput(handler, 16, new_handler_info.rm_segment * 16);
#ifdef XDEBUG
	sprintf(Tempbuf, "Handler at segment %x", new_handler_info.rm_segment);
	error_msg(Tempbuf, 0);
#endif
    }
    _go32_dpmi_get_real_mode_interrupt_vector(0x24, &old_handler_info);
    _go32_dpmi_set_real_mode_interrupt_vector(0x24, &new_handler_info);
    clear_hard_error();
    return;
}

/* switch back to old critical error handler
 */
void
hard_error_teardown(void)
{
    if (handler_installed)
	_go32_dpmi_set_real_mode_interrupt_vector(0x24, &old_handler_info);
    return;
}

/* test_hard_err - test whether a critical error has occurred and return
   error code (0 if none or handler never installed).
 */
int
did_hard_error_occur(void)
{
    short error_code;

    if (handler_installed) {
	dosmemget(new_handler_info.rm_segment * 16 + 14, 2, &error_code);
#ifdef XDEBUG
	if (error_code) {
	    sprintf(Tempbuf, "Critical error code is %d", error_code);
	    error_msg(Tempbuf, 0);
	}
#endif
	return (error_code);
    }
    return (0);
}
