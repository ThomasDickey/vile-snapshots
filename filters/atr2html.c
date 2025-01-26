/*
 * Convert attributed text to html.
 *
 * $Id: atr2html.c,v 1.13 2025/01/26 10:43:31 tom Exp $
 */
#include <unfilter.h>

static int last_attrib;
static int last_column;

#define THIS_PROGRAM "atr2html"

/*
 * yes - the title is empty...
 * (use it as a placeholder for post-processing)
 */
void
begin_unfilter(FILE *dst)
{
    fprintf(dst, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n");
    fprintf(dst, "<html>\n");
    fprintf(dst, "<head>\n");
    fprintf(dst, "<meta name=\"generator\" content=\"%s\">\n", THIS_PROGRAM);
    fprintf(dst, "<meta http-equiv=\"Content-Type\" "
	    "content=\"text/html; charset=us-ascii\">\n");
    fprintf(dst, "<title></title>\n");
    fprintf(dst, "</head>\n");
    fprintf(dst, "<body>\n");
    fprintf(dst,
	    "<!--{{%s--><p style=\"font-family: monospace; font-size: 10pt;\">\n", THIS_PROGRAM);
    last_attrib = 0;
    last_column = 0;
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
#define NBSP "&nbsp;"
    static const char *tabstrip = NBSP NBSP NBSP NBSP NBSP NBSP NBSP NBSP;
    int next_adjust;
    int tabs_adjust = 0;
    const char *alias = NULL;
    char buffer[8];

    (void) attrib;

    if (ch == '\t') {
	next_adjust = (last_column | 7) + 1;
	tabs_adjust = next_adjust - last_column;
	last_column = next_adjust;
    } else {
	++last_column;
    }
    switch (ch) {
    case '\t':
	alias = tabstrip + ((8 - tabs_adjust) * ((int) sizeof(NBSP) - 1));
	break;
    case '\n':
	if (last_column > 1)
	    alias = "<br>\n";
	else
	    alias = NBSP "<br>\n";
	last_column = 0;
	break;
    case ' ':
	alias = NBSP;
	break;
    case '<':
	alias = "&lt;";
	break;
    case '>':
	alias = "&gt;";
	break;
    case '&':
	alias = "&amp;";
	break;
    default:
	if (ch < 32) {
	    sprintf(buffer, "^%c", ch + '@');
	    alias = buffer;
	}
	break;
    }
    if (alias != NULL) {
	vl_fputs(alias, dst);
    } else {
	vl_putc(ch, dst);
    }
}

void
end_unfilter(FILE *dst)
{
    markup_unfilter(dst, 0);
    fprintf(dst, "<!--%s}}--></p>\n</body></html>", THIS_PROGRAM);
}
