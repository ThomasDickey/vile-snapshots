/*
 * $Id: charsets.c,v 1.35 2007/09/03 16:19:22 tom Exp $
 *
 * see
 http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/unicode_42jv.asp
 http://en.wikipedia.org/wiki/Byte_Order_Mark
 http://en.wikipedia.org/wiki/UTF-16
 */

#include <estruct.h>
#include <chgdfunc.h>
#include <edef.h>
#include <nefsms.h>
/* *INDENT-OFF* */
static const UCHAR mark_NONE[]    = { 0x00 };
static const UCHAR mark_UTF8[]    = { 0xef, 0xbb, 0xbf };
static const UCHAR mark_UTF16LE[] = { 0xff, 0xfe };
static const UCHAR mark_UTF16BE[] = { 0xfe, 0xff };
static const UCHAR mark_UTF32LE[] = { 0xff, 0xfe, 0x00, 0x00 };
static const UCHAR mark_UTF32BE[] = { 0x00, 0x00, 0xfe, 0xff };

#define IsNonNull(code) (code == 0xff)

typedef struct {
    BOM_CODES code;
    const UCHAR *mark;
    unsigned size;
} BOM_TABLE;

#define DATA(name) { bom_##name, mark_##name, sizeof(mark_##name) }
static const BOM_TABLE bom_table[] = {
    { bom_NONE, mark_NONE, 0 },
    DATA(UTF8),
    DATA(UTF32LE),	/* must be before UTF-16 entries */
    DATA(UTF32BE),
    DATA(UTF16LE),
    DATA(UTF16BE),
};
#undef DATA
/* *INDENT-ON* */

/******************************************************************************/

static void
allow_decoder(BUFFER *bp, unsigned need)
{
    if (need > bp->decode_utf_len) {
	bp->decode_utf_len = (need + 1) * 2;
	bp->decode_utf_buf = typereallocn(UINT,
					  bp->decode_utf_buf,
					  bp->decode_utf_len);
    }
}

static void
allow_encoder(BUFFER *bp, unsigned need)
{
    if (need > bp->encode_utf_len) {
	bp->encode_utf_len = (need + 1) * 2;
	bp->encode_utf_buf = typereallocn(char,
					  bp->encode_utf_buf,
					  bp->encode_utf_len);
    }
}

int
vl_conv_to_utf8(UCHAR * target, UINT source, B_COUNT limit)
{
#define CH(n) (UCHAR)((source) >> ((n) * 8))
    int rc = 0;

    if (source <= 0x0000007f)
	rc = 1;
    else if (source <= 0x000007ff)
	rc = 2;
    else if (source <= 0x0000ffff)
	rc = 3;
    else if (source <= 0x001fffff)
	rc = 4;
    else if (source <= 0x03ffffff)
	rc = 5;
    else			/* (source <= 0x7fffffff) */
	rc = 6;

    if ((B_COUNT) rc > limit) {	/* whatever it is, we cannot decode it */
	TRACE2(("limit failed %d/%ld in vl_conv_to_utf8\n", rc, limit));
	rc = 0;
    }

    if (target != 0) {
	switch (rc) {
	case 1:
	    target[0] = (UCHAR) CH(0);
	    break;

	case 2:
	    target[1] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[0] = (UCHAR) (0xc0 | (CH(0) >> 6) | ((CH(1) & 0x07) << 2));
	    break;

	case 3:
	    target[2] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[1] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[0] = (UCHAR) (0xe0 | ((CH(1) & 0xf0) >> 4));
	    break;

	case 4:
	    target[3] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[2] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[1] = (UCHAR) (0x80 | ((CH(1) & 0xf0) >> 4) | ((CH(2) &
								  0x03) << 4));
	    target[0] = (UCHAR) (0xf0 | ((CH(2) & 0x1f) >> 2));
	    break;

	case 5:
	    target[4] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[3] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[2] = (UCHAR) (0x80 | ((CH(1) & 0xf0) >> 4) | ((CH(2) &
								  0x03) << 4));
	    target[1] = (UCHAR) (0x80 | (CH(2) >> 2));
	    target[0] = (UCHAR) (0xf8 | (CH(3) & 0x03));
	    break;

	case 6:
	    target[5] = (UCHAR) (0x80 | (CH(0) & 0x3f));
	    target[4] = (UCHAR) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	    target[3] = (UCHAR) (0x80 | (CH(1) >> 4) | ((CH(2) & 0x03) << 4));
	    target[2] = (UCHAR) (0x80 | (CH(2) >> 2));
	    target[1] = (UCHAR) (0x80 | (CH(3) & 0x3f));
	    target[0] = (UCHAR) (0xfc | ((CH(3) & 0x40) >> 6));
	    break;
	}
	TRACE2(("decode %#08x %02X.%02X.%02X.%02X %d:%.*s\n", source,
		CH(3), CH(2), CH(1), CH(0), rc, rc, target));
    }

    return rc;			/* number of bytes needed in target */
#undef CH
}

int
vl_conv_to_utf32(UINT * target, const char *source, B_COUNT limit)
{
#define CH(n) (UCHAR)((*target) >> ((n) * 8))
    int rc = 0;
    int j;
    UINT mask = 0;

    /*
     * Find the number of bytes we will need from the source.
     */
    if ((*source & 0x80) == 0) {
	rc = 1;
	mask = (UINT) * source;
    } else if ((*source & 0xe0) == 0xc0) {
	rc = 2;
	mask = (UINT) (*source & 0x1f);
    } else if ((*source & 0xf0) == 0xe0) {
	rc = 3;
	mask = (UINT) (*source & 0x0f);
    } else if ((*source & 0xf8) == 0xf0) {
	rc = 4;
	mask = (UINT) (*source & 0x07);
    } else if ((*source & 0xfc) == 0xf8) {
	rc = 5;
	mask = (UINT) (*source & 0x03);
    } else if ((*source & 0xfe) == 0xfc) {
	rc = 6;
	mask = (UINT) (*source & 0x01);
    }

    if ((B_COUNT) rc > limit) {	/* whatever it is, we cannot decode it */
	TRACE2(("limit failed %d/%ld in vl_conv_to_utf32\n", rc, limit));
	rc = 0;
    }

    /*
     * sanity-check.
     */
    if (rc > 1) {
	for (j = 1; j < rc; j++) {
	    if ((source[j] & 0xc0) != 0x80)
		break;
	}
	if (j != rc) {
	    rc = 1;
	}
    }

    if (target != 0) {
	int shift = 0;
	*target = 0;
	for (j = 1; j < rc; j++) {
	    *target |= (UINT) (source[rc - j] & 0x3f) << shift;
	    shift += 6;
	}
	*target |= mask << shift;

	TRACE2(("encode %2d:%.*s -> %#08x %02X.%02X.%02X.%02X\n",
		rc, rc, source,
		*target,
		CH(3), CH(2), CH(1), CH(0)));
    }
    return rc;
#undef CH
}

static const BOM_TABLE *
find_mark_info(int code)
{
    const BOM_TABLE *result = 0;
    unsigned n;

    for (n = 0; n < TABLESIZE(bom_table); ++n) {
	const BOM_TABLE *mp = bom_table + n;
	if (mp->code == (BOM_CODES) code) {
	    result = mp;
	    break;
	}
    }
    return result;
}

static BOM_CODES
get_bom(BUFFER *bp)
{
    BOM_CODES rc = b_val(bp, VAL_BYTEORDER_MARK);
    if (rc == bom_NONE)
	rc = bp->implied_BOM;
    return rc;
}

static int
line_has_mark(const BOM_TABLE * mp, UCHAR * buffer, B_COUNT length)
{
    int result = FALSE;

    if (length >= mp->size
	&& mp->size != 0
	&& memcmp(buffer, mp->mark, mp->size) == 0) {
	result = TRUE;
    }
    return result;
}

static int
dump_as_utfXX(BUFFER *bp, const char *buf, int nbuf, const char *ending)
{
    int rc = 0;
    const BOM_TABLE *mp = find_mark_info(get_bom(bp));

    if (mp != 0 && mp->size > 1) {
	unsigned j = 0;
	unsigned k = 0;
	unsigned need = nbuf + strlen(ending);
	unsigned lend = strlen(ending);

	allow_encoder(bp, need * mp->size);
	allow_decoder(bp, need);
	while (j < (unsigned) nbuf) {
	    int skip = vl_conv_to_utf32(bp->decode_utf_buf + k++,
					buf + j,
					nbuf - j);
	    j += skip;
	}
	while (*ending != 0) {
	    (void) vl_conv_to_utf32(bp->decode_utf_buf + k++,
				    ending++,
				    lend--);
	}
	need = k;

	for (j = k = 0; k < need; j += mp->size, ++k) {
	    switch (mp->code) {
	    case bom_NONE:
		/* FALLTHRU */
	    case bom_UTF8:
		/* ignored */
		break;
	    case bom_UTF16LE:
		bp->encode_utf_buf[j + 0] = bp->decode_utf_buf[k];
		bp->encode_utf_buf[j + 1] = bp->decode_utf_buf[k] >> 8;
		break;
	    case bom_UTF16BE:
		bp->encode_utf_buf[j + 1] = bp->decode_utf_buf[k];
		bp->encode_utf_buf[j + 0] = bp->decode_utf_buf[k] >> 8;
		break;
	    case bom_UTF32LE:
		bp->encode_utf_buf[j + 0] = bp->decode_utf_buf[k];
		bp->encode_utf_buf[j + 1] = bp->decode_utf_buf[k] >> 8;
		bp->encode_utf_buf[j + 2] = bp->decode_utf_buf[k] >> 16;
		bp->encode_utf_buf[j + 3] = bp->decode_utf_buf[k] >> 24;
		break;
	    case bom_UTF32BE:
		bp->encode_utf_buf[j + 3] = bp->decode_utf_buf[k];
		bp->encode_utf_buf[j + 2] = bp->decode_utf_buf[k] >> 8;
		bp->encode_utf_buf[j + 1] = bp->decode_utf_buf[k] >> 16;
		bp->encode_utf_buf[j + 0] = bp->decode_utf_buf[k] >> 24;
		break;
	    }
	}
	rc = j;
    }
    return rc;
}

static int
load_as_utf8(BUFFER *bp, LINE *lp)
{
    int rc = FALSE;
    const BOM_TABLE *mp = find_mark_info(get_bom(bp));

    if (mp != 0 && mp->size > 1) {
	int pass;
	unsigned j, k;
	unsigned need = llength(lp);
	unsigned used;

	allow_decoder(bp, need);
	if (bp->decode_utf_buf != 0) {
	    if (need) {
		for (j = k = 0; j < need; ++k) {
		    UCHAR ch = (UCHAR) lgetc(lp, j);
		    if (ch == '\r' || ch == '\n') {
			bp->decode_utf_buf[k] = ch;
			++j;	/* see remove_crlf_nulls() */
			continue;
		    }
		    switch (mp->code) {
		    case bom_NONE:
			/* FALLTHRU */
		    case bom_UTF8:
			/* ignored */
			break;
		    case bom_UTF16LE:
			bp->decode_utf_buf[k] = (lgetc(lp, j)
						 + (lgetc(lp, j + 1) << 8));
			break;
		    case bom_UTF16BE:
			bp->decode_utf_buf[k] = (lgetc(lp, j + 1)
						 + (lgetc(lp, j) << 8));
			break;
		    case bom_UTF32LE:
			bp->decode_utf_buf[k] = (lgetc(lp, j + 0)
						 + (lgetc(lp, j + 1) << 8)
						 + (lgetc(lp, j + 2) << 16)
						 + (lgetc(lp, j + 3) << 24));
			break;
		    case bom_UTF32BE:
			bp->decode_utf_buf[k] = (lgetc(lp, j + 3)
						 + (lgetc(lp, j + 2) << 8)
						 + (lgetc(lp, j + 1) << 16)
						 + (lgetc(lp, j + 0) << 24));
			break;
		    }
		    j += mp->size;
		}
		used = k;

		for (pass = 1; pass <= 2; ++pass) {
		    UCHAR *buffer = (pass == 1) ? 0 : (UCHAR *) lvalue(lp);
		    for (j = k = 0; j < used; ++j) {
			int nn = vl_conv_to_utf8(buffer,
						 bp->decode_utf_buf[j],
						 used - j);
			if (buffer != 0)
			    buffer += nn;
			k += nn;
		    }
		    if (pass == 1) {
			TRACE2(("need %d, have %d\n", k, lp->l_size));
			if ((int) k > llength(lp)) {
			    char *ntext;

			    /*
			     * We are doing this conversion on the initial load
			     * of the buffer, do not want to allow undo.  Just
			     * go ahead and reallocate the line's text buffer.
			     */
			    if ((ntext = castalloc(char, k)) == NULL) {
				rc = FALSE;
				break;
			    }
			    ltextfree(lp, bp);
			    lvalue(lp) = ntext;
			    lp->l_size = k;
			    llength(lp) = k;
			} else {
			    llength(lp) = k;
			}
		    }
		}
	    }
	    rc = TRUE;
	} else {
	    bp->decode_utf_len = 0;
	}
    }
    return rc;
}

/*
 * Remove the extra nulls (if any - according to the encoding) after
 * \r and \n bytes.  This is done to make the existing logic for checking
 * recordseparator work without change.
 */
static void
remove_crlf_nulls(BUFFER *bp, UCHAR * buffer, B_COUNT * length)
{
    const BOM_TABLE *mp = find_mark_info(get_bom(bp));
    UCHAR mark_cr[4];
    UCHAR mark_lf[4];
    unsigned marklen = 0;

    memset(mark_cr, 0, sizeof(mark_cr));
    memset(mark_lf, 0, sizeof(mark_lf));

    switch (mp->code) {
    case bom_NONE:
	/* FALLTHRU */
    case bom_UTF8:
	/* ignored */
	break;
    case bom_UTF16LE:
	marklen = 2;
	mark_cr[0] = '\r';
	mark_lf[0] = '\n';
	break;
    case bom_UTF16BE:
	marklen = 2;
	mark_cr[1] = '\r';
	mark_lf[1] = '\n';
	break;
    case bom_UTF32LE:
	marklen = 4;
	mark_cr[0] = '\r';
	mark_lf[0] = '\n';
	break;
    case bom_UTF32BE:
	marklen = 4;
	mark_cr[3] = '\r';
	mark_lf[3] = '\n';
	break;
    }
    if (marklen != 0) {
	B_COUNT dst = 0;
	B_COUNT src = 0;
	char skip = 0;
	while (src < *length) {
	    if (!memcmp(mark_cr, buffer + src, marklen))
		skip = '\r';
	    else if (!memcmp(mark_lf, buffer + src, marklen))
		skip = '\n';
	    if (skip) {
		buffer[dst++] = skip;
		skip = 0;
	    } else {
		memcpy(buffer + dst, buffer + src, marklen);
		dst += marklen;
	    }
	    src += marklen;
	}
	*length = dst;
    }
}

/*
 * Returns a percentage for the number of cells in the buffer which are
 * explained by interpreting them according to the given byte-order mark
 * pattern with the assumption that most of the content is ASCII or ISO-8859-1
 * (8-bits).
 */
static int
riddled_buffer(const BOM_TABLE * mp, UCHAR * buffer, B_COUNT length)
{
    int result = 0;
    B_COUNT total = 0;
    int offset = 0;
    unsigned j, k;

    if (mp->size && !(mp->size % 2)) {
	TRACE(("checking if %s / %d-byte\n",
	       choice_to_name(&fsm_byteorder_mark_blist,
			      mp->code),
	       mp->size));

	/* Check the line-length.  If it is not a multiple of the pattern
	 * size, just give up.
	 */
	if ((length + offset) % mp->size) {
	    TRACE(("length %ld vs pattern %d - give up\n", length, mp->size));
	} else {
	    /*
	     * Now walk through the line and measure the pattern against it.
	     */
	    for (j = offset; j < (unsigned) length; j += mp->size) {
		int found = 1;
		for (k = 0; k < mp->size; ++k) {
		    UCHAR have = buffer[j + k];
		    UCHAR want = (UCHAR) IsNonNull(mp->mark[k]);
		    if (!have ^ !want) {
			found = 0;
			break;
		    }
		}
		if (found) {
		    total += mp->size;
		}
	    }
	}
	result = (int) ((100.0 * total) / length);

	TRACE(("...%ld/%ld ->%d%%\n", total, length, result));
    }
    return result;
}

static void
set_byteorder_mark(BUFFER *bp, int value)
{
    if (value != ENUM_UNKNOWN
	&& value != global_b_val(VAL_BYTEORDER_MARK)) {
	make_local_b_val(bp, VAL_BYTEORDER_MARK);
	set_b_val(bp, VAL_BYTEORDER_MARK, value);
    }
}

static void
set_encoding(BUFFER *bp, int value)
{
    if (value != ENUM_UNKNOWN
	&& value != global_b_val(VAL_FILE_ENCODING)) {
	make_local_b_val(bp, VAL_FILE_ENCODING);
	set_b_val(bp, VAL_FILE_ENCODING, value);
    }
}

static void
set_encoding_from_bom(BUFFER *bp, int bom_value)
{
    const BOM_TABLE *mp;
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
	set_encoding(bp, result);
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

/******************************************************************************/

int
aligned_charset(BUFFER *bp, UCHAR * buffer, B_COUNT * length)
{
    int rc = FALSE;
    const BOM_TABLE *mp = find_mark_info(get_bom(bp));

    (void) buffer;
    if (mp != 0 && mp->size > 1) {
	rc = !(*length % mp->size);
    }
    return rc;
}

int
cleanup_charset(BUFFER *bp, UCHAR * buffer, B_COUNT * length)
{
    remove_crlf_nulls(bp, buffer, length);
    return TRUE;
}

/*
 * Call this once after reading the buffer (or the first line).
 * But do it before deducing the majormode (to avoid conflict with "preamble").
 *
 * It checks if the byteorder-mark is "auto", and if so, looks at the
 * line to determine what value to use.  It sets the local buffer mode
 * for the result.
 *
 * Having a value other than "none", it then modifies the first line,
 * stripping the BOM bytes.
 */
int
decode_bom(BUFFER *bp, UCHAR * buffer, B_COUNT * length)
{
    const BOM_TABLE *mp;
    int code = FALSE;
    int result;
    unsigned n;

    TRACE((T_CALLED "decode_bom(%s) length %ld\n", bp->b_bname, *length));

    if (b_val(bp, VAL_BYTEORDER_MARK) == ENUM_UNKNOWN) {
	result = bom_NONE;
	for (n = 0; n < TABLESIZE(bom_table); ++n) {
	    mp = bom_table + n;
	    if (line_has_mark(mp, buffer, *length)) {
		result = mp->code;
		TRACE(("...matched %d\n", result));
		break;
	    }
	}
	set_byteorder_mark(bp, result);
    }

    if (b_val(bp, VAL_BYTEORDER_MARK) > bom_NONE
	&& (mp = find_mark_info(b_val(bp, VAL_BYTEORDER_MARK))) != 0
	&& line_has_mark(mp, buffer, *length)) {
	for (n = 0; n < *length - mp->size; ++n) {
	    buffer[n] = buffer[n + mp->size];
	}
	*length -= mp->size;

	set_encoding_from_bom(bp, b_val(bp, VAL_BYTEORDER_MARK));
	code = TRUE;
    }
    returnCode(code);
}

/*
 * Rewrite the line from UTF-16 or UTF-32 into UTF-8.
 * That may increase the number of bytes used to store the data.
 */
int
decode_charset(BUFFER *bp, LINE *lp)
{
    int rc = FALSE;

    if (b_val(bp, VAL_FILE_ENCODING) == enc_UTF16
	|| b_val(bp, VAL_FILE_ENCODING) == enc_UTF32) {
	rc = load_as_utf8(bp, lp);
    }
    return rc;
}

/*
 * Check if we have an explicit encoding.  If not, inspect the buffer contents
 * to decide what encoding to use.
 *
 * By observation, some UTF-16 files written by other editors have no BOM.  It
 * is possible that UTF-32 files may be missing a BOM as well.  We can
 * determine this by seeing if the file is riddled with nulls (in the right
 * pattern of course).  If we find a match for one of these, recode the buffer
 * into UTF-8.
 *
 * If the encoding is unknown or 8-bit, we can inspect the buffer to see if it
 * makes more sense as UTF-8.
 */
int
deduce_charset(BUFFER *bp, UCHAR * buffer, B_COUNT * length, int always)
{
    int rc = FALSE;

    TRACE((T_CALLED "deduce_charset(%s) bom:%s, encoding:%s\n",
	   bp->b_bname,
	   choice_to_name(&fsm_byteorder_mark_blist,
			  b_val(bp, VAL_BYTEORDER_MARK)),
	   choice_to_name(&fsm_file_encoding_blist,
			  b_val(bp, VAL_FILE_ENCODING))));

    bp->implied_BOM = bom_NONE;
    if (b_val(bp, VAL_FILE_ENCODING) == enc_DEFAULT) {
	unsigned n;
	int match = 0;
	int found = -1;

	for (n = 0; n < TABLESIZE(bom_table); ++n) {
	    int check = riddled_buffer(&bom_table[n], buffer, *length);
	    if (check > match) {
		match = check;
		found = n;
	    }
	}
	if (found > 0 && match >= b_val(bp, VAL_PERCENT_UTF8)) {
	    bp->implied_BOM = bom_table[found].code;
	    set_encoding_from_bom(bp, bp->implied_BOM);
	    TRACE(("...found_charset %s\n",
		   choice_to_name(&fsm_byteorder_mark_blist,
				  get_bom(bp))));
	    rc = TRUE;
	} else if (always) {
	    TRACE(("...try looking for UTF-8\n"));
	    if (check_utf8(buffer, *length) == TRUE)
		found_utf8(bp);
	}
    } else {
	rc = TRUE;
    }
    remove_crlf_nulls(bp, buffer, length);
    returnCode(rc);
}

/*
 * Check if the given buffer should be treated as UTF-8.
 * For UTF-8, we have to have _some_ UTF-8 encoding, and _all_
 * of the buffer has to match the pattern.
 */
int
check_utf8(UCHAR * buffer, B_COUNT length)
{
    unsigned n;
    int check = TRUE;
    int skip = 0;
    int found;
    UINT target;

    for (n = found = 0; n < length - 1; n += skip) {
	skip = vl_conv_to_utf32(&target,
				(char *) (buffer + n),
				length - n);
	if (skip == 0) {
	    check = FALSE;
	    break;
	} else if (skip > 1) {
	    found = 1;
	}
    }
    return ((check && found)
	    ? TRUE
	    : (check
	       ? SORTOFTRUE
	       : FALSE));
}

/*
 * If we found UTF-8 encoding, set the buffer to match.
 */
void
found_utf8(BUFFER *bp)
{
    TRACE(("...found UTF-8\n"));
    bp->implied_BOM = bom_UTF8;
    set_encoding_from_bom(bp, bp->implied_BOM);
}

/*
 * encode BOM while writing file, without modifying the buffer.
 */
int
write_bom(BUFFER *bp)
{
    const BOM_TABLE *mp;
    int status = FIOSUC;

    if ((mp = find_mark_info(b_val(bp, VAL_BYTEORDER_MARK))) != 0) {
	status = ffputline((const char *) mp->mark, mp->size, NULL);
    }
    return status;
}

/*
 * encode the UTF-8 text into UTF-16 or UTF-32, according to the buffer's
 * file-encoding mode.
 */
int
encode_charset(BUFFER *bp, const char *buf, int nbuf, const char *ending)
{
    int rc = 0;

    if (b_val(bp, VAL_FILE_ENCODING) == enc_UTF16
	|| b_val(bp, VAL_FILE_ENCODING) == enc_UTF32) {
	rc = dump_as_utfXX(bp, buf, nbuf, ending ? ending : "");
    }
    return rc;
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
    if (testing) {
	;
    } else {
	if (!glob_vals) {
	    set_bom_from_encoding(bp, args->local->vp->i);
	}
	set_bufflags(glob_vals, WFHARD | WFMODE);
    }
    return TRUE;
}
