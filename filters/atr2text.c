/*
 * Convert attributed text to something like nroff output.
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/atr2text.c,v 1.2 2003/05/07 19:47:34 tom Exp $
 */
#include <unfilter.h>

void
begin_unfilter(FILE *dst GCC_UNUSED)
{
}

void
markup_unfilter(FILE *dst GCC_UNUSED, int attrib GCC_UNUSED)
{
}

void
write_unfilter(FILE *dst, int ch, int attrib)
{
    if (isprint(ch)) {
	if (attrib & ATR_BOLD) {
	    fputc(ch, dst);
	    fputc('\b', dst);
	} else if (attrib & (ATR_UNDERLINE | ATR_ITALIC)) {
	    fputc('_', dst);
	    fputc('\b', dst);
	}
    }
    fputc(ch, dst);
}

void
end_unfilter(FILE *dst GCC_UNUSED)
{
}
