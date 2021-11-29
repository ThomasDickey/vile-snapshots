/*
 * Convert attributed text to something like nroff output.
 *
 * $Id: atr2text.c,v 1.5 2007/05/05 15:14:56 tom Exp $
 */
#include <unfilter.h>

void
begin_unfilter(FILE *dst GCC_UNUSED)
{
    (void) dst;
}

void
markup_unfilter(FILE *dst GCC_UNUSED, int attrib GCC_UNUSED)
{
    (void) dst;
    (void) attrib;
}

void
write_unfilter(FILE *dst, int ch, int attrib)
{
    if (isprint(ch)) {
	if (attrib & ATR_BOLD) {
	    vl_putc(ch, dst);
	    vl_putc('\b', dst);
	} else if (attrib & (ATR_UNDERLINE | ATR_ITALIC)) {
	    vl_putc('_', dst);
	    vl_putc('\b', dst);
	}
    }
    vl_putc(ch, dst);
}

void
end_unfilter(FILE *dst GCC_UNUSED)
{
    (void) dst;
}
