/*
 * Unix crypt(1)-style interface.
 * Written by T.E.Dickey for vile (March 1999).
 *
 * $Id: ucrypt.c,v 1.25 2018/10/25 22:50:17 tom Exp $
 */

#include "estruct.h"
#include "edef.h"

#if	OPT_ENCRYPT

#include "vl_crypt.h"

/*
 * Get the string to use as an encryption string.
 */
static int
get_encryption_key(char *key,	/* where to write key */
		   size_t len)
{
    int status;			/* return status */
    int save_vl_echo = vl_echo;
    char temp[NKEYLEN];

    /* turn command input echo off */
    vl_echo = FALSE;

    temp[0] = EOS;
    status = mlreply("-Encryption String: ", temp, (UINT) (len - 1));
    vl_echo = save_vl_echo;

    if (status == TRUE)
	vl_make_encrypt_key(key, temp);

    mlerase();
    return (status);
}

/*
 * Set the buffer's encryption key if needed.
 */
int
vl_resetkey(BUFFER *bp,
	    const char *fname)
{
    register int s;		/* temporary return value */

    /* flag the buffer as not having encryption */
    ffdocrypt(FALSE);

    /* if we want to encrypt */
    if (b_val(bp, MDCRYPT)) {
	char temp[NFILEN];

	/* don't automatically inherit key from other buffers */
	if (bp->b_cryptkey[0] != EOS
	    && !b_is_argument(bp)
	    && strcmp(lengthen_path(vl_strncpy(temp, fname, sizeof(temp))), bp->b_fname)) {
	    char prompt[80];
	    (void) lsprintf(prompt, "Use crypt-key from %s", bp->b_bname);
	    s = mlyesno(prompt);
	    if (s != TRUE)
		return (s == FALSE);
	}

	/* make a key if we don't have one */
	if (bp->b_cryptkey[0] == EOS) {
	    s = get_encryption_key(bp->b_cryptkey, sizeof(bp->b_cryptkey));
	    if (s != TRUE)
		return (s == FALSE);
	}

	ffdocrypt(TRUE);
	vl_setup_encrypt(bp->b_cryptkey, vl_seed);
    }

    return TRUE;
}

/* change current buffer's encryption key */
/* ARGSUSED */
int
vl_setkey(int f GCC_UNUSED,
	  int n GCC_UNUSED)
{
    char result[NKEYLEN];
    int rc;

    memset(result, 0, sizeof(result));
    rc = get_encryption_key(result, sizeof(result));
    if (rc == TRUE) {
	TRACE(("set key for %s\n", curbp->b_bname));
	(void) vl_strncpy(curbp->b_cryptkey, result, sizeof(curbp->b_cryptkey));
	set_local_b_val(curbp, MDCRYPT, TRUE);
	curwp->w_flag |= WFMODE;
    } else if (rc == FALSE) {
	if (curbp->b_cryptkey[0] != EOS) {
	    rc = mlyesno("Discard encryption key");
	    if (rc == TRUE) {
		TRACE(("reset key for %s\n", curbp->b_bname));
		curbp->b_cryptkey[0] = EOS;
		if (global_b_val(MDCRYPT)) {
		    set_local_b_val(curbp, MDCRYPT, FALSE);
		    curwp->w_flag |= WFMODE;
		} else if (b_val(curbp, MDCRYPT)) {
		    make_global_val(curbp->b_values.bv, global_b_values.bv, MDCRYPT);
		    curwp->w_flag |= WFMODE;
		}
	    }
	}
    }
    return (rc);
}

#endif
