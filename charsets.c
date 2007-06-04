/*
 * $Id: charsets.c,v 1.10 2007/06/04 00:03:43 tom Exp $
 *
 * see
 http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/unicode_42jv.asp
 http://en.wikipedia.org/wiki/Byte_Order_Mark
 http://en.wikipedia.org/wiki/UTF-16
 */

#include <estruct.h>
#include <chgdfunc.h>
#include <edef.h>

#define DATA(name) { bom_##name, mark_##name, sizeof(mark_##name) }
/* *INDENT-OFF* */
static const char mark_UTF8[]    = { (char)0xef, (char)0xbb, (char)0xbf };
static const char mark_UTF16LE[] = { (char)0xff, (char)0xfe };
static const char mark_UTF16BE[] = { (char)0xfe, (char)0xff };
static const char mark_UTF32LE[] = { (char)0xff, (char)0xfe, (char)0x00, (char)0x00 };
static const char mark_UTF32BE[] = { (char)0x00, (char)0x00, (char)0xfe, (char)0xff };

typedef struct {
    BOM_CODES code;
    const char *mark;
    unsigned size;
} BOM_TABLE;

static BOM_TABLE bom_table[] = {
    { bom_NONE, "", 0 },
    DATA(UTF8),
    DATA(UTF32LE),	/* before UTF-16 entries */
    DATA(UTF32BE),
    DATA(UTF16LE),
    DATA(UTF16BE),
};
/* *INDENT-ON* */

static BOM_TABLE *
find_mark_info(int code)
{
    BOM_TABLE *result = 0;
    unsigned n;

    for (n = 0; n < TABLESIZE(bom_table); ++n) {
	BOM_TABLE *mp = bom_table + n;
	if (mp->code == (BOM_CODES) code) {
	    result = mp;
	    break;
	}
    }
    return result;
}

static int
line_has_mark(LINE *lp, BOM_TABLE * mp)
{
    int result = FALSE;

    if (llength(lp) >= (int) mp->size
	&& mp->size != 0
	&& memcmp(lp->l_text, mp->mark, mp->size) == 0) {
	result = TRUE;
    }
    return result;
}

static void
set_encoding_from_bom(BUFFER *bp, int bom_value)
{
    BOM_TABLE *mp;
    int result;

    if (bom_value > bom_NONE
	&& (mp = find_mark_info(bom_value)) != 0) {

	switch (mp->code) {
	case bom_UTF8:
	    result = enc_UTF8;
	    break;
	case bom_UTF16LE:
	case bom_UTF16BE:
	    result = enc_UTF16;
	    break;
	case bom_UTF32LE:
	case bom_UTF32BE:
	    result = enc_UTF32;
	    break;
	default:
	    result = ENUM_UNKNOWN;
	    break;
	}
	if (result != ENUM_UNKNOWN
	    && result != global_b_val(VAL_FILE_ENCODING)) {
	    make_local_b_val(bp, VAL_FILE_ENCODING);
	    set_b_val(bp, VAL_FILE_ENCODING, result);
	}
    }
}

static void
set_bom_from_encoding(BUFFER *bp, int enc_value)
{
    int result = b_val(bp, VAL_BYTEORDER_MARK);

    if (result > bom_NONE) {
	switch (enc_value) {
	case enc_UTF8:
	case enc_UTF16:
	case enc_UTF32:
	    break;
	default:
	    if (result != ENUM_UNKNOWN
		&& result != global_b_val(VAL_BYTEORDER_MARK)) {
		make_local_b_val(bp, VAL_BYTEORDER_MARK);
		set_b_val(bp, VAL_BYTEORDER_MARK, bom_NONE);
	    }
	    break;
	}
    }
}

/*
 * Call this once after reading the buffer (or the first line).
 * But do it before deducing the majormode.
 *
 * It checks if the byteorder-mark is "auto", and if so, looks at the
 * line to determine what value to use.  It sets the local buffer mode
 * for the result.
 *
 * Having a value other than "none", it then modifies the first line,
 * stripping the BOM bytes.
 */
int
decode_bom(BUFFER *bp)
{
    BOM_TABLE *mp;
    LINE *lp = lforw(buf_head(bp));
    int code = FALSE;
    int result;
    unsigned n;

    TRACE(("decode_bom(%s)\n", bp->b_bname));

    if (b_val(bp, VAL_BYTEORDER_MARK) == ENUM_UNKNOWN) {
	result = bom_NONE;
	for (n = 0; n < TABLESIZE(bom_table); ++n) {
	    mp = bom_table + n;
	    if (line_has_mark(lp, mp)) {
		result = mp->code;
		TRACE(("...matched %d\n", result));
		break;
	    }
	}
	if (result != global_b_val(VAL_BYTEORDER_MARK)) {
	    make_local_b_val(bp, VAL_BYTEORDER_MARK);
	    set_b_val(bp, VAL_BYTEORDER_MARK, result);
	}
    }

    if (b_val(bp, VAL_BYTEORDER_MARK) > bom_NONE
	&& (mp = find_mark_info(b_val(bp, VAL_BYTEORDER_MARK))) != 0
	&& line_has_mark(lp, mp)) {
	for (n = 0; n < llength(lp) - mp->size; ++n) {
	    lp->l_text[n] = lp->l_text[n + mp->size];
	}
	llength(lp) -= mp->size;

	set_encoding_from_bom(bp, b_val(bp, VAL_BYTEORDER_MARK));
	code = TRUE;
    }
    return code;
}

/*
 * encode BOM while writing file, without modifying the buffer.
 */
int
write_bom(BUFFER *bp)
{
    BOM_TABLE *mp;
    int status = FIOSUC;

    if ((mp = find_mark_info(b_val(bp, VAL_BYTEORDER_MARK))) != 0) {
	status = ffputline(mp->mark, mp->size, NULL);
    }
    return status;
}

/*
 * if byteorder mark changes, ensure that file-encoding is set compatibly.
 */
int
chgd_byteorder(BUFFER *bp,
	       VALARGS * args,
	       int glob_vals,
	       int testing)
{
    if (!testing && !glob_vals) {
	set_encoding_from_bom(bp, args->local->vp->i);
    }
    return TRUE;
}

/*
 * If file-encoding changes to non-UTF-8, set byteorder-mark to none.
 * Only keep it set if changing from one UTF-encoding to another.
 */
int
chgd_fileencode(BUFFER *bp,
		VALARGS * args,
		int glob_vals,
		int testing)
{
    if (!testing && !glob_vals) {
	set_bom_from_encoding(bp, args->local->vp->i);
    }
    return TRUE;
}
