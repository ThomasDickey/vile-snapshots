/* This standalone utility program constructs the function, key and command
 *	binding tables for vile.  The input is a data file containing the
 *	desired default relationships among the three entities.  Output
 *	is nebind.h, neproto.h, nefunc.h, and nename.h, all of which are then
 *	included in main.c
 *
 *	Copyright (c) 1990 by Paul Fox
 *	Copyright (c) 1990, 1995 by Paul Fox and Tom Dickey
 *
 *	See the file "cmdtbls" for input data formats, and "estruct.h" for
 *	the output structures.
 *
 * Heavily modified/enhanced to also generate the table of mode and variable
 * names and their #define "bindings", based on input from the file modetbl,
 * by Tom Dickey, 1993.    -pgf
 *
 *
 * $Header: /users/source/archives/vile.vcs/RCS/mktbls.c,v 1.77 1997/02/09 19:30:39 tom Exp $
 *
 */

#define	OPT_IFDEF_MODES	1	/* true iff we can ifdef modes */

/* stuff borrowed/adapted from estruct.h */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else	/* !defined(HAVE_CONFIG_H) */

#ifndef SYS_VMS
#define SYS_VMS 0
#endif

/* Note: VAX-C doesn't recognize continuation-line in ifdef lines */
# ifdef vms
#  define HAVE_STDLIB_H 1
# endif

	/* pc-stuff */
# if defined(__TURBOC__) || defined(__WATCOMC__) || defined(__GO32__) || defined(__IBMC__)
#  define HAVE_STDLIB_H 1
# endif

	/* unix-stuff */
# if (defined(__GNUC__) && (defined(apollo) || defined(sun) || defined(__hpux) || defined(linux)))
#  define HAVE_STDLIB_H 1
# endif
#endif	/* !defined(HAVE_CONFIG_H) */

#ifndef HAVE_STDLIB_H
# define HAVE_STDLIB_H 0
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
# if !defined(HAVE_CONFIG_H) || MISSING_EXTERN_MALLOC
extern	char *	malloc	( unsigned int len );
# endif
# if !defined(HAVE_CONFIG_H) || MISSING_EXTERN_FREE
extern	void	free	( char *ptr );
# endif
#endif

/*----------------------------------------------------------------------------*/
#ifdef	main	/* we're trying to intercept it, e.g., for Windows wrapper */
#define	ReturnFromMain return
#endif

#ifndef	ReturnFromMain
#define ReturnFromMain exit
#endif

/*----------------------------------------------------------------------------*/
#ifndef DOALLOC
#define DOALLOC 0
#endif

#if DOALLOC
#include "trace.h"
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

/* argument for 'exit()' or '_exit()' */
#if	SYS_VMS
#include	<stsdef.h>
#define GOODEXIT	(STS$M_INHIB_MSG | STS$K_SUCCESS)
#define BADEXIT		(STS$M_INHIB_MSG | STS$K_ERROR)
#else
#define GOODEXIT	0
#define BADEXIT		1
#endif

#define	TABLESIZE(v)	(sizeof(v)/sizeof(v[0]))

/*--------------------------------------------------------------------------*/

#define MAX_BIND        4	/* maximum # of key-binding types */
#define	MAX_PARSE	5	/* maximum # of tokens on line */
#define	LEN_BUFFER	50	/* nominal buffer-length */
#define	MAX_BUFFER	(LEN_BUFFER*10)
#define	LEN_CHRSET	256	/* total # of chars in set (ascii) */

	/* patch: why not use <ctype.h> ? */
#define	DIFCNTRL	0x40
#define tocntrl(c)	((c)^DIFCNTRL)
#define toalpha(c)	((c)^DIFCNTRL)
#define	DIFCASE		0x20
#define	isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define	islower(c)	((c) >= 'a' && (c) <= 'z')
#define toupper(c)	((c)^DIFCASE)
#define tolower(c)	((c)^DIFCASE)

#ifndef	TRUE
#define	TRUE	(1)
#define	FALSE	(0)
#endif

#define EOS     '\0'

#define	L_CURL	'{'
#define	R_CURL	'}'

#define	Fprintf	(void)fprintf
#define	Sprintf	(void)sprintf

#if MISSING_EXTERN_FPRINTF
extern	int	fprintf	( FILE *fp, const char *fmt, ... );
#endif

#define	SaveEndif(head)	InsertOnEnd(&head, "#endif")

#define	FreeIfNeeded(p) if (p != 0) { free(p); p = 0; }

/*--------------------------------------------------------------------------*/

typedef	struct stringl {
	char *Name;	/* stores primary-data */
	char *Func;	/* stores secondary-data */
	char *Data;	/* associated data, if any */
	char *Cond;	/* stores ifdef-flags */
	char *Note;	/* stores comment, if any */
	struct stringl *nst;
} LIST;

static	char	*Blank = "";

static	LIST	*all_names,
		*all_kbind,	/* data for kbindtbl[] */
		*all_funcs,	/* data for extern-lines in neproto.h */
		*all__FUNCs,	/* data for {}-lines in nefunc.h */
		*all__CMDFs,	/* data for extern-lines in nefunc.h */
		*all_envars,
		*all_ufuncs,
		*all_modes,	/* data for name-completion of modes */
		*all_gmodes,	/* data for GLOBAL modes */
		*all_bmodes,	/* data for BUFFER modes */
		*all_wmodes;	/* data for WINDOW modes */

	int	main ( int argc, char **argv );

static	int	isspace (int c);
static	int	isprint (int c);

static	char *	Alloc (unsigned len);
static	char *	StrAlloc (const char *s);
static	LIST *	ListAlloc (void);

static	void	badfmt (const char *s);
static	void	badfmt2 (const char *s, int col);

static	void	WriteLines (FILE *fp, const char *const *list, int count);
static	FILE *	OpenHeader (const char *name, char **argv);

static	void	InsertSorted (LIST **headp, const char *name, const char *func, const char *data, const char *cond, const char *note);
static	void	InsertOnEnd (LIST **headp, const char *name);

static	char *	append (char *dst, const char *src);
static	char *	formcond (const char *c1, const char *c2);
static	int	LastCol (char *buffer);
static	char *	PadTo (int col, char *buffer);
static	int	two_conds (int c, char *cond);
static	void	set_binding (int btype, int c, char *cond, char *func);
static	int	Parse (char *input, char **vec);

static	void	BeginIf (void);
static	void	WriteIf (FILE *fp, const char *cond);
static	void	FlushIf (FILE *fp);

static	char *	AbbrevMode (char *src);
static	char *	NormalMode (char *src);
static	const char * c2TYPE (int c);
static	void	CheckModes ( char *name );
static	char *	Mode2Key (char *type, char *name, char *cond);
static	char *	Name2Symbol (char *name);
static	char *	Name2Address (char *name, char *type);

static	void	DefineOffset (FILE *fp);
static	void	WriteIndexStruct (FILE *fp, LIST *p, const char *ppref);
static	void	WriteModeDefines (LIST *p, const char *ppref);
static	void	WriteModeSymbols (LIST *p);

static	void	save_all_modes (const char *type, char *normal, const char *abbrev, char *cond);
static	void	dump_all_modes (void);

static	void	save_bindings (char *s, char *func, char *cond);
static	void	dump_bindings (void);

static	void	save_bmodes (char *type, char **vec);
static	void	dump_bmodes (void);

static	void	start_evar_h (char **argv);
static	void	finish_evar_h (void);
static	void	init_envars (void);
static	void	save_envars (char **vec);
static	void	dump_envars (void);

static	void	save_funcs (char *func, char *flags, char *cond, char *old_cond, char *help);
static	void	dump_funcs (FILE *fp, LIST *head);

static	void	save_gmodes (char *type, char **vec);
static	void	dump_gmodes (void);

static	void	save_names (char *name, char *func, char *cond);
static	void	dump_names (void);

static	void	init_ufuncs (void);
static	void	save_ufuncs (char **vec);
static	void	dump_ufuncs (void);

static	void	save_wmodes (char *type, char **vec);
static	void	dump_wmodes (void);

	/* definitions for sections of cmdtbl */
#define	SECT_CMDS 0
#define	SECT_FUNC 1
#define	SECT_VARS 2
#define	SECT_GBLS 3
#define	SECT_BUFF 4
#define	SECT_WIND 5

	/* definitions for indices to 'asciitbl[]' vs 'kbindtbl[]' */
#define ASCIIBIND 0
#define CTLXBIND 1
#define CTLABIND 2
#define SPECBIND 3

static	char *bindings  [LEN_CHRSET];
static	char *conditions[LEN_CHRSET];
static	const char *tblname   [MAX_BIND] = {"asciitbl", "ctlxtbl", "metatbl", "spectbl" };
static	const char *prefname  [MAX_BIND] = {"",         "CTLX|",   "CTLA|",   "SPEC|" };

static	char *inputfile;
static	int l = 0;
static	FILE *cmdtbl;
static	FILE *nebind, *neprot, *nefunc, *nename;
static	FILE *nevars, *nemode, *nefkeys;
static	jmp_buf my_top;

/******************************************************************************/
static int
isspace(int c)
{
	return c == ' ' || c == '\t' || c == '\n';
}

static int
isprint(int c)
{
	return c >= ' ' && c < 0x7f;
}

/******************************************************************************/
static char *
Alloc(unsigned len)
{
	char	*pointer = malloc(len);
	if (pointer == 0)
		badfmt("bug: not enough memory");
        return pointer;
}

static char *
StrAlloc(const char *s)
{
	return strcpy(Alloc((unsigned)strlen(s)+1), s);
}

static LIST *
ListAlloc(void)
{
	return (LIST *)Alloc(sizeof(LIST));
}

/******************************************************************************/
static void
badfmt(const char *s)
{
	Fprintf(stderr,"\"%s\", line %d: bad format:", inputfile, l);
	Fprintf(stderr,"\t%s\n",s);
	longjmp(my_top,1);
}

static void
badfmt2(const char *s, int col)
{
	char	temp[MAX_BUFFER];
	Sprintf(temp, "%s (column %d)", s, col);
	badfmt(temp);
}

/******************************************************************************/
static void
WriteLines(
FILE	*fp,
const char *const *list,
int	count)
{
	while (count-- > 0)
		Fprintf(fp, "%s\n", *list++);
}
#define	write_lines(fp,list) WriteLines(fp, list, (int)TABLESIZE(list))

/******************************************************************************/
static FILE *
OpenHeader(
const char *name,
char	**argv)
{
	register FILE *fp;
	static const char *progcreat =
"/* %s: this header file was produced automatically by\n\
 * the %s program, based on input from the file %s\n */\n";

	if ((fp = fopen(name, "w")) == 0) {
		Fprintf(stderr,"mktbls: couldn't open header file %s\n", name);
		longjmp(my_top,1);
	}
	Fprintf(fp, progcreat, name, argv[0], argv[1]);
	return fp;
}

/******************************************************************************/
static void
InsertSorted(
LIST	**headp,
const char *name,
const char *func,
const char *data,
const char *cond,
const char *note)
{
	register LIST *n, *p, *q;
	register int  r;

	n = ListAlloc();
	n->Name = StrAlloc(name);
	n->Func = StrAlloc(func);
	n->Data = StrAlloc(data);
	n->Cond = StrAlloc(cond);
	n->Note = StrAlloc(note);

	for (p = *headp, q = 0; p != 0; q = p, p = p->nst) {
		if ((r = strcmp(n->Name, p->Name)) < 0)
			break;
		else if (r == 0 && !strcmp(n->Cond, p->Cond))
			badfmt("duplicate name");
	}
	n->nst = p;
	if (q == 0)
		*headp = n;
	else
		q->nst = n;
}

static void
InsertOnEnd(
LIST	**headp,
const char *name)
{
	register LIST *n, *p, *q;

	n = ListAlloc();
	n->Name = StrAlloc(name);
	n->Func = Blank;
	n->Data = Blank;
	n->Cond = Blank;
	n->Note = Blank;

	for (p = *headp, q = 0; p != 0; q = p, p = p->nst)
		;

	n->nst = 0;
	if (q == 0)
		*headp = n;
	else
		q->nst = n;
}

/******************************************************************************/
static char *
append(
char	*dst,
const char *src)
{
	(void)strcat(dst, src);
	return (dst + strlen(dst));
}

static char *
formcond(const char *c1, const char *c2)
{
	static char cond[MAX_BUFFER];
	if (c1[0] && c2[0])
		Sprintf(cond, "(%s) & (%s)", c1, c2);
	else if (c1[0] || c2[0])
		Sprintf(cond, "(%s%s)", c1, c2);
	else
		cond[0] = EOS;
	return cond;
}

static int
LastCol(char *buffer)
{
	register int	col = 0,
			c;
	while ((c = *buffer++) != 0) {
		if (isprint(c))
			col++;
		else if (c == '\t')
			col = (col | 7) + 1;
	}
	return col;
}

static char *
PadTo(int col, char *buffer)
{
	int	any	= 0,
		len	= strlen(buffer),
		now;
	char	with;

	for (;;) {
		if ((now = LastCol(buffer)) >= col) {
			if (any)
				break;
			else
				with = ' ';
		} else if (col-now > 1)
			with = '\t';
		else
			with = ' ';

		buffer[len++] = with;
		buffer[len]   = EOS;
		any++;
	}
	return buffer;
}

static int
two_conds(int c, char *cond)
{
	/* return true if both bindings have different
	 conditions associated with them */
	return (cond[0] != '\0' &&
		conditions[c] != NULL &&
		strcmp(cond, conditions[c]) != '\0');
}

static void
set_binding (
int	btype,
int	c,
char *	cond,
char *	func)
{
	char	name[MAX_BUFFER];

	if (btype != ASCIIBIND) {
		if (c < ' ') {
			Sprintf(name, "%stocntrl('%c')",
				prefname[btype],
				toalpha(c));
		} else if (c >= 0x80) {
			Sprintf(name, "%s0x%x",
				prefname[btype], c);
		} else {
			Sprintf(name, "%s'%s%c'",
				prefname[btype],
				(c == '\'' || c == '\\') ? "\\" : "",
				c);
		}
		InsertSorted(&all_kbind, name, func, "", cond, "");
	} else {
		if (bindings[c] != NULL) {
			if (!two_conds(c,cond))
				badfmt("duplicate key binding");
			free(bindings[c]);
		}
		bindings[c] = StrAlloc(func);
		if (cond[0]) {
			FreeIfNeeded(conditions[c]);
			conditions[c] = StrAlloc(cond);
		} else {
			conditions[c] = NULL;
		}
	}
}

/******************************************************************************/
	/* returns the number of non-comment tokens parsed, with a list of
	 * tokens (0=comment) as a side-effect.  Note that quotes are removed
	 * from the token, so we have to have them only in the first token! */
static int
Parse(
char	*input,
char	**vec)
{
	register int	expecting = TRUE,
			count = 0,
			quote = 0,
			n,
			c;

	for (c = 0; c < MAX_PARSE; c++)
		vec[c] = "";
	for (c = strlen(input); c > 0 && isspace(input[c-1]); c--)
		input[c-1] = EOS;

	for (n = 0; (c = input[n++]) != EOS; ) {
		if (quote) {
			if (c == quote) {
				quote = 0;
				if (input[n] && !isspace(input[n]))
					badfmt2("expected blank", n);
				input[n-1] = EOS;
			}
		} else {
			if ((c == '"') || (c == '\'')) {
				quote = c;
			} else if (c == '<') {
				c = quote = '>';
			} else if (isspace(c)) {
				input[n-1] = EOS;
				expecting = TRUE;
			} else if (c == '#') {
				while (isspace(input[n]))
					n++;
				vec[0] = input+n;
				break;
			}
			if (expecting && !isspace(c)) {
				if (count+1 >= MAX_PARSE)
					break;
				vec[++count] = input + n - ((c != quote)?1:0);
				expecting = FALSE;
			}
		}
	}
	return count;
}

/******************************************************************************/
static const char *lastIfdef;

static void
BeginIf(void)
{
	lastIfdef = 0;
}

static void
WriteIf(
FILE	*fp,
const char *cond)
{
	if (cond == 0)
		cond = "";
	if (cond[0] != EOS) {
		if (lastIfdef != 0) {
			if (!strcmp(lastIfdef, cond))
				return;
			FlushIf(fp);
		}
		Fprintf(fp, "#if %s\n", lastIfdef = cond);
	} else
		FlushIf(fp);
}

static void
FlushIf(FILE *fp)
{
	if (lastIfdef != 0) {
		Fprintf(fp, "#endif\n");
		lastIfdef = 0;
	}
}

/******************************************************************************/
/* get abbreviation by taking the uppercase chars only */
static char *
AbbrevMode(char *src)
{
	char *dst = StrAlloc(src);
	register char	*s = src,
			*d = dst;
	while (*s) {
		if (isupper(*s))
			*d++ = (char)tolower(*s);
		s++;
	}
	*d = EOS;
	return dst;
}

/* get name, converted to lowercase */
static char *
NormalMode(char *src)
{
	char *dst = StrAlloc(src);
	register char *s = dst;
	while (*s) {
		if (isupper(*s))
			*s = (char)tolower(*s);
		s++;
	}
	return dst;
}

/* given single-char type-key (cf: Mode2Key), return define-string */
static const char *
c2TYPE(int c)
{
	const char *value;
	switch (c) {
	case 'b':	value	= "BOOL";	break;
	case 'e':	value	= "ENUM";	break;
	case 'i':	value	= "INT";	break;
	case 's':	value	= "STRING";	break;
	case 'x':	value	= "REGEX";	break;
	default:	value	= "?";
	}
	return value;
}

/* check that the mode-name won't be illegal */
static void
CheckModes(char *name)
{
	if (!strncmp(name, "no", 2))
		badfmt("illegal mode-name");
}

/* make a sort-key for mode-name */
static char *
Mode2Key(char *type, char *name, char *cond)
{
	int	c;
	char	*abbrev = AbbrevMode(name),
		*normal = NormalMode(name),
		*tmp = Alloc((unsigned)(4 + strlen(normal) + strlen(abbrev)));

	CheckModes(normal);
	CheckModes(abbrev);

	switch (c = *type) {
	case 'b':
	case 'e':
	case 'i':
	case 's':	break;
	case 'r':	c = 'x';	/* make this sort after strings */
	}

	save_all_modes(type, normal, abbrev, cond);

	(void)sprintf(tmp, "%s\n%c\n%s", normal, c, abbrev);
#if NO_LEAKS
	free(normal);
	free(abbrev);
#endif
	return tmp;
}

/* converts a mode-name to a legal (hopefully unique!) symbol */
static char *
Name2Symbol(char *name)
{
	char	*base, *dst;
	register char c;

	/* allocate enough for adjustment in 'Name2Address()' */
	/*   "+ 10" for comfort */
	base = dst = Alloc((unsigned)(strlen(name) + 10));

	*dst++ = 's';
	*dst++ = '_';
	while ((c = *name++) != EOS) {
		if (c == '-')
			c = '_';
		*dst++ = c;
	}
	*dst++ = '_';
	*dst++ = '_';
	*dst = EOS;
	return base;
}

/* converts a mode-name & type to a reference to string-value */
static char *
Name2Address(
char	*name,
char	*type)
{
	/*  "+ 10" for comfort */
        unsigned len = strlen(name) + 10;
	char	*base = Alloc(len);
	char	*temp;

	temp = Name2Symbol(name);
	if (strlen(temp) + 1 + ((*type == 'b') ? 4 : 0) > len)
        	badfmt("bug: buffer overflow in Name2Address");

	(void)strcpy(base, temp);
	if (*type == 'b')
		(void)strcat(strcat(strcpy(base+2, "no"), temp+2), "+2");
	free(temp);
	return base;
}

/* define Member_Offset macro, used in index-definitions */
static void
DefineOffset(FILE *fp)
{
#if	OPT_IFDEF_MODES
	Fprintf(fp,
"#ifndef\tMember_Offset\n\
#define\tMember_Offset(T, M)\t(((int)&(((T*)0)->M))/\\\n\
\t\t\t\t ((int)&(((T*)0)->Q1) - (int)&(((T*)0)->s_MAX)))\n\
#endif\n");
#endif
}

/* generate the index-struct (used for deriving ifdef-able index definitions) */
static void
WriteIndexStruct(
FILE	*fp,
LIST	*p,
const char *ppref)
{
#if	OPT_IFDEF_MODES
	char	*s,
		temp[MAX_BUFFER],
		line[MAX_BUFFER],
		*vec[MAX_PARSE];

	BeginIf();
	Fprintf(fp, "typedef\tstruct\t%c\n", L_CURL);
	while (p != 0) {
		WriteIf(fp, p->Cond);
		(void)Parse(strcpy(line, p->Name), vec);
		Sprintf(temp, "\tchar\t%s;", s = Name2Symbol(vec[1]));
		free(s);
		if (p->Note[0]) {
			(void)PadTo(32, temp);
			Sprintf(temp+strlen(temp), "/* %s */", p->Note);
		}
		Fprintf(fp, "%s\n", temp);
		p = p->nst;
	}

	FlushIf(fp);
	Fprintf(fp, "\tchar\ts_MAX;\n");
	Fprintf(fp, "\tchar\tQ1;\n");
	Fprintf(fp, "\t%c Index%s;\n\n", R_CURL, ppref);
#endif	/* OPT_IFDEF_MODES */
}

/* generate the index-definitions */
static void
WriteModeDefines(
LIST	*p,
const char *ppref)
{
	char	temp[MAX_BUFFER],
		line[MAX_BUFFER],
		*vec[MAX_PARSE];
	int	count	= 0;
#if OPT_IFDEF_MODES
	char	*s;
	BeginIf();
#endif

	for (; p != 0; p = p->nst, count++) {
		(void)Parse(strcpy(line, p->Name), vec);
		Sprintf(temp, "#define %.1s%s%s ",
			(*ppref == 'B') ? "" : ppref,
			(*vec[2] == 'b') ? "MD" : "VAL_",
			p->Func);
		(void)PadTo(24, temp);
#if OPT_IFDEF_MODES
		WriteIf(nemode, p->Cond);
		Sprintf(temp+strlen(temp), "Member_Offset(Index%s, %s)",
			ppref, s = Name2Symbol(vec[1]));
		free(s);
#else
		Sprintf(temp+strlen(temp), "%d", count);
		if (p->Note[0]) {
			(void)PadTo(32, temp);
			Sprintf(temp+strlen(temp), "/* %s */", p->Note);
		}
#endif /* OPT_IFDEF_MODES */
		Fprintf(nemode, "%s\n", temp);
	}

	Fprintf(nemode, "\n");
#if OPT_IFDEF_MODES
	FlushIf(nemode);
	Sprintf(temp, "#define NUM_%c_VALUES\tMember_Offset(Index%s, s_MAX)",
		*ppref, ppref);
#else
	Sprintf(temp, "#define NUM_%c_VALUES\t%d", *ppref, count);
#endif /* OPT_IFDEF_MODES */

	(void)PadTo(32, temp);
	Sprintf(temp+strlen(temp), "/* TABLESIZE(%c_valuenames) -- %s */\n",
		tolower(*ppref), ppref);
	Fprintf(nemode, "%s", temp);
	Fprintf(nemode, "#define MAX_%c_VALUES\t%d\n\n", *ppref, count);
}

static void
WriteModeSymbols(LIST *p)
{
	char	temp[MAX_BUFFER],
		line[MAX_BUFFER];
	char	*vec[MAX_PARSE],
		*s;

	/* generate the symbol-table */
	BeginIf();
	while (p != 0) {
#if OPT_IFDEF_MODES
		WriteIf(nemode, p->Cond);
#endif
		(void)Parse(strcpy(line, p->Name), vec);
		Sprintf(temp, "\t%c %s,",
			L_CURL, s = Name2Address(vec[1], vec[2]));
		(void)PadTo(32, temp);
		free(s);
		s = 0;

		Sprintf(temp+strlen(temp), "%s,",
			*vec[3] ? (s = Name2Address(vec[3], vec[2])) : "\"X\"");
		(void)PadTo(48, temp);
		if (s != 0)
			free(s);

		Sprintf(temp+strlen(temp), "VALTYPE_%s,", c2TYPE(*vec[2]));
		(void)PadTo(64, temp);
		if (!strcmp(p->Data, "0"))
			(void)strcat(temp, "(ChgdFunc)0 },");
		else
			Sprintf(temp+strlen(temp), "%s },", p->Data);
		Fprintf(nemode, "%s\n", temp);
		p = p->nst;
	}
	FlushIf(nemode);

}

/******************************************************************************/
static void
save_all_modes(
const char *type,
char	*normal,
const char *abbrev,
char	*cond)
{
	if (*type == 'b') {
		char	t_normal[LEN_BUFFER],
			t_abbrev[LEN_BUFFER];
		save_all_modes("Bool",
			strcat(strcpy(t_normal, "no"), normal),
			*abbrev
				? strcat(strcpy(t_abbrev, "no"), abbrev)
				: "",
			cond);
	}
	InsertSorted(&all_modes, normal, type, "", cond, "");
	if (*abbrev)
		InsertSorted(&all_modes, abbrev, type, "", cond, "");
}

static void
dump_all_modes(void)
{
	static const char *const top[] = {
		"",
		"#ifdef realdef",
		"/*",
		" * List of strings shared between all_modes, b_valnames and w_valnames",
		" */",
		"static const char",
		};
	static const char *const middle[] = {
		"\ts_NULL[] = \"\";",
		"#endif /* realdef */",
		"",
		"#ifdef realdef",
		"const char *const all_modes[] = {",
		};
	static const char *const bottom[] = {
		"\tNULL\t/* ends table */",
		"};",
		"#else",
		"extern const char *const all_modes[];",
		"#endif /* realdef */",
		};
	char	temp[MAX_BUFFER], *s;
	register LIST *p, *q;

	InsertSorted(&all_modes, "all", "?", "", "", "");
	write_lines(nemode, top);
	BeginIf();
	for (p = all_modes; p; p = p->nst) {
		if (p->Func[0] != 'b') {
			for (q = p->nst; q != 0; q = q->nst)
				if (q->Func[0] != 'b')
					break;
#if OPT_IFDEF_MODES
			WriteIf(nemode, p->Cond);
#endif
			Sprintf(temp, "\t%s[]",	s = Name2Symbol(p->Name));
			(void)PadTo(32, temp);
			free(s);
			Sprintf(temp+strlen(temp), "= \"%s\",", p->Name);
			(void)PadTo(64, temp);
			Fprintf(nemode, "%s/* %s */\n", temp, p->Func);
		}
	}
	FlushIf(nemode);

	write_lines(nemode, middle);
	for (p = all_modes; p; p = p->nst) {
#if OPT_IFDEF_MODES
		WriteIf(nemode, p->Cond);
#endif
		Fprintf(nemode, "\t%s,\n", s = Name2Address(p->Name, p->Func));
		free(s);
	}
	FlushIf(nemode);

	write_lines(nemode, bottom);
}

/******************************************************************************/
static void
save_bindings(char *s, char *func, char *cond)
{
	int btype, c, highbit;

	btype = ASCIIBIND;

	if (*s == '^' && *(s+1) == 'A'&& *(s+2) == '-') {
		btype = CTLABIND;
		s += 3;
	} else if (*s == 'F' && *(s+1) == 'N' && *(s+2) == '-') {
		btype = SPECBIND;
		s += 3;
	} else if (*s == '^' && *(s+1) == 'X'&& *(s+2) == '-') {
		btype = CTLXBIND;
		s += 3;
	}
	if (*s == 'M' && *(s+1) == '-') {
		highbit = 0x80;
		s += 2;
	} else {
		highbit = 0;
	}

	if (*s == '\\') { /* try for an octal value */
		c = 0;
		while (*++s < '8' && *s >= '0')
			c = (c*8) + *s - '0';
		if (c >= LEN_CHRSET)
			badfmt("octal character too big");
		c |= highbit;
		set_binding(btype, c, cond, func);
	} else if (*s == '^' && (c = *(s+1)) != EOS) { /* a control char? */
		if (c > 'a' &&  c < 'z')
			c = toupper(c);
		c = tocntrl(c);
		c |= highbit;
		set_binding(btype, c, cond, func);
		s += 2;
	} else if ((c = *s) != 0) {
		c |= highbit;
		set_binding(btype, c, cond, func);
		s++;
	} else {
		badfmt("getting binding");
	}

	if (*s != EOS)
		badfmt("got extra characters");
}

static void
dump_bindings(void)
{
	char	temp[MAX_BUFFER];
	const char *sctl, *meta;
	int i, c, btype;
	register LIST *p;

	btype = ASCIIBIND;

	Fprintf(nebind,"\nconst CMDFUNC *%s[%d] = %c\n",
		tblname[btype], LEN_CHRSET, L_CURL);

	BeginIf();
	for (i = 0; i < LEN_CHRSET; i++) {
		WriteIf(nebind, conditions[i]);

		sctl = "";
		if (i & 0x80)
		    meta = "meta-";
		else
		    meta = "";
		c = i & 0x7f;
		if (c < ' ' || c > '~') {
			sctl = "ctrl-";
			c = toalpha(c);
		}

		if (bindings[i])
			Sprintf(temp, "\t&f_%s,", bindings[i]);
		else
			Sprintf(temp, "\tNULL,");

		Fprintf(nebind, "%s/* %s%s%c */\n", PadTo(32, temp),
							meta, sctl, c);
		if (conditions[i] != 0)
			Fprintf(nebind,"#else\n\tNULL,\n");
		FlushIf(nebind);

	}
	Fprintf(nebind, "%c;\n", R_CURL);

	Fprintf(nebind,"\nKBIND kbindtbl[] = %c\n", L_CURL);
	BeginIf();
	for (p = all_kbind; p; p = p->nst) {
		WriteIf(nebind, p->Cond);
		Sprintf(temp, "\t%c %s,", L_CURL, p->Name);
		Fprintf(nebind, "%s&f_%s %c,\n",
			PadTo(32, temp), p->Func, R_CURL);
	}
	FlushIf(nebind);

	Fprintf(nebind,"\t{ 0, NULL }\n");
	Fprintf(nebind,"%c;\n", R_CURL);
}

/******************************************************************************/
static void
save_bmodes(
char	*type,
char	**vec)
{
	char *key = Mode2Key(type, vec[1], vec[4]);
	InsertSorted(&all_bmodes, key, vec[2], vec[3], vec[4], vec[0]);
#if NO_LEAKS
	free(key);
#endif
}

static void
dump_bmodes(void)
{
	static const char *const top[] = {
		"",
		"/* buffer mode flags\t*/",
		"/* the indices of B_VALUES.v[] */",
		};
	static const char *const middle[] = {
		"",
		"typedef struct B_VALUES {",
		"\t/* each entry is a val, and a ptr to a val */",
		"\tstruct VAL bv[MAX_B_VALUES+1];",
		"} B_VALUES;",
		"",
		"#ifdef realdef",
		"const struct VALNAMES b_valuenames[] = {",
		};
	static const char *const bottom[] = {
		"",
		"\t{ NULL,\tNULL,\tVALTYPE_INT, 0 }",
		"};",
		"#else",
		"extern const struct VALNAMES b_valuenames[];",
		"#endif",
		};

	write_lines(nemode, top);
	WriteIndexStruct(nemode, all_bmodes, "Buffers");
	WriteModeDefines(all_bmodes, "Buffers");
	write_lines(nemode, middle);
	WriteModeSymbols(all_bmodes);
	write_lines(nemode, bottom);
}

/******************************************************************************/
static void
start_evar_h(char **argv)
{
	static const char *const head[] = {
		"",
		"#if OPT_EVAL",
		"",
		"/*\tstructure to hold user variables and their definitions\t*/",
		"",
		"typedef struct UVAR {",
		"\tstruct UVAR *next;",
		"\tchar *u_name;\t\t/* name of user variable */",
		"\tchar *u_value;\t\t\t\t/* value (string) */",
		"} UVAR;",
		"",
		"decl_uninit( UVAR *user_vars );\t/* user variables */",
		"",
		};

	if (!nevars) {
		nevars = OpenHeader("nevars.h", argv);
		write_lines(nevars, head);
	}
}

static void
finish_evar_h(void)
{
	if (nevars)
		Fprintf(nevars, "\n#endif /* OPT_EVAL */\n");
}

/******************************************************************************/
static void
init_envars(void)
{
	static const char *const head[] = {
		"",
		"/*\tlist of recognized environment variables\t*/",
		"",
		"#ifdef realdef",
		"const char *const envars[] = {"
		};
	static	int	done;

	if (!done++)
		write_lines(nevars, head);
}

static void
save_envars(char **vec)
{
	/* insert into 'all_modes' to provide for common name-completion
	 * table, and into 'all_envars' to get name/index correspondence.
	 */
	InsertSorted(&all_modes,  vec[1], "env",  "", formcond("OPT_EVAL", vec[3]), "");
	InsertSorted(&all_envars, vec[1], vec[2], "", vec[3], vec[0]);
}

static void
dump_envars(void)
{
	static const char *const middle[] = {
		"\tNULL\t/* ends table for name-completion */",
		"};",
		"#else",
		"extern const char *const envars[];",
		"#endif",
		"",
		"/* \tand its preprocesor definitions\t\t*/",
		""
		};
	char	temp[MAX_BUFFER];
	register LIST *p;
	register int count;
#if OPT_IFDEF_MODES
	char	*s;
#endif

	BeginIf();
	for (p = all_envars, count = 0; p != 0; p = p->nst) {
		if (!count++)
			init_envars();
#if OPT_IFDEF_MODES
		WriteIf(nevars, p->Cond);
#endif
		Fprintf(nevars, "\t%s,\n", s = Name2Symbol(p->Name));
		free(s);
	}
	FlushIf(nevars);

	for (p = all_envars, count = 0; p != 0; p = p->nst) {
		if (!count++) {
			write_lines(nevars, middle);
			BeginIf();
			WriteIndexStruct(nevars, all_envars, "Vars");
		}
#if OPT_IFDEF_MODES
		WriteIf(nevars, p->Cond);
		Sprintf(temp, "#define\tEV%s", p->Func);
		(void)PadTo(24, temp);
		Sprintf(temp + strlen(temp), "Member_Offset(IndexVars, %s)",
			s = Name2Symbol(p->Name));
		free(s);
		Fprintf(nevars, "%s\n", temp);
#else
		Sprintf(temp, "#define\tEV%s", p->Func);
		Fprintf(nevars, "%s%d\n", PadTo(24, temp), count-1);
#endif
	}
	FlushIf(nevars);
}

/******************************************************************************/
static void
save_funcs(
char	*func,
char	*flags,
char	*cond,
char	*old_cond,
char	*help)
{
	char	temp[MAX_BUFFER];
	register char	*s;

	if (strcmp(cond, old_cond)) {
		if (*old_cond) {
			SaveEndif(all_funcs);
			SaveEndif(all__FUNCs);
			SaveEndif(all__CMDFs);
		}
		if (*cond) {
			Sprintf(temp, "#if %s", cond);
			InsertOnEnd(&all_funcs, temp);
			InsertOnEnd(&all__FUNCs, temp);
			InsertOnEnd(&all__CMDFs, temp);
		}
		(void)strcpy(old_cond, cond);
	}
	Sprintf(temp, "extern int %s ( int f, int n );", func);
	InsertOnEnd(&all_funcs, temp);

	s = append(strcpy(temp, "\tconst CMDFUNC f_"), func);
	(void)PadTo(32, temp);
	s = append(s, "= { ");
	s = append(s, func);
	s = append(s, ",");
	(void)PadTo(56, temp);
	s = append(s, flags);
	s = append(s, "\n#if OPT_ONLINEHELP\n\t\t,\"");
	s = append(s, help);
	(void)append(s, "\"\n#endif\n };");
	InsertOnEnd(&all__FUNCs, temp);

	s = append(strcpy(temp, "extern const CMDFUNC f_"), func);
	(void)append(s, ";");
	InsertOnEnd(&all__CMDFs, temp);
}

static void
dump_funcs(
FILE	*fp,
LIST	*head)
{
	register LIST *p;
	for (p = head; p != 0; p = p->nst)
		Fprintf(fp, "%s\n", p->Name);
}

/******************************************************************************/
static void
save_gmodes(
char	*type,
char	**vec)
{
	char *key = Mode2Key(type, vec[1], vec[4]);
	InsertSorted(&all_gmodes, key, vec[2], vec[3], vec[4], vec[0]);
#if NO_LEAKS
	free(key);
#endif
}

static void
dump_gmodes(void)
{
	static const char *const top[] = {
		"",
		"/* global mode flags\t*/",
		"/* the indices of G_VALUES.v[] */",
		};
	static const char *const middle[] = {
		"",
		"typedef struct G_VALUES {",
		"\t/* each entry is a val, and a ptr to a val */",
		"\tstruct VAL gv[MAX_G_VALUES+1];",
		"} G_VALUES;",
		"",
		"#ifdef realdef",
		"const struct VALNAMES g_valuenames[] = {",
		};
	static const char *const bottom[] = {
		"",
		"\t{ NULL,\tNULL,\tVALTYPE_INT, 0 }",
		"};",
		"#else",
		"extern const struct VALNAMES g_valuenames[];",
		"#endif",
		};

	write_lines(nemode, top);
	WriteIndexStruct(nemode, all_gmodes, "Globals");
	WriteModeDefines(all_gmodes, "Globals");
	write_lines(nemode, middle);
	WriteModeSymbols(all_gmodes);
	write_lines(nemode, bottom);
}

/******************************************************************************/
static void
save_names(char *name, char *func, char *cond)
{
	InsertSorted(&all_names, name, func, "", cond, "");
}

static void
dump_names(void)
{
	register LIST *m;
	char	temp[MAX_BUFFER];

	Fprintf(nename,"\n/* if you maintain this by hand, keep it in */\n");
	Fprintf(nename,"/* alphabetical order!!!! */\n\n");
	Fprintf(nename,"const NTAB nametbl[] = {\n");

	BeginIf();
	for (m = all_names; m != NULL; m = m->nst) {
		WriteIf(nename, m->Cond);
		Sprintf(temp, "\t{ \"%s\",", m->Name);
		Fprintf(nename, "%s&f_%s },\n", PadTo(40, temp), m->Func);
	}
	FlushIf(nename);
	Fprintf(nename,"\t{ NULL, NULL }\n};\n");
}

/******************************************************************************/
static void
init_ufuncs(void)
{
	static const char *const head[] = {
		"",
		"/*\tlist of recognized user functions\t*/",
		"",
		"typedef struct UFUNC {",
		"\tchar *f_name;\t/* name of function */",
		"\tint f_type;\t/* 1 = monamic, 2 = dynamic */",
		"} UFUNC;",
		"",
		"#define\tNILNAMIC\t0",
		"#define\tMONAMIC\t\t1",
		"#define\tDYNAMIC\t\t2",
		"#define\tTRINAMIC\t3",
		"",
		"#ifdef realdef",
		"const UFUNC funcs[] = {",
		};
	static	int	done;

	if (!done++)
		write_lines(nevars, head);
}

static void
save_ufuncs(char **vec)
{
	InsertSorted(&all_ufuncs, vec[1], vec[2], vec[3], "", vec[0]);
}

static void
dump_ufuncs(void)
{
	static	const char	*const middle[] = {
		"};",
		"#else",
		"extern const UFUNC funcs[];",
		"#endif",
		"",
		"/* \tand its preprocesor definitions\t\t*/",
		"",
		};
	char	temp[MAX_BUFFER];
	register LIST *p;
	register int	count;

	for (p = all_ufuncs, count = 0; p != 0; p = p->nst) {
		if (!count++)
			init_ufuncs();
		Sprintf(temp, "\t{\"%s\",", p->Name);
		(void)PadTo(15, temp);
		Sprintf(temp+strlen(temp), "%s},", p->Data);
		if (p->Note[0]) {
			(void)PadTo(32, temp);
			Sprintf(temp+strlen(temp), "/* %s */", p->Note);
		}
		Fprintf(nevars, "%s\n", temp);
	}
	for (p = all_ufuncs, count = 0; p != 0; p = p->nst) {
		if (!count)
			write_lines(nevars, middle);
		Sprintf(temp, "#define\tUF%s", p->Func);
		Fprintf(nevars, "%s%d\n", PadTo(24, temp), count++);
	}
	Fprintf(nevars, "\n#define\tNFUNCS\t\t%d\n", count);
}

/******************************************************************************/
static void
save_wmodes(
char	*type,
char	**vec)
{
	char *key = Mode2Key(type,vec[1],vec[4]);
	InsertSorted(&all_wmodes, key, vec[2], vec[3], vec[4], vec[0]);
#if NO_LEAKS
	free(key);
#endif
}

static void
dump_wmodes(void)
{
	static const char *top[] = {
		"",
		"/* these are the boolean, integer, and pointer value'd settings that are",
		"\tassociated with a window, and usually settable by a user.  There",
		"\tis a global set that is inherited into a buffer, and its windows",
		"\tin turn are inherit the buffer's set. */",
		};
	static const char *middle[] = {
		"",
		"typedef struct W_VALUES {",
		"\t/* each entry is a val, and a ptr to a val */",
		"\tstruct VAL wv[MAX_W_VALUES+1];",
		"} W_VALUES;",
		"",
		"#ifdef realdef",
		"const struct VALNAMES w_valuenames[] = {",
		};
	static const char *bottom[] = {
		"",
		"\t{ NULL,\tNULL,\tVALTYPE_INT, 0 }",
		"};",
		"#else",
		"extern const struct VALNAMES w_valuenames[];",
		"#endif",
		};

	write_lines(nemode, top);
	WriteIndexStruct(nemode, all_wmodes, "Windows");
	WriteModeDefines(all_wmodes, "Windows");
	write_lines(nemode, middle);
	WriteModeSymbols(all_wmodes);
	write_lines(nemode, bottom);
}

/******************************************************************************/
#if NO_LEAKS
static void
free_LIST (LIST **p)
{
	LIST	*q;

	while ((q = *p) != 0) {
		*p = q->nst;
		if (q->Name != Blank) FreeIfNeeded(q->Name);
		if (q->Func != Blank) FreeIfNeeded(q->Func);
		if (q->Data != Blank) FreeIfNeeded(q->Data);
		if (q->Cond != Blank) FreeIfNeeded(q->Cond);
		if (q->Note != Blank) FreeIfNeeded(q->Note);
		free((char *)q);
	}
}

/*
 * Free all memory allocated within 'mktbls'. This is used both for debugging
 * as well as for allowing 'mktbls' to be an application procedure that is
 * repeatedly invoked from a GUI.
 */
static void
free_mktbls (void)
{
	register int k;

	free_LIST(&all_names);
	free_LIST(&all_funcs);
	free_LIST(&all__FUNCs);
	free_LIST(&all__CMDFs);
	free_LIST(&all_envars);
	free_LIST(&all_ufuncs);
	free_LIST(&all_modes);
	free_LIST(&all_kbind);
	free_LIST(&all_gmodes);
	free_LIST(&all_bmodes);
	free_LIST(&all_wmodes);

	for (k = 0; k < LEN_CHRSET; k++) {
		FreeIfNeeded(bindings[k]);
		FreeIfNeeded(conditions[k]);
	}
#if DOALLOC
	show_alloc();
#endif
}
#else
#define free_mktbls()
#endif	/* NO_LEAKS */

/******************************************************************************/
int
main(int argc, char *argv[])
{
	char *vec[MAX_PARSE];
	char line[MAX_BUFFER];
	char func[LEN_BUFFER];
	char flags[LEN_BUFFER];
	char funchelp[MAX_BUFFER];
	char old_fcond[LEN_BUFFER],	fcond[LEN_BUFFER];
	char modetype[LEN_BUFFER];
	int section;
	int r;

	func[0] = flags[0] = fcond[0] = old_fcond[0] = modetype[0] = EOS;

	if (setjmp(my_top))
		ReturnFromMain(BADEXIT);

	if (argc != 2) {
		Fprintf(stderr, "usage: mktbls cmd-file\n");
		longjmp(my_top,1);
	}

	if ((cmdtbl = fopen(inputfile = argv[1],"r")) == NULL ) {
		Fprintf(stderr,"mktbls: couldn't open cmd-file\n");
		longjmp(my_top,1);
	}

	*old_fcond = EOS;
	section = SECT_CMDS;

	/* process each input line */
	while (fgets(line, sizeof(line), cmdtbl) != NULL) {
		char	col0	= line[0],
			col1	= line[1];

		l++;
		r = Parse(line, vec);

		switch (col0) {
		case '#':		/* comment */
		case '\n':		/* empty-list */
			break;

		case '.':		/* a new section */
			switch (col1) {
			case 'c':
				section = SECT_CMDS;
				break;
			case 'e':
				section = SECT_VARS;
				start_evar_h(argv);
				break;
			case 'f':
				section = SECT_FUNC;
				start_evar_h(argv);
				break;
			case 'g':
				section = SECT_GBLS;
				break;
			case 'b':
				section = SECT_BUFF;
				break;
			case 'w':
				section = SECT_WIND;
				break;
			default:
				badfmt("unknown section");
			}
			break;

		case '\t':		/* a new function */
			switch (section) {
			case SECT_CMDS:
				switch (col1) {
				case '"':	/* then it's an english name */
					if (r < 1 || r > 2)
						badfmt("looking for english name");

					save_names(vec[1], func, formcond(fcond,vec[2]));
					break;

				case '\'':	/* then it's a key */
					if (r < 1 || r > 3)
						badfmt("looking for key binding");

					if (strncmp("KEY_",vec[2],4) == 0) {
						if (strncmp("FN-",vec[1],3) != 0)
							badfmt("KEY_xxx definition must for FN- binding");
						if (!nefkeys)
								nefkeys = OpenHeader("nefkeys.h", argv);
						Fprintf(nefkeys, "#define %16s (SPEC|'%s')\n",
								vec[2],vec[1]+3);
						vec[2] = vec[3];
					}
					save_bindings(vec[1], func, formcond(fcond,vec[2]));
					break;

				case '<':	/* then it's a help string */
					/* put code here. */
					(void)strcpy(funchelp, vec[1]);
					break;

				default:
					badfmt("bad line");
				}
				break;

			case SECT_GBLS:
				if (r < 2 || r > 4)
					badfmt("looking for GLOBAL modes");
				save_gmodes(modetype, vec);
				break;

			case SECT_BUFF:
				if (r < 2 || r > 4)
					badfmt("looking for BUFFER modes");
				save_bmodes(modetype, vec);
				break;

			case SECT_WIND:
				if (r < 2 || r > 4)
					badfmt("looking for WINDOW modes");
				save_wmodes(modetype, vec);
				break;

			default:
				badfmt("did not expect a tab");
			}
			break;

		default:		/* cache information about funcs */
			switch (section) {
			case SECT_CMDS:
				if (r < 2 || r > 3)
					badfmt("looking for new function");

				/* don't save this yet -- we may get a
					a help line for it.  save the previous
					one now, and hang onto this one */
				if (func[0]) { /* flush the old one */
					save_funcs( func, flags, fcond, 
						old_fcond, funchelp);
					funchelp[0] = EOS;
				}
				(void)strcpy(func,  vec[1]);
				(void)strcpy(flags, vec[2]);
				(void)strcpy(fcond, vec[3]);
				break;

			case SECT_VARS:
				if (r < 2 || r > 3)
					badfmt("looking for char *envars[]");
				save_envars(vec);
				break;

			case SECT_FUNC:
				if (r < 2 || r > 3)
					badfmt("looking for UFUNC func[]");
				save_ufuncs(vec);
				break;

			case SECT_GBLS:
			case SECT_BUFF:
			case SECT_WIND:
				if (r != 1
				 || (!strcmp(vec[1], "bool")
				  && !strcmp(vec[1], "enum")
				  && !strcmp(vec[1], "int")
				  && !strcmp(vec[1], "string")
				  && !strcmp(vec[1], "regex")))
					badfmt("looking for mode datatype");
				(void)strcpy(modetype, vec[1]);
				break;

			default:
				badfmt("section not implemented");
			}
		}
	}

	if (func[0]) { /* flush the old one */
		save_funcs( func, flags, fcond, old_fcond, funchelp);
		funchelp[0] = EOS;
	}
	if (*old_fcond) {
		SaveEndif(all_funcs);
		SaveEndif(all__FUNCs);
		SaveEndif(all__CMDFs);
	}

	if (all_names) {
		nebind = OpenHeader("nebind.h", argv);
		nefunc = OpenHeader("nefunc.h", argv);
		neprot = OpenHeader("neproto.h", argv);
		nename = OpenHeader("nename.h", argv);
		dump_names();
		dump_bindings();
		dump_funcs(neprot, all_funcs);

		Fprintf(nefunc, "\n#ifdef real_CMDFUNCS\n\n");
		dump_funcs(nefunc, all__FUNCs);
		Fprintf(nefunc, "\n#else\n\n");
		dump_funcs(nefunc, all__CMDFs);
		Fprintf(nefunc, "\n#endif\n");
	}

	if (all_envars) {
		dump_envars();
		dump_ufuncs();
		finish_evar_h();
	}

	if (all_wmodes || all_bmodes) {
		nemode = OpenHeader("nemode.h", argv);
		DefineOffset(nemode);
		dump_all_modes();
		dump_gmodes();
		dump_wmodes();
		dump_bmodes();
	}

	free_mktbls();
	ReturnFromMain(GOODEXIT);
	/*NOTREACHED*/
}
