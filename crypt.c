/*	Crypt:	Encryption routines for MicroEMACS
 *		written by Dana Hoggatt and Daniel Lawrence
 *
 * $Header: /users/source/archives/vile.vcs/RCS/crypt.c,v 1.16 1997/01/10 11:07:43 tom Exp $
 *
 */

#include	"estruct.h"
#include	"edef.h"

#if	OPT_ENCRYPT
static	int	mod95 (int val);

int
ue_makekey(			/* make encryption key */
char	*key,			/* where to write key */
ALLOC_T	len)
{
	register int status;	/* return status */
	int odisinp = disinp;	/* original value of disinp */
	char	temp[NPAT];

	/* turn command input echo off */
	disinp = FALSE;

	/* get the string to use as an encryption string */
	temp[0] = EOS;
	status = mlreply("Encryption String: ", temp, (int)len-1);
	disinp = odisinp;

        if (status == TRUE) {
		(void)strcpy(key, temp);

		/* and encrypt it */
		ue_crypt((char *)0, 0);
		ue_crypt(key, strlen(key));
	}
	mlerase();		/* clear it off the bottom line */
	return(status);
}

/* ARGSUSED */
int
ue_setkey(	/* reset encryption key of current buffer */
int f,		/* default flag */
int n)		/* numeric argument */
{
	register int s = ue_makekey(curbp->b_key, NPAT);

	if (s == FALSE) {
		if (curbp->b_key[0] != EOS) {
			s = mlyesno("Discard encryption key");
			if (s == TRUE) {
				curbp->b_key[0] = EOS;
				make_local_b_val(curbp, MDCRYPT);
				set_b_val(curbp, MDCRYPT, FALSE);
				curwp->w_flag |= WFMODE;
			}
		}
	}
	return (s);
}

/**********
 *
 *	ue_crypt - in place encryption/decryption of a buffer
 *
 *	(C) Copyright 1986, Dana L. Hoggatt
 *	1216, Beck Lane, Lafayette, IN
 *
 *	When consulting directly with the author of this routine, 
 *	please refer to this routine as the "DLH-POLY-86-B CIPHER".  
 *
 *	This routine was written for Dan Lawrence, for use in V3.8 of
 *	MicroEMACS, a public domain text/program editor.  
 *
 *	I kept the following goals in mind when preparing this function:
 *
 *	    1.	All printable characters were to be encrypted back
 *		into the printable range, control characters and
 *		high-bit characters were to remain unaffected.  this
 *		way, encrypted would still be just as cheap to
 *		transmit down a 7-bit data path as they were before.
 *
 *	    2.	The encryption had to be portable.  The encrypted
 *		file from one computer should be able to be decrypted
 *		on another computer.
 *
 *	    3.	The encryption had to be inexpensive, both in terms
 *		of speed and space.
 *
 *	    4.	The system needed to be secure against all but the
 *		most determined of attackers.
 *
 *	For encryption of a block of data, one calls crypt passing
 *	a pointer to the data block and its length. The data block is 
 *	encrypted in place, that is, the encrypted output overwrites
 *	the input.  Decryption is totally isomorphic, and is performed
 *	in the same manner by the same routine.  
 *
 *	Before using this routine for encrypting data, you are expected
 *	to specify an encryption key.  This key is an arbitrary string,
 *	to be supplied by the user.  To set the key takes two calls to 
 *	ue_crypt().  First, you call
 *
 *		ue_crypt(NULL, vector)
 *
 *	This resets all internal control information.  Typically (and 
 *	specifically in the case on MICRO-emacs) you would use a "vector" 
 *	of 0.  Other values can be used to customize your editor to be 
 *	"incompatible" with the normally distributed version.  For 
 *	this purpose, the best results will be obtained by avoiding
 *	multiples of 95.
 *
 *	Then, you "encrypt" your password by calling
 *
 *		ue_crypt(pass, strlen(pass))
 *
 *	where "pass" is your password string.  Crypt() will destroy 
 *	the original copy of the password (it becomes encrypted),
 *	which is good.  You do not want someone on a multiuser system 
 *	to peruse your memory space and bump into your password.  
 *	Still, it is a better idea to erase the password buffer to 
 *	defeat memory perusal by a more technical snooper.  
 *
 *	For the interest of cryptologists, at the heart of this
 *	function is a Beaufort Cipher.  The cipher alphabet is the 
 *	range of printable characters (' ' to '~'), all "control" 
 *	and "high-bit" characters are left unaltered.
 *
 *	The key is a variant autokey, derived from a weighted sum 
 *	of all the previous clear text and cipher text.  A counter 
 *	is used as salt to obliterate any simple cyclic behavior 
 *	from the clear text, and key feedback is used to assure 
 *	that the entire message is based on the original key, 
 *	preventing attacks on the last part of the message as if 
 *	it were a pure autokey system.
 *
 *	Overall security of encrypted data depends upon three
 *	factors:  the fundamental cryptographic system must be
 *	difficult to compromise; exhaustive searching of the key 
 *	space must be computationally expensive; keys and plaintext 
 *	must remain out of sight.  This system satisfies this set
 *	of conditions to within the degree desired for MicroEMACS.
 *
 *	Though direct methods of attack (against systems such as 
 *	this) do exist, they are not well known and will consume 
 *	considerable amounts of computing time.  An exhaustive
 *	search requires over a billion investigations, on average.
 *
 *	The choice, entry, storage, manipulation, alteration, 
 *	protection and security of the keys themselves are the 
 *	responsibility of the user.  
 *
 **********/

void
ue_crypt(
register char *bptr,	/* buffer of characters to be encrypted */
register ALLOC_T len)	/* number of characters in the buffer */
{
	register int cc;	/* current character being considered */

	static long key = 0;	/* 29 bit encipherment key */
	static int salt = 0;	/* salt to spice up key with */

	if (!bptr) {		/* is there anything here to encrypt? */
		key = len;	/* set the new key */
		salt = len;	/* set the new salt */
		return;
	}
	while (len--) {		/* for every character in the buffer */

		cc = *bptr;	/* get a character out of the buffer */

		/* only encipher printable characters */
		if ((cc >= ' ') && (cc <= '~')) {

/**  If the upper bit (bit 29) is set, feed it back into the key.  This 
	assures us that the starting key affects the entire message.  **/

			key &= 0x1FFFFFFFL;	/* strip off overflow */
			if (key & 0x10000000L) {
				key ^= 0x0040A001L;	/* feedback */
			}

/**  Down-bias the character, perform a Beaufort encipherment, and 
	up-bias the character again.  We want key to be positive 
	so that the left shift here will be more portable and the 
	mod95() faster   **/

			cc = mod95((int)(key % 95) - (cc - ' ')) + ' ';

/**  the salt will spice up the key a little bit, helping to obscure 
	any patterns in the clear text, particularly when all the 
	characters (or long sequences of them) are the same.  We do 
	not want the salt to go negative, or it will affect the key 
	too radically.  It is always a good idea to chop off cyclics 
	to prime values.  **/

			if (++salt >= 20857) {	/* prime modulus */
				salt = 0;
			}

/**  our autokey (a special case of the running key) is being 
	generated by a weighted checksum of clear text, cipher 
	text, and salt.   **/

			key = key + key + cc + *bptr + salt;
		}
		*bptr++ = cc;	/* put character back into buffer */
	}
	return;
}

static int mod95(register int val)
{
	/*  The mathematical MOD does not match the computer MOD  */

	/*  Yes, what I do here may look strange, but it gets the
		job done, and portably at that.  */

	while (val >= 9500)
		val -= 9500;
	while (val >= 950)
		val -= 950;
	while (val >= 95)
		val -= 95;
	while (val < 0)
		val += 95;
	return (val);
}
#endif
