/*
 * Parsing and I/O support for atr2text, etc.
 *
 * $Id: unfilter.c,v 1.14 2025/01/26 10:43:31 tom Exp $
 */
#define CAN_TRACE 0
#define CAN_VMS_PATH 0
#include <unfilter.h>

typedef enum {
    Default,
    Repeat,
    Attribs,
    Color,
    Markup,
    EatString
} STATES;

typedef struct {
    int length;
    int attrib;
} COUNTS;

static COUNTS *my_counts = NULL;
static size_t my_length = 0;

static void
failed(const char *s)
{
    perror(s);
    exit(BADEXIT);
}

static void
unfilter(FILE *src, FILE *dst)
{
    int ch;
    int count = 0;
    int attrs = 0;
    STATES state = Default;
    size_t n;

    begin_unfilter(dst);
    while ((ch = vl_getc(src)) != EOF) {
	ch = CharOf(ch);
	if (state == Default) {
	    if (ch == CTL_A) {
		state = Repeat;
		count = 0;
		attrs = 0;
	    } else {
		int changed = 0;

		write_unfilter(dst, ch, attrs);

		for (n = 0; n < my_length; ++n) {
		    if (my_counts[n].length > 0) {
			my_counts[n].length -= 1;
			if (my_counts[n].length == 0)
			    changed = 1;
		    }
		}
		if (changed) {
		    attrs = 0;
		    for (n = 0; n < my_length; ++n) {
			if (my_counts[n].length > 0) {
			    attrs |= my_counts[n].attrib & (ATR_BOLD |
							    ATR_UNDERLINE |
							    ATR_REVERSE |
							    ATR_ITALIC);
			    if ((attrs & ATR_COLOR) == 0) {
				attrs |= my_counts[n].attrib & (ATR_COLOR | 0xff);
			    }
			}
		    }
		    markup_unfilter(dst, attrs);
		}
	    }
	} else if (state == EatString) {
	    if (ch == 0)
		state = Default;
	} else if (ch == ':') {
	    if (count == 0)
		count = 1;
	    if (attrs != 0) {
		markup_unfilter(dst, attrs);
		if (my_length == 0) {
		    my_length = 10;
		    if ((my_counts = typecallocn(COUNTS, my_length)) == NULL)
			failed("malloc");
		}
		for (n = 0; n < my_length; ++n) {
		    if (my_counts[n].length == 0) {
			my_counts[n].length = count;
			my_counts[n].attrib = attrs;
			break;
		    }
		}
	    }
	    state = Default;
	} else {
	    switch (state) {
	    case Repeat:
		if (isdigit(ch)) {
		    count = (count * 10) + (ch - '0');
		    break;
		} else {
		    state = Attribs;
		}
		/* FALLTHRU */
	    case Attribs:
		switch (ch) {
		case 'C':
		    state = Color;
		    attrs |= ATR_COLOR;
		    break;
		case 'U':
		    attrs |= ATR_UNDERLINE;
		    break;
		case 'B':
		    attrs |= ATR_BOLD;
		    break;
		case 'R':
		    attrs |= ATR_REVERSE;
		    break;
		case 'I':
		    attrs |= ATR_ITALIC;
		    break;
		case 'H':
		case 'M':
		    state = EatString;
		    break;
		}
		break;
	    case Color:
		if (isxdigit(ch)) {
		    int color = 0;
		    if (isdigit(ch)) {
			color = ch - '0';
		    } else if (isupper(ch) && ch <= 'F') {
			color = ch - 'A' + 10;
		    } else if (islower(ch) && ch <= 'f') {
			color = ch - 'a' + 10;
		    }
		    attrs |= color;
		}
		state = Attribs;
		break;
	    case EatString:
	    case Markup:
	    case Default:
		break;
	    }
	}
    }
    end_unfilter(dst);
}

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    (void) argc;
    (void) argv;

    unfilter(stdin, stdout);
    exit(GOODEXIT);
}
