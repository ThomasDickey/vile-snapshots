/*
 * Convert attributed text to ANSI-style escape sequences, allowing color.
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/atr2ansi.c,v 1.2 2003/05/07 19:18:16 tom Exp $
 */
#include <unfilter.h>

#define CSI "\033["

static int last_attrib;

void
begin_unfilter(FILE *dst GCC_UNUSED)
{
    last_attrib = 0;
}

void
markup_unfilter(FILE *dst, int attrib)
{
    if (attrib != 0) {
	fputs(CSI "0", dst);
	if (attrib & ATR_BOLD)
	    fputs(";1", dst);
	if (attrib & (ATR_UNDERLINE | ATR_ITALIC))
	    fputs(";4", dst);
	if (attrib & ATR_REVERSE)
	    fputs(";7", dst);
	if (attrib & ATR_COLOR) {
	    fputs(";3", dst);
	    fputc('0' + (attrib & 7), dst);
	}
	fputc('m', dst);
    } else if (last_attrib != 0) {
	fputs(CSI "0m", dst);
    }
    last_attrib = attrib;
}

void
write_unfilter(FILE *dst, int ch, int attrib GCC_UNUSED)
{
    fputc(ch, dst);
}

void
end_unfilter(FILE *dst GCC_UNUSED)
{
    markup_unfilter(dst, 0);
}
