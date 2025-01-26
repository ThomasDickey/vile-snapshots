/*	Crypt:	Encryption routines for MicroEMACS
 *		written by Dana Hoggatt and Paul Fox.
 *
 * $Id: ecrypt.c,v 1.29 2025/01/26 10:43:31 tom Exp $
 *
 */

#define CAN_TRACE    0
#define CAN_VMS_PATH 0

#include <filters.h>

#undef	OPT_ENCRYPT
#define	OPT_ENCRYPT 1

#include <vl_crypt.h>

#define KEY_LIMIT 256

#define MailMode   options['m']
#define UnixCrypt  options['u']

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
# include <string.h>
  /* An ANSI string.h and pre-ANSI memory.h might conflict.  */
# if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#  include <memory.h>
# endif	/* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HEADERS and not HAVE_STRING_H */
# if defined(HAVE_STRINGS_H)
#  include <strings.h>
  /* memory.h and strings.h conflict on some systems */
  /* FIXME: should probably define memcpy and company in terms of bcopy,
     et al here */
# endif
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

static ULONG ue_key = 0;	/* 29 bit encipherment key */
static int ue_salt = 0;		/* salt to spice up key with */

static void
failed(const char *s)
{
    perror(s);
    exit(BADEXIT);
}

static int
mod95(register int val)
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

static void
ue_crypt(register char *bptr,	/* buffer of characters to be encrypted */
	 register UINT len)	/* number of characters in the buffer */
{
    register int cc;		/* current character being considered */

    if (!bptr) {		/* is there anything here to encrypt? */
	ue_key = len;		/* set the new key */
	ue_salt = (int) len;	/* set the new salt */
	return;
    }
    while (len--) {		/* for every character in the buffer */

	cc = *bptr;		/* get a character out of the buffer */

	/* only encipher printable characters */
	if ((cc >= ' ') && (cc <= '~')) {

/**  If the upper bit (bit 29) is set, feed it back into the key.  This
	assures us that the starting key affects the entire message.  **/

	    ue_key &= 0x1FFFFFFFL;	/* strip off overflow */
	    if (ue_key & 0x10000000L) {
		ue_key ^= 0x0040A001L;	/* feedback */
	    }

/**  Down-bias the character, perform a Beaufort encipherment, and
	up-bias the character again.  We want key to be positive
	so that the left shift here will be more portable and the
	mod95() faster   **/

	    cc = mod95((int) (ue_key % 95) - (cc - ' ')) + ' ';

/**  the salt will spice up the key a little bit, helping to obscure
	any patterns in the clear text, particularly when all the
	characters (or long sequences of them) are the same.  We do
	not want the salt to go negative, or it will affect the key
	too radically.  It is always a good idea to chop off cyclics
	to prime values.  **/

	    if (++ue_salt >= 20857) {	/* prime modulus */
		ue_salt = 0;
	    }

/**  our autokey (a special case of the running key) is being
	generated by a weighted checksum of clear text, cipher
	text, and salt.   **/

	    ue_key = (ue_key
		      + ue_key
		      + (ULONG) cc
		      + (ULONG) (*bptr)
		      + (ULONG) ue_salt);
	}
	*bptr++ = (char) cc;	/* put character back into buffer */
    }
    return;
}

static void
filecrypt(FILE *ifp, char *key, char *options, const char *seedValue)
{
    char buf[BUFSIZ];
    char real_key[KEY_LIMIT];
    char temp_key[KEY_LIMIT];
    char *temp = NULL;
    int ch = EOF, c0;

    if (UnixCrypt) {
	int seed = (int) strtol(seedValue, &temp, 0);
	size_t len = KEY_LIMIT - 4;

	strncpy(temp_key, key, len)[len] = '\0';
	strncpy(real_key, key, len)[len] = '\0';
	vl_make_encrypt_key(real_key, temp_key);
	vl_setup_encrypt(real_key, seed);

    } else {

	ue_key = 0;
	ue_salt = 0;
	ue_crypt((char *) 0, 0);
	ue_crypt(key, (unsigned) strlen(key));

    }

    while (1) {
	c0 = ch;
	ch = vl_getc(ifp);
	if (ch == EOF && feof(ifp))
	    break;
	if (ferror(ifp))
	    failed("reading file");
	buf[0] = (char) ch;
	buf[1] = 0;

	if (MailMode) {
	    if (ch == '\n' && c0 == '\n')
		MailMode = 0;
	}
	if (!MailMode) {
	    if (UnixCrypt) {
		vl_encrypt_blok(buf, 1);
	    } else {
		ue_crypt(buf, 1);
	    }
	}
	if (vl_putc(buf[0], stdout) == EOF)
	    failed("writing stdout");
    }

    if (fflush(stdout))
	failed("flushing stdout");
}

static void
usage(const char *prog)
{
    static const char *table[] =
    {
	"usage: %s [options] [file ...]",
	"",
	"Encrypt one or more files, writing result to standard output.",
	"If no file is specified, read from standard input.",
	"",
	"Options:",
	"  -k KEY specify crypt-key",
#ifdef HAVE_GETPASS
	"         If option is missing, %s will prompt for it.",
#endif
	"  -m     forces \"mail-mode\", which leaves text before",
	"         the first empty line of a file (i.e. headers) intact.",
	"  -s NUM seed value for Unix-style encryption (default matches vile)",
	"  -u     use Unix-style encryption (default is UEmacs)"
    };
    size_t n;
    for (n = 0; n < TABLESIZE(table); ++n) {
	fprintf(stderr, table[n], prog);
	fputc('\n', stderr);
    }
    exit(BADEXIT);
}

static void
do_stream(const char *prog, FILE *fp, char *key, char *options, const char *seed)
{
    if (!key[0]) {
#ifdef HAVE_GETPASS
	char *userkey = (char *) getpass("Enter key: ");
	size_t len = strlen(userkey);

	/* HACK -- the linux version of getpass is not interruptible.  this
	 * means there's no way to abort a botched passwd.  until this problem
	 * goes away (and i do believe it's a bug, and i'm not planning on
	 * providing our own getpass()) we'll just see if the last char of the
	 * entered key is ^C, and consider it an abort if so.  yes, this should
	 * really be the INTR char -pgf
	 */
	if (len == 0 || userkey[len - 1] == 3) {
	    fprintf(stderr, "%s: Aborted\n", prog);
	    exit(BADEXIT);
	}
	(void) strncpy(key, userkey, (size_t) KEY_LIMIT);
	(void) memset(userkey, '.', strlen(userkey));
#else
	usage(prog);
#endif
    }
    filecrypt(fp, key, options, seed);
}

static void
do_filename(const char *prog, const char *name, char *key, char *options, const char *seed)
{
    FILE *fp = fopen(name, "r");
    if (fp == NULL)
	failed(name);
    do_stream(prog, fp, key, options, seed);
    (void) fclose(fp);
}

int
main(int argc, char **argv)
{
    char empty[1];
    char key[KEY_LIMIT];
    char *prog = argv[0];
    char options[256];
    const char *seed = "123";
    int had_file = 0;
    int n;

    empty[0] = 0;
    memset(key, 0, sizeof(key));
    memset(options, 0, sizeof(options));

    for (n = 1; n < argc; ++n) {
	if (*argv[n] == '-') {
	    char *param = argv[n] + 1;
	    while (*param != 0) {
		switch (*param++) {
		case 'k':
		    if (*param == 0)
			param = argv[++n];
		    if (param == NULL) {
			usage(prog);
		    } else if (strlen(param) > KEY_LIMIT - 1) {
			fprintf(stderr, "%s: excessive key length\n", prog);
			exit(BADEXIT);
		    } else {
			(void) strncpy(key, param, (size_t) KEY_LIMIT);
			(void) memset(param, '.', strlen(param));
			key[KEY_LIMIT - 1] = '\0';
			param = empty;
		    }
		    break;
		case 'm':
		    /* -m for mailmode:  leaves headers intact (up to first
		     * blank line) then crypts the rest.  i use this for
		     * encrypting mail messages in mh folders -pgf */
		    MailMode = 1;
		    break;
		case 's':
		    if (*param == 0)
			param = argv[++n];
		    if (param == NULL)
			usage(prog);
		    seed = param;
		    param = empty;
		    break;
		case 'u':
		    UnixCrypt = 1;
		    break;
		default:
		    fprintf(stderr, "unknown switch -%c\n", param[-1]);
		    usage(prog);
		    break;
		}
	    }
	} else {
	    had_file = 1;
	    do_filename(prog, argv[n], key, options, seed);
	}
    }

    if (!had_file) {
	do_stream(prog, stdin, key, options, seed);
    }
    fflush(stdout);
    fclose(stdout);
    exit(GOODEXIT);
}
