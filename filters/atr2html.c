/*
 * Convert attributed text to html.
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/atr2html.c,v 1.5 2009/10/07 09:23:07 tom Exp $
 */
#include <unfilter.h>

static int last_attrib;

/*
 * yes - the title is empty...
 */
void
begin_unfilter(FILE *dst)
{
    fprintf(dst, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n");
    fprintf(dst, "<html><head><title></title></head><body><pre>\n");
    last_attrib = 0;
}

#define OPEN(mask)  ( (attrib & (mask)) && !(last_attrib & (mask)))
#define CLOSE(mask) (!(attrib & (mask)) &&  (last_attrib & (mask)))

void
markup_unfilter(FILE *dst, int attrib)
{
    if (last_attrib != 0) {
	if (CLOSE(ATR_COLOR))
	    vl_fputs("</font>", dst);
	if (CLOSE(ATR_UNDERLINE | ATR_ITALIC))
	    vl_fputs("</em>", dst);
	if (CLOSE(ATR_BOLD))
	    vl_fputs("</strong>", dst);
    }
    if (attrib != 0) {
	if (OPEN(ATR_BOLD))
	    vl_fputs("<strong>", dst);
	if (OPEN(ATR_UNDERLINE | ATR_ITALIC))
	    vl_fputs("<em>", dst);
	if (OPEN(ATR_COLOR)) {
	    int r = attrib & 1;
	    int g = attrib & 2;
	    int b = attrib & 4;
	    int z = attrib & 8 ? 0xff : 0x80;
	    fprintf(dst, "<font color=\"#%02X%02X%02X\">",
		    r ? z : 0,
		    g ? z : 0,
		    b ? z : 0);
	}
    }
    last_attrib = attrib;
}

void
write_unfilter(FILE *dst, int ch, int attrib GCC_UNUSED)
{
    const char *alias = 0;

    (void) attrib;

    switch (ch) {
    case '<':
	alias = "&lt;";
	break;
    case '>':
	alias = "&gt;";
	break;
    case '&':
	alias = "&amp;";
	break;
    }
    if (alias != 0) {
	vl_fputs(alias, dst);
    } else {
	vl_putc(ch, dst);
    }
}

void
end_unfilter(FILE *dst)
{
    markup_unfilter(dst, 0);
    fprintf(dst, "</pre></body></html>");
}
