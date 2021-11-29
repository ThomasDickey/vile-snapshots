/*
 * Convert attributed text to ANSI-style escape sequences, allowing color.
 *
 * $Id: atr2ansi.c,v 1.5 2007/05/05 15:23:03 tom Exp $
 */
#include <unfilter.h>

#define CSI "\033["

static int last_attrib;

void
begin_unfilter(FILE *dst GCC_UNUSED)
{
    (void) dst;

    last_attrib = 0;
}

void
markup_unfilter(FILE *dst, int attrib)
{
    if (attrib != 0) {
	vl_fputs(CSI "0", dst);
	if (attrib & ATR_BOLD)
	    vl_fputs(";1", dst);
	if (attrib & (ATR_UNDERLINE | ATR_ITALIC))
	    vl_fputs(";4", dst);
	if (attrib & ATR_REVERSE)
	    vl_fputs(";7", dst);
	if (attrib & ATR_COLOR) {
	    vl_fputs(";3", dst);
	    vl_putc('0' + (attrib & 7), dst);
	}
	vl_putc('m', dst);
    } else if (last_attrib != 0) {
	vl_fputs(CSI "0m", dst);
    }
    last_attrib = attrib;
}

void
write_unfilter(FILE *dst, int ch, int attrib GCC_UNUSED)
{
    (void) attrib;

    vl_putc(ch, dst);
}

void
end_unfilter(FILE *dst GCC_UNUSED)
{
    (void) dst;

    markup_unfilter(dst, 0);
}
