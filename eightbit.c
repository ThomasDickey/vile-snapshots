/*
 * $Id: eightbit.c,v 1.74 2010/05/12 09:21:41 tom Exp $
 *
 * Maintain "8bit" file-encoding mode by converting incoming UTF-8 to single
 * bytes, and providing a function that tells vile whether a given Unicode
 * value will map to the eight-bit value in the "narrow" locale.
 */

#include <estruct.h>
#include <chgdfunc.h>
#include <edef.h>
#include <nefsms.h>

#if OPT_LOCALE
#include <locale.h>
#endif

#if OPT_ICONV_FUNCS
#include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#define StrMalloc(s) ((s) ? strmalloc(s) : 0)

#if DISP_TERMCAP || DISP_CURSES || DISP_BORLAND
#define USE_MBTERM 1
static int (*save_getch) (void);
static OUTC_DCL(*save_putch) (int c);
#else
#define USE_MBTERM 0
#endif

/******************************************************************************/
CHARTYPE vl_ctype_ascii[] =
{
    (vl_cntrl),			/* 0:^@ */
    (vl_cntrl),			/* 1:^A */
    (vl_cntrl),			/* 2:^B */
    (vl_cntrl),			/* 3:^C */
    (vl_cntrl),			/* 4:^D */
    (vl_cntrl),			/* 5:^E */
    (vl_cntrl),			/* 6:^F */
    (vl_cntrl),			/* 7:^G */
    (vl_cntrl),			/* 8:^H */
    (vl_space | vl_cntrl),	/* 9:^I */
    (vl_space | vl_cntrl),	/* 10:^J */
    (vl_space | vl_cntrl),	/* 11:^K */
    (vl_space | vl_cntrl),	/* 12:^L */
    (vl_space | vl_cntrl),	/* 13:^M */
    (vl_cntrl),			/* 14:^N */
    (vl_cntrl),			/* 15:^O */
    (vl_cntrl),			/* 16:^P */
    (vl_cntrl),			/* 17:^Q */
    (vl_cntrl),			/* 18:^R */
    (vl_cntrl),			/* 19:^S */
    (vl_cntrl),			/* 20:^T */
    (vl_cntrl),			/* 21:^U */
    (vl_cntrl),			/* 22:^V */
    (vl_cntrl),			/* 23:^W */
    (vl_cntrl),			/* 24:^X */
    (vl_cntrl),			/* 25:^Y */
    (vl_cntrl),			/* 26:^Z */
    (vl_cntrl),			/* 27:^[ */
    (vl_cntrl),			/* 28:^\ */
    (vl_cntrl),			/* 29:^] */
    (vl_cntrl),			/* 30:^^ */
    (vl_cntrl),			/* 31:^_ */
    (vl_space | vl_print),	/* 32:  */
    (vl_print | vl_punct),	/* 33:! */
    (vl_print | vl_punct),	/* 34:" */
    (vl_print | vl_punct),	/* 35:# */
    (vl_print | vl_punct),	/* 36:$ */
    (vl_print | vl_punct),	/* 37:% */
    (vl_print | vl_punct),	/* 38:& */
    (vl_print | vl_punct),	/* 39:' */
    (vl_print | vl_punct),	/* 40:( */
    (vl_print | vl_punct),	/* 41:) */
    (vl_print | vl_punct),	/* 42:* */
    (vl_print | vl_punct),	/* 43:+ */
    (vl_print | vl_punct),	/* 44:, */
    (vl_print | vl_punct),	/* 45:- */
    (vl_print | vl_punct),	/* 46:. */
    (vl_print | vl_punct),	/* 47:/ */
    (vl_digit | vl_print | vl_xdigit),	/* 48:0 */
    (vl_digit | vl_print | vl_xdigit),	/* 49:1 */
    (vl_digit | vl_print | vl_xdigit),	/* 50:2 */
    (vl_digit | vl_print | vl_xdigit),	/* 51:3 */
    (vl_digit | vl_print | vl_xdigit),	/* 52:4 */
    (vl_digit | vl_print | vl_xdigit),	/* 53:5 */
    (vl_digit | vl_print | vl_xdigit),	/* 54:6 */
    (vl_digit | vl_print | vl_xdigit),	/* 55:7 */
    (vl_digit | vl_print | vl_xdigit),	/* 56:8 */
    (vl_digit | vl_print | vl_xdigit),	/* 57:9 */
    (vl_print | vl_punct),	/* 58:: */
    (vl_print | vl_punct),	/* 59:; */
    (vl_print | vl_punct),	/* 60:< */
    (vl_print | vl_punct),	/* 61:= */
    (vl_print | vl_punct),	/* 62:> */
    (vl_print | vl_punct),	/* 63:? */
    (vl_print | vl_punct),	/* 64:@ */
    (vl_upper | vl_print | vl_xdigit),	/* 65:A */
    (vl_upper | vl_print | vl_xdigit),	/* 66:B */
    (vl_upper | vl_print | vl_xdigit),	/* 67:C */
    (vl_upper | vl_print | vl_xdigit),	/* 68:D */
    (vl_upper | vl_print | vl_xdigit),	/* 69:E */
    (vl_upper | vl_print | vl_xdigit),	/* 70:F */
    (vl_upper | vl_print),	/* 71:G */
    (vl_upper | vl_print),	/* 72:H */
    (vl_upper | vl_print),	/* 73:I */
    (vl_upper | vl_print),	/* 74:J */
    (vl_upper | vl_print),	/* 75:K */
    (vl_upper | vl_print),	/* 76:L */
    (vl_upper | vl_print),	/* 77:M */
    (vl_upper | vl_print),	/* 78:N */
    (vl_upper | vl_print),	/* 79:O */
    (vl_upper | vl_print),	/* 80:P */
    (vl_upper | vl_print),	/* 81:Q */
    (vl_upper | vl_print),	/* 82:R */
    (vl_upper | vl_print),	/* 83:S */
    (vl_upper | vl_print),	/* 84:T */
    (vl_upper | vl_print),	/* 85:U */
    (vl_upper | vl_print),	/* 86:V */
    (vl_upper | vl_print),	/* 87:W */
    (vl_upper | vl_print),	/* 88:X */
    (vl_upper | vl_print),	/* 89:Y */
    (vl_upper | vl_print),	/* 90:Z */
    (vl_print | vl_punct),	/* 91:[ */
    (vl_print | vl_punct),	/* 92:\ */
    (vl_print | vl_punct),	/* 93:] */
    (vl_print | vl_punct),	/* 94:^ */
    (vl_print | vl_punct),	/* 95:_ */
    (vl_print | vl_punct),	/* 96:` */
    (vl_lower | vl_print | vl_xdigit),	/* 97:a */
    (vl_lower | vl_print | vl_xdigit),	/* 98:b */
    (vl_lower | vl_print | vl_xdigit),	/* 99:c */
    (vl_lower | vl_print | vl_xdigit),	/* 100:d */
    (vl_lower | vl_print | vl_xdigit),	/* 101:e */
    (vl_lower | vl_print | vl_xdigit),	/* 102:f */
    (vl_lower | vl_print),	/* 103:g */
    (vl_lower | vl_print),	/* 104:h */
    (vl_lower | vl_print),	/* 105:i */
    (vl_lower | vl_print),	/* 106:j */
    (vl_lower | vl_print),	/* 107:k */
    (vl_lower | vl_print),	/* 108:l */
    (vl_lower | vl_print),	/* 109:m */
    (vl_lower | vl_print),	/* 110:n */
    (vl_lower | vl_print),	/* 111:o */
    (vl_lower | vl_print),	/* 112:p */
    (vl_lower | vl_print),	/* 113:q */
    (vl_lower | vl_print),	/* 114:r */
    (vl_lower | vl_print),	/* 115:s */
    (vl_lower | vl_print),	/* 116:t */
    (vl_lower | vl_print),	/* 117:u */
    (vl_lower | vl_print),	/* 118:v */
    (vl_lower | vl_print),	/* 119:w */
    (vl_lower | vl_print),	/* 120:x */
    (vl_lower | vl_print),	/* 121:y */
    (vl_lower | vl_print),	/* 122:z */
    (vl_print | vl_punct),	/* 123:{ */
    (vl_print | vl_punct),	/* 124:| */
    (vl_print | vl_punct),	/* 125:} */
    (vl_print | vl_punct),	/* 126:~ */
    (vl_cntrl),			/* 127:^? */
    (0),			/* 128:\200 */
    (0),			/* 129:\201 */
    (0),			/* 130:\202 */
    (0),			/* 131:\203 */
    (0),			/* 132:\204 */
    (0),			/* 133:\205 */
    (0),			/* 134:\206 */
    (0),			/* 135:\207 */
    (0),			/* 136:\210 */
    (0),			/* 137:\211 */
    (0),			/* 138:\212 */
    (0),			/* 139:\213 */
    (0),			/* 140:\214 */
    (0),			/* 141:\215 */
    (0),			/* 142:\216 */
    (0),			/* 143:\217 */
    (0),			/* 144:\220 */
    (0),			/* 145:\221 */
    (0),			/* 146:\222 */
    (0),			/* 147:\223 */
    (0),			/* 148:\224 */
    (0),			/* 149:\225 */
    (0),			/* 150:\226 */
    (0),			/* 151:\227 */
    (0),			/* 152:\230 */
    (0),			/* 153:\231 */
    (0),			/* 154:\232 */
    (0),			/* 155:\233 */
    (0),			/* 156:\234 */
    (0),			/* 157:\235 */
    (0),			/* 158:\236 */
    (0),			/* 159:\237 */
    (0),			/* 160:\240 */
    (0),			/* 161:\241 */
    (0),			/* 162:\242 */
    (0),			/* 163:\243 */
    (0),			/* 164:\244 */
    (0),			/* 165:\245 */
    (0),			/* 166:\246 */
    (0),			/* 167:\247 */
    (0),			/* 168:\250 */
    (0),			/* 169:\251 */
    (0),			/* 170:\252 */
    (0),			/* 171:\253 */
    (0),			/* 172:\254 */
    (0),			/* 173:\255 */
    (0),			/* 174:\256 */
    (0),			/* 175:\257 */
    (0),			/* 176:\260 */
    (0),			/* 177:\261 */
    (0),			/* 178:\262 */
    (0),			/* 179:\263 */
    (0),			/* 180:\264 */
    (0),			/* 181:\265 */
    (0),			/* 182:\266 */
    (0),			/* 183:\267 */
    (0),			/* 184:\270 */
    (0),			/* 185:\271 */
    (0),			/* 186:\272 */
    (0),			/* 187:\273 */
    (0),			/* 188:\274 */
    (0),			/* 189:\275 */
    (0),			/* 190:\276 */
    (0),			/* 191:\277 */
    (0),			/* 192:\300 */
    (0),			/* 193:\301 */
    (0),			/* 194:\302 */
    (0),			/* 195:\303 */
    (0),			/* 196:\304 */
    (0),			/* 197:\305 */
    (0),			/* 198:\306 */
    (0),			/* 199:\307 */
    (0),			/* 200:\310 */
    (0),			/* 201:\311 */
    (0),			/* 202:\312 */
    (0),			/* 203:\313 */
    (0),			/* 204:\314 */
    (0),			/* 205:\315 */
    (0),			/* 206:\316 */
    (0),			/* 207:\317 */
    (0),			/* 208:\320 */
    (0),			/* 209:\321 */
    (0),			/* 210:\322 */
    (0),			/* 211:\323 */
    (0),			/* 212:\324 */
    (0),			/* 213:\325 */
    (0),			/* 214:\326 */
    (0),			/* 215:\327 */
    (0),			/* 216:\330 */
    (0),			/* 217:\331 */
    (0),			/* 218:\332 */
    (0),			/* 219:\333 */
    (0),			/* 220:\334 */
    (0),			/* 221:\335 */
    (0),			/* 222:\336 */
    (0),			/* 223:\337 */
    (0),			/* 224:\340 */
    (0),			/* 225:\341 */
    (0),			/* 226:\342 */
    (0),			/* 227:\343 */
    (0),			/* 228:\344 */
    (0),			/* 229:\345 */
    (0),			/* 230:\346 */
    (0),			/* 231:\347 */
    (0),			/* 232:\350 */
    (0),			/* 233:\351 */
    (0),			/* 234:\352 */
    (0),			/* 235:\353 */
    (0),			/* 236:\354 */
    (0),			/* 237:\355 */
    (0),			/* 238:\356 */
    (0),			/* 239:\357 */
    (0),			/* 240:\360 */
    (0),			/* 241:\361 */
    (0),			/* 242:\362 */
    (0),			/* 243:\363 */
    (0),			/* 244:\364 */
    (0),			/* 245:\365 */
    (0),			/* 246:\366 */
    (0),			/* 247:\367 */
    (0),			/* 248:\370 */
    (0),			/* 249:\371 */
    (0),			/* 250:\372 */
    (0),			/* 251:\373 */
    (0),			/* 252:\374 */
    (0),			/* 253:\375 */
    (0),			/* 254:\376 */
    (0),			/* 255:\377 */
};
/******************************************************************************/
CHARTYPE vl_ctype_latin1[] =
{
    (vl_cntrl),			/* 0:^@ */
    (vl_cntrl),			/* 1:^A */
    (vl_cntrl),			/* 2:^B */
    (vl_cntrl),			/* 3:^C */
    (vl_cntrl),			/* 4:^D */
    (vl_cntrl),			/* 5:^E */
    (vl_cntrl),			/* 6:^F */
    (vl_cntrl),			/* 7:^G */
    (vl_cntrl),			/* 8:^H */
    (vl_space | vl_cntrl),	/* 9:^I */
    (vl_space | vl_cntrl),	/* 10:^J */
    (vl_space | vl_cntrl),	/* 11:^K */
    (vl_space | vl_cntrl),	/* 12:^L */
    (vl_space | vl_cntrl),	/* 13:^M */
    (vl_cntrl),			/* 14:^N */
    (vl_cntrl),			/* 15:^O */
    (vl_cntrl),			/* 16:^P */
    (vl_cntrl),			/* 17:^Q */
    (vl_cntrl),			/* 18:^R */
    (vl_cntrl),			/* 19:^S */
    (vl_cntrl),			/* 20:^T */
    (vl_cntrl),			/* 21:^U */
    (vl_cntrl),			/* 22:^V */
    (vl_cntrl),			/* 23:^W */
    (vl_cntrl),			/* 24:^X */
    (vl_cntrl),			/* 25:^Y */
    (vl_cntrl),			/* 26:^Z */
    (vl_cntrl),			/* 27:^[ */
    (vl_cntrl),			/* 28:^\ */
    (vl_cntrl),			/* 29:^] */
    (vl_cntrl),			/* 30:^^ */
    (vl_cntrl),			/* 31:^_ */
    (vl_space | vl_print),	/* 32:  */
    (vl_print | vl_punct),	/* 33:! */
    (vl_print | vl_punct),	/* 34:" */
    (vl_print | vl_punct),	/* 35:# */
    (vl_print | vl_punct),	/* 36:$ */
    (vl_print | vl_punct),	/* 37:% */
    (vl_print | vl_punct),	/* 38:& */
    (vl_print | vl_punct),	/* 39:' */
    (vl_print | vl_punct),	/* 40:( */
    (vl_print | vl_punct),	/* 41:) */
    (vl_print | vl_punct),	/* 42:* */
    (vl_print | vl_punct),	/* 43:+ */
    (vl_print | vl_punct),	/* 44:, */
    (vl_print | vl_punct),	/* 45:- */
    (vl_print | vl_punct),	/* 46:. */
    (vl_print | vl_punct),	/* 47:/ */
    (vl_digit | vl_print | vl_xdigit),	/* 48:0 */
    (vl_digit | vl_print | vl_xdigit),	/* 49:1 */
    (vl_digit | vl_print | vl_xdigit),	/* 50:2 */
    (vl_digit | vl_print | vl_xdigit),	/* 51:3 */
    (vl_digit | vl_print | vl_xdigit),	/* 52:4 */
    (vl_digit | vl_print | vl_xdigit),	/* 53:5 */
    (vl_digit | vl_print | vl_xdigit),	/* 54:6 */
    (vl_digit | vl_print | vl_xdigit),	/* 55:7 */
    (vl_digit | vl_print | vl_xdigit),	/* 56:8 */
    (vl_digit | vl_print | vl_xdigit),	/* 57:9 */
    (vl_print | vl_punct),	/* 58:: */
    (vl_print | vl_punct),	/* 59:; */
    (vl_print | vl_punct),	/* 60:< */
    (vl_print | vl_punct),	/* 61:= */
    (vl_print | vl_punct),	/* 62:> */
    (vl_print | vl_punct),	/* 63:? */
    (vl_print | vl_punct),	/* 64:@ */
    (vl_upper | vl_print | vl_xdigit),	/* 65:A */
    (vl_upper | vl_print | vl_xdigit),	/* 66:B */
    (vl_upper | vl_print | vl_xdigit),	/* 67:C */
    (vl_upper | vl_print | vl_xdigit),	/* 68:D */
    (vl_upper | vl_print | vl_xdigit),	/* 69:E */
    (vl_upper | vl_print | vl_xdigit),	/* 70:F */
    (vl_upper | vl_print),	/* 71:G */
    (vl_upper | vl_print),	/* 72:H */
    (vl_upper | vl_print),	/* 73:I */
    (vl_upper | vl_print),	/* 74:J */
    (vl_upper | vl_print),	/* 75:K */
    (vl_upper | vl_print),	/* 76:L */
    (vl_upper | vl_print),	/* 77:M */
    (vl_upper | vl_print),	/* 78:N */
    (vl_upper | vl_print),	/* 79:O */
    (vl_upper | vl_print),	/* 80:P */
    (vl_upper | vl_print),	/* 81:Q */
    (vl_upper | vl_print),	/* 82:R */
    (vl_upper | vl_print),	/* 83:S */
    (vl_upper | vl_print),	/* 84:T */
    (vl_upper | vl_print),	/* 85:U */
    (vl_upper | vl_print),	/* 86:V */
    (vl_upper | vl_print),	/* 87:W */
    (vl_upper | vl_print),	/* 88:X */
    (vl_upper | vl_print),	/* 89:Y */
    (vl_upper | vl_print),	/* 90:Z */
    (vl_print | vl_punct),	/* 91:[ */
    (vl_print | vl_punct),	/* 92:\ */
    (vl_print | vl_punct),	/* 93:] */
    (vl_print | vl_punct),	/* 94:^ */
    (vl_print | vl_punct),	/* 95:_ */
    (vl_print | vl_punct),	/* 96:` */
    (vl_lower | vl_print | vl_xdigit),	/* 97:a */
    (vl_lower | vl_print | vl_xdigit),	/* 98:b */
    (vl_lower | vl_print | vl_xdigit),	/* 99:c */
    (vl_lower | vl_print | vl_xdigit),	/* 100:d */
    (vl_lower | vl_print | vl_xdigit),	/* 101:e */
    (vl_lower | vl_print | vl_xdigit),	/* 102:f */
    (vl_lower | vl_print),	/* 103:g */
    (vl_lower | vl_print),	/* 104:h */
    (vl_lower | vl_print),	/* 105:i */
    (vl_lower | vl_print),	/* 106:j */
    (vl_lower | vl_print),	/* 107:k */
    (vl_lower | vl_print),	/* 108:l */
    (vl_lower | vl_print),	/* 109:m */
    (vl_lower | vl_print),	/* 110:n */
    (vl_lower | vl_print),	/* 111:o */
    (vl_lower | vl_print),	/* 112:p */
    (vl_lower | vl_print),	/* 113:q */
    (vl_lower | vl_print),	/* 114:r */
    (vl_lower | vl_print),	/* 115:s */
    (vl_lower | vl_print),	/* 116:t */
    (vl_lower | vl_print),	/* 117:u */
    (vl_lower | vl_print),	/* 118:v */
    (vl_lower | vl_print),	/* 119:w */
    (vl_lower | vl_print),	/* 120:x */
    (vl_lower | vl_print),	/* 121:y */
    (vl_lower | vl_print),	/* 122:z */
    (vl_print | vl_punct),	/* 123:{ */
    (vl_print | vl_punct),	/* 124:| */
    (vl_print | vl_punct),	/* 125:} */
    (vl_print | vl_punct),	/* 126:~ */
    (vl_cntrl),			/* 127:^? */
    (vl_cntrl),			/* 128:\200 */
    (vl_cntrl),			/* 129:\201 */
    (vl_cntrl),			/* 130:\202 */
    (vl_cntrl),			/* 131:\203 */
    (vl_cntrl),			/* 132:\204 */
    (vl_cntrl),			/* 133:\205 */
    (vl_cntrl),			/* 134:\206 */
    (vl_cntrl),			/* 135:\207 */
    (vl_cntrl),			/* 136:\210 */
    (vl_cntrl),			/* 137:\211 */
    (vl_cntrl),			/* 138:\212 */
    (vl_cntrl),			/* 139:\213 */
    (vl_cntrl),			/* 140:\214 */
    (vl_cntrl),			/* 141:\215 */
    (vl_cntrl),			/* 142:\216 */
    (vl_cntrl),			/* 143:\217 */
    (vl_cntrl),			/* 144:\220 */
    (vl_cntrl),			/* 145:\221 */
    (vl_cntrl),			/* 146:\222 */
    (vl_cntrl),			/* 147:\223 */
    (vl_cntrl),			/* 148:\224 */
    (vl_cntrl),			/* 149:\225 */
    (vl_cntrl),			/* 150:\226 */
    (vl_cntrl),			/* 151:\227 */
    (vl_cntrl),			/* 152:\230 */
    (vl_cntrl),			/* 153:\231 */
    (vl_cntrl),			/* 154:\232 */
    (vl_cntrl),			/* 155:\233 */
    (vl_cntrl),			/* 156:\234 */
    (vl_cntrl),			/* 157:\235 */
    (vl_cntrl),			/* 158:\236 */
    (vl_cntrl),			/* 159:\237 */
    (vl_print | vl_punct),	/* 160:  */
    (vl_print | vl_punct),	/* 161:¡ */
    (vl_print | vl_punct),	/* 162:¢ */
    (vl_print | vl_punct),	/* 163:£ */
    (vl_print | vl_punct),	/* 164:¤ */
    (vl_print | vl_punct),	/* 165:¥ */
    (vl_print | vl_punct),	/* 166:¦ */
    (vl_print | vl_punct),	/* 167:§ */
    (vl_print | vl_punct),	/* 168:¨ */
    (vl_print | vl_punct),	/* 169:© */
    (vl_print),			/* 170:ª */
    (vl_print | vl_punct),	/* 171:« */
    (vl_print | vl_punct),	/* 172:¬ */
    (vl_print | vl_punct),	/* 173:­ */
    (vl_print | vl_punct),	/* 174:® */
    (vl_print | vl_punct),	/* 175:¯ */
    (vl_print | vl_punct),	/* 176:° */
    (vl_print | vl_punct),	/* 177:± */
    (vl_print | vl_punct),	/* 178:² */
    (vl_print | vl_punct),	/* 179:³ */
    (vl_print | vl_punct),	/* 180:´ */
    (vl_lower | vl_print),	/* 181:µ */
    (vl_print | vl_punct),	/* 182:¶ */
    (vl_print | vl_punct),	/* 183:· */
    (vl_print | vl_punct),	/* 184:¸ */
    (vl_print | vl_punct),	/* 185:¹ */
    (vl_print),			/* 186:º */
    (vl_print | vl_punct),	/* 187:» */
    (vl_print | vl_punct),	/* 188:¼ */
    (vl_print | vl_punct),	/* 189:½ */
    (vl_print | vl_punct),	/* 190:¾ */
    (vl_print | vl_punct),	/* 191:¿ */
    (vl_upper | vl_print),	/* 192:À */
    (vl_upper | vl_print),	/* 193:Á */
    (vl_upper | vl_print),	/* 194:Â */
    (vl_upper | vl_print),	/* 195:Ã */
    (vl_upper | vl_print),	/* 196:Ä */
    (vl_upper | vl_print),	/* 197:Å */
    (vl_upper | vl_print),	/* 198:Æ */
    (vl_upper | vl_print),	/* 199:Ç */
    (vl_upper | vl_print),	/* 200:È */
    (vl_upper | vl_print),	/* 201:É */
    (vl_upper | vl_print),	/* 202:Ê */
    (vl_upper | vl_print),	/* 203:Ë */
    (vl_upper | vl_print),	/* 204:Ì */
    (vl_upper | vl_print),	/* 205:Í */
    (vl_upper | vl_print),	/* 206:Î */
    (vl_upper | vl_print),	/* 207:Ï */
    (vl_upper | vl_print),	/* 208:Ð */
    (vl_upper | vl_print),	/* 209:Ñ */
    (vl_upper | vl_print),	/* 210:Ò */
    (vl_upper | vl_print),	/* 211:Ó */
    (vl_upper | vl_print),	/* 212:Ô */
    (vl_upper | vl_print),	/* 213:Õ */
    (vl_upper | vl_print),	/* 214:Ö */
    (vl_print | vl_punct),	/* 215:× */
    (vl_upper | vl_print),	/* 216:Ø */
    (vl_upper | vl_print),	/* 217:Ù */
    (vl_upper | vl_print),	/* 218:Ú */
    (vl_upper | vl_print),	/* 219:Û */
    (vl_upper | vl_print),	/* 220:Ü */
    (vl_upper | vl_print),	/* 221:Ý */
    (vl_upper | vl_print),	/* 222:Þ */
    (vl_lower | vl_print),	/* 223:ß */
    (vl_lower | vl_print),	/* 224:à */
    (vl_lower | vl_print),	/* 225:á */
    (vl_lower | vl_print),	/* 226:â */
    (vl_lower | vl_print),	/* 227:ã */
    (vl_lower | vl_print),	/* 228:ä */
    (vl_lower | vl_print),	/* 229:å */
    (vl_lower | vl_print),	/* 230:æ */
    (vl_lower | vl_print),	/* 231:ç */
    (vl_lower | vl_print),	/* 232:è */
    (vl_lower | vl_print),	/* 233:é */
    (vl_lower | vl_print),	/* 234:ê */
    (vl_lower | vl_print),	/* 235:ë */
    (vl_lower | vl_print),	/* 236:ì */
    (vl_lower | vl_print),	/* 237:í */
    (vl_lower | vl_print),	/* 238:î */
    (vl_lower | vl_print),	/* 239:ï */
    (vl_lower | vl_print),	/* 240:ð */
    (vl_lower | vl_print),	/* 241:ñ */
    (vl_lower | vl_print),	/* 242:ò */
    (vl_lower | vl_print),	/* 243:ó */
    (vl_lower | vl_print),	/* 244:ô */
    (vl_lower | vl_print),	/* 245:õ */
    (vl_lower | vl_print),	/* 246:ö */
    (vl_print | vl_punct),	/* 247:÷ */
    (vl_lower | vl_print),	/* 248:ø */
    (vl_lower | vl_print),	/* 249:ù */
    (vl_lower | vl_print),	/* 250:ú */
    (vl_lower | vl_print),	/* 251:û */
    (vl_lower | vl_print),	/* 252:ü */
    (vl_lower | vl_print),	/* 253:ý */
    (vl_lower | vl_print),	/* 254:þ */
    (vl_lower | vl_print),	/* 255:ÿ */
};

/******************************************************************************/

typedef struct {
    UINT code;			/* Unicode value */
    char *text;			/* UTF-8 string */
} TABLE_8BIT;

static TABLE_8BIT table_8bit_utf8[N_chars];

typedef struct {
    UINT code;			/* Unicode value */
    int rinx;			/* actual index in table_8bit_utf8[] */
} RINDEX_8BIT;

static RINDEX_8BIT rindex_8bit[N_chars];

static int
cmp_rindex(const void *a, const void *b)
{
    const RINDEX_8BIT *p = (const RINDEX_8BIT *) a;
    const RINDEX_8BIT *q = (const RINDEX_8BIT *) b;
    return (int) p->code - (int) q->code;
}

#if OPT_ICONV_FUNCS
#define NO_ICONV  (iconv_t)(-1)

static iconv_t mb_desc = NO_ICONV;

static void
close_encoding(void)
{
    if (mb_desc != NO_ICONV) {
	iconv_close(mb_desc);
	mb_desc = NO_ICONV;
    }
}

static int
try_encoding(const char *from, const char *to)
{
    close_encoding();

    mb_desc = iconv_open(to, from);

    TRACE(("try_encoding(from=%s, to=%s) %s\n",
	   from, to,
	   ((mb_desc != NO_ICONV)
	    ? "OK"
	    : "FAIL")));

    return (mb_desc != NO_ICONV);
}

static void
open_encoding(char *from, char *to)
{
    if (!try_encoding(from, to)) {
	fprintf(stderr, "Cannot setup translation from %s to %s\n", from, to);
	tidy_exit(BADEXIT);
    }
}

static void
initialize_table_8bit_utf8(void)
{
    int n;

    for (n = 0; n < N_chars; ++n) {
	size_t converted;
	char input[80];
	ICONV_CONST char *ip = input;
	char output[80];
	char *op = output;
	size_t in_bytes = 1;
	size_t out_bytes = sizeof(output);
	input[0] = (char) n;
	input[1] = 0;
	converted = iconv(mb_desc, &ip, &in_bytes, &op, &out_bytes);
	if (converted == (size_t) (-1)) {
	    TRACE(("err:%d\n", errno));
	    TRACE(("convert(%d) %d %d/%d\n", n,
		   (int) converted, (int) in_bytes, (int) out_bytes));
	} else {
	    output[sizeof(output) - out_bytes] = 0;
	    FreeIfNeeded(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = strmalloc(output);
	    vl_conv_to_utf32(&(table_8bit_utf8[n].code),
			     table_8bit_utf8[n].text,
			     strlen(table_8bit_utf8[n].text));
#if OPT_TRACE > 1
	    if (n != (int) table_8bit_utf8[n].code)
		TRACE(("8bit_utf8 %d:%#4x\n", n, table_8bit_utf8[n].code));
#endif
	}
    }
}
#endif /* OPT_ICONV_FUNCS */

char *
vl_narrowed(const char *wide)
{
    char *result = 0;

    TRACE((T_CALLED "vl_narrowed(%s)\n", NonNull(wide)));
    if (wide != 0) {
	const char *mapping = getenv("VILE_LOCALE_MAPPING");
	const char *parsed;
	char *on_left;
	char *on_right;
	regexp *exp;
	int n;

	/*
	 * The default mapping strips ".UTF-8", ".utf-8", ".UTF8" or ".utf8",
	 * and assumes that the default locale alias corresponds to the 8-bit
	 * encoding.  That is true on many systems.
	 */
	if (mapping == 0)
	    mapping = "/\\.\\(UTF\\|utf\\)[-]\\?8$//";

	parsed = mapping;
	on_left = regparser(&parsed);
	on_right = regparser(&parsed);
	if (on_left != 0
	    && on_right != 0
	    && parsed[1] == '\0'
	    && (exp = regcomp(on_left, strlen(on_left), TRUE)) != 0) {
	    int len = (int) strlen(wide);
	    int found = 0;

	    if ((result = malloc(strlen(wide) + 2 + strlen(on_right))) != 0) {
		strcpy(result, wide);
		for (n = 0; n < len; ++n) {
		    found = regexec(exp, result, result + len, n, len);
		    if (found)
			break;
		}

		/*
		 * Substitute the result (back-substitutions not supported).
		 */
		if (found) {
		    char *save = strmalloc(exp->endp[0]);

		    TRACE(("vl_narrowed match \"%.*s\", replace with \"%s\"\n",
			   (int) (exp->endp[0] - exp->startp[0]),
			   exp->startp[0],
			   on_right));

		    strcpy(exp->startp[0], on_right);
		    strcat(exp->startp[0], save);
		    free(save);
		} else {
		    free(result);
		    result = 0;
		}
	    }
	    regfree(exp);
	} else {
	    fprintf(stderr, "VILE_LOCAL_MAPPING error:%s\n", mapping);
	}
	FreeIfNeeded(on_left);
	FreeIfNeeded(on_right);
    }
    returnString(result);
}

/*
 * Do the rest of the work for setting up the wide- and narrow-locales.
 *
 * Set vl_encoding to the internal code used for resolving encoding of
 * buffers whose file-encoding is "locale" (enc_LOCALE).
 */
void
vl_init_8bit(const char *wide, const char *narrow)
{
    int fixup, fixed_up;
    int n;

    TRACE((T_CALLED "vl_init_8bit(%s, %s)\n", NonNull(wide), NonNull(narrow)));
    if (wide == 0 || narrow == 0) {
	TRACE(("setup POSIX-locale\n"));
	vl_encoding = enc_POSIX;
	vl_wide_enc.locale = 0;
	vl_narrow_enc.locale = 0;
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
    } else if (strcmp(wide, narrow)) {
	TRACE(("setup mixed-locale(%s, %s)\n", wide, narrow));
	vl_wide_enc.locale = StrMalloc(wide);
	vl_narrow_enc.locale = StrMalloc(narrow);
	vl_get_encoding(&vl_wide_enc.encoding, wide);
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
	TRACE(("...setup_locale(%s, %s)\n",
	       NONNULL(vl_wide_enc.encoding),
	       NONNULL(vl_narrow_enc.encoding)));

	if (vl_is_utf8_encoding(vl_wide_enc.encoding)) {
	    vl_encoding = enc_UTF8;
	} else {
	    vl_encoding = enc_8BIT;
	}

	/*
	 * If the wide/narrow encodings do not differ, that is probably because
	 * the narrow encoding is really a wide-encoding.
	 */
#if OPT_ICONV_FUNCS
	if (vl_narrow_enc.encoding != 0
	    && vl_wide_enc.encoding != 0
	    && strcmp(vl_narrow_enc.encoding, vl_wide_enc.encoding)) {
	    open_encoding(vl_narrow_enc.encoding, vl_wide_enc.encoding);
	    initialize_table_8bit_utf8();
	    close_encoding();

	    /*
	     * If we were able to convert in one direction, the other should
	     * succeed in vl_mb_getch().
	     */
	    open_encoding(vl_wide_enc.encoding, vl_narrow_enc.encoding);
	}
#endif
    } else {
	TRACE(("setup narrow-locale(%s)\n", narrow));
	vl_encoding = enc_8BIT;
	vl_wide_enc.locale = 0;
	vl_narrow_enc.locale = StrMalloc(narrow);
	vl_get_encoding(&vl_narrow_enc.encoding, narrow);
#if OPT_ICONV_FUNCS
	if (try_encoding(vl_narrow_enc.encoding, "UTF-8")) {
	    initialize_table_8bit_utf8();
	    close_encoding();
	}
#endif
    }

    /*
     * Even if we do not have iconv, we can still convert between the narrow
     * encoding (if it happens to be ISO-8859-1) and UTF-8.
     */
    if (vl_is_latin1_encoding(vl_narrow_enc.encoding)) {
	fixup = N_chars;
    } else {
	/* if nothing else, still accept POSIX characters */
	fixup = 128;
    }
    for (n = 0, latin1_codes = 128, fixed_up = 0; n < fixup; ++n) {
	if (table_8bit_utf8[n].text == 0) {
	    char temp[10];
	    int len = vl_conv_to_utf8((UCHAR *) temp, (UINT) n, sizeof(temp));

	    temp[len] = EOS;
	    table_8bit_utf8[n].code = (UINT) n;
	    FreeIfNeeded(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = strmalloc(temp);
	    if (len)
		++fixed_up;
	}
    }
    TRACE(("fixed-up %d of %d 8bit/utf8 mappings\n", fixed_up, fixup));

    /*
     * Build reverse-index.
     */
    TRACE2(("build rindex_8bit\n"));
    for (n = 0; n < N_chars; ++n) {
	int code = (int) table_8bit_utf8[n].code;

	if (n == 0 || code > 0) {
	    rindex_8bit[n].code = (UINT) code;
	    rindex_8bit[n].rinx = n;
	} else {
	    table_8bit_utf8[n].code = (UINT) - 1;
	    rindex_8bit[n].code = (UINT) - 1;
	    rindex_8bit[n].rinx = -1;
	}

	TRACE2(("code %d (0x%04X) is \\u%04X:%s\n", n, n,
		table_8bit_utf8[n].code,
		NonNull(table_8bit_utf8[n].text)));
    }
    qsort(rindex_8bit, N_chars, sizeof(RINDEX_8BIT), cmp_rindex);

    /*
     * As long as the narrow/wide encodings match, we can treat those as
     * Latin-1.
     */
    for (n = 0, latin1_codes = 0; n < N_chars; ++n) {
	int check;
	if (vl_ucs_to_8bit(&check, n)) {
	    latin1_codes = n + 1;
	} else {
	    break;
	}
    }
    TRACE(("assume %d Latin1 codes\n", latin1_codes));

    returnVoid();
}

int
vl_is_8bit_encoding(const char *value)
{
    int rc = vl_is_latin1_encoding(value);
    if (!rc) {
	rc = (isEmpty(value)
	      || strstr(value, "ASCII") != 0
	      || strstr(value, "ANSI") != 0
	      || strncmp(value, "KOI8-R", 6) == 0);
    }
    return rc;
}

int
vl_is_latin1_encoding(const char *value)
{
    int rc = FALSE;

    if (!isEmpty(value)) {
#ifdef WIN32
	char *suffix = strchr(value, '.');
	if (suffix != 0
	    && !strcmp(suffix, ".1252")) {
	    rc = TRUE;
	} else
#endif
	    if (strncmp(value, "ISO-8859", 8) == 0
		|| strncmp(value, "ISO 8859", 8) == 0
		|| strncmp(value, "ISO_8859", 8) == 0
		|| strncmp(value, "ISO8859", 7) == 0
		|| strncmp(value, "8859", 4) == 0) {
	    rc = TRUE;
	}
    }
    return rc;
}

int
vl_is_utf8_encoding(const char *value)
{
    int rc = FALSE;

    if (!isEmpty(value)) {
	char *suffix = strchr(value, '.');
	if (suffix != 0) {
#ifdef WIN32
	    if (!strcmp(suffix, ".65000")) {
		rc = TRUE;
	    } else
#endif
		if (strchr(value + 1, '.') == 0
		    && vl_is_utf8_encoding(value + 1)) {
		rc = TRUE;
	    }
	} else if (!strcmp(value, "UTF8")
		   || !strcmp(value, "UTF-8")
		   || !strcmp(value, "utf8")
		   || !strcmp(value, "utf-8")
		   || !strcmp(value, "646")) {
	    rc = TRUE;
	}
    }
    return rc;
}

/*
 * Check if the given Unicode value can be mapped to an "8bit" value.
 */
int
vl_mb_is_8bit(int code)
{
    int result = 0;

    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);

    if (p != 0)
	result = ((int) p->code >= 0 && p->code < 256);

    return result;
}

/*
 * Returns a string representing the current locale.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_locale(char **target)
{
    char *result = setlocale(LC_CTYPE, 0);

    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0)
	    result = strmalloc(result);
	*target = result;
    }
    return result;
}

/*
 * Returns a string representing the character encoding.
 * If the target is nonnull, allocate a copy of it.
 */
char *
vl_get_encoding(char **target, const char *locale)
{
    static char iso_latin1[] = "ISO-8859-1";
    static char utf_eight[] = "UTF-8";

    char *result = 0;
    char *actual = setlocale(LC_CTYPE, locale);

    TRACE((T_CALLED "vl_get_encoding(%s)\n", NONNULL(locale)));
    if (isEmpty(actual)) {	/* nonempty means legal locale */
	char *mylocale;

	/*
	 * If it failed, this may be because we asked for a narrow locale
	 * that corresponds to the given (wide) locale.
	 */
	beginDisplay();
	if (vl_is_utf8_encoding(locale)) {
	    result = utf_eight;
	} else if (!isEmpty(locale) && (mylocale = strmalloc(locale)) != 0) {
	    regexp *exp;

	    exp = regcomp(tb_values(latin1_expr),
			  (size_t) tb_length0(latin1_expr), TRUE);
	    if (exp != 0) {
		if (nregexec(exp, mylocale, (char *) 0, 0, -1)) {
		    TRACE(("... found match in $latin1-expr\n"));
		    result = iso_latin1;
		}
		free(exp);
	    }
	    free(mylocale);
	}
	endofDisplay();
    } else {
#ifdef HAVE_LANGINFO_CODESET
	result = nl_langinfo(CODESET);
#else
	if (vl_is_utf8_encoding(locale)) {
	    result = utf_eight;
	} else if (isEmpty(locale)
		   || !strcmp(locale, "C")
		   || !strcmp(locale, "POSIX")) {
	    result = "ASCII";
	} else {
	    result = iso_latin1;
	}
#endif
    }
    if (target != 0) {
	FreeIfNeeded(*target);
	if (result != 0)
	    result = strmalloc(result);
	*target = result;
    }
    returnString(result);
}

/*
 * Use the lookup table created in vl_init_8bit() to convert an "8bit"
 * value to the corresponding UTF-8 string.  If the current locale
 * encoding is ISO-8859-1 (the default), this is a 1-1 mapping.
 */
const char *
vl_mb_to_utf8(int code)
{
    const char *result = 0;
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    if (p != 0 && p->rinx >= 0)
	result = table_8bit_utf8[p->rinx].text;

    return result;
}

/*
 * Use the lookup table created in vl_init_8bit() to convert an "8bit"
 * value to the corresponding Unicode value.  If the current locale
 * encoding is ISO-8859-1 (the default), this is a 1-1 mapping.
 */
int
vl_8bit_to_ucs(int *result, int code)
{
    int status = FALSE;

    if (code >= 0 && code < 256 && (int) table_8bit_utf8[code].code >= 0) {
	*result = (int) table_8bit_utf8[code].code;
	status = TRUE;
    }
    return status;
}

/*
 * Use the lookup table to find if a Unicode value exists in the 8bit/utf-8
 * mapping, and if so, set the 8bit result, returning true.
 */
int
vl_ucs_to_8bit(int *result, int code)
{
    int status = FALSE;
    RINDEX_8BIT *p;
    RINDEX_8BIT key;

    key.code = (UINT) code;
    p = (RINDEX_8BIT *) bsearch(&key,
				rindex_8bit,
				N_chars,
				sizeof(RINDEX_8BIT),
				cmp_rindex);
    if (p != 0 && p->rinx >= 0) {
	*result = p->rinx;
	status = TRUE;
    }
    return status;
}

#if USE_MBTERM
#if OPT_ICONV_FUNCS
/*
 * Decode a buffer as UTF-8, returning the character value if successful.
 * If unsuccessful, return -1.
 */
static int
decode_utf8(char *input, int used)
{
    UINT check = 0;
    int rc = 0;
    int ch;

    /*
     * If iconv gave up - because a character was not representable
     * in the narrow encoding - just convert it from UTF-8.
     *
     * FIXME: perhaps a better solution would be to use iconv for
     * converting from the wide encoding to UTF-32. 
     */
    rc = vl_conv_to_utf32(&check, input, (B_COUNT) used);
    if ((rc == used) && (check != 0) && !isSpecial(check))
	ch = (int) check;
    else
	ch = -1;
    return ch;
}
#endif

static int
vl_mb_getch(void)
{
    int ch;
    char input[80];
#if OPT_ICONV_FUNCS
    int used = 0;

    for (;;) {
	ch = save_getch();
	input[used++] = (char) ch;
	input[used] = 0;

	ch = decode_utf8(input, used);
	if (ch >= 0 || used > 5)
	    break;
    }
#else
    char *ip;
    for (ip = input;;) {
	UINT result;

	if ((ch = save_getch()) < 0)
	    break;
	*ip++ = ch;
	*ip = EOS;
	ch = vl_conv_to_utf32(&result, input, ip - input);
	if (ch == 0) {
	    ch = -1;
	} else if (ch == (ip - input)) {
	    /* ensure result is 8-bits */
	    if (vl_mb_to_utf8(result) != 0)
		ch = result;
	    else
		ch = -1;
	    break;
	}
    }
#endif
    TRACE(("vl_mb_getch:%#x\n", ch));
    return ch;
}

static OUTC_DCL
vl_mb_putch(int c)
{
    if (c > 0) {
	int rc;
	UCHAR temp[10];
	const char *s = vl_mb_to_utf8(c);

	/*
	 * If we got no result, then it was not in the cached mapping to 8bit.
	 * But we can still convert it to UTF-8.
	 */
	if (s == 0) {
	    rc = vl_conv_to_utf8(temp, (UINT) c, sizeof(temp));
	    if (rc > 0) {
		temp[rc] = EOS;
		s = (const char *) temp;
	    }
	}
	if (s != 0) {
	    while (*s != 0) {
		save_putch(*s++);
	    }
	}
    }
    OUTC_RET 0;
}

/*
 * If the display driver (termcap/terminfo/curses) is capable of
 * reading/writing UTF-8 (or other iconv-supported multibyte sequences),
 * save/intercept the put/get pointers from 'term'.
 */
void
vl_open_mbterm(void)
{
    if (okCTYPE2(vl_wide_enc)) {
	TRACE(("vl_open_mbterm\n"));
	save_putch = term.putch;
	save_getch = term.getch;

	term.putch = vl_mb_putch;
	term.getch = vl_mb_getch;

	term.set_enc(enc_UTF8);
    }
}

void
vl_close_mbterm(void)
{
    if (okCTYPE2(vl_wide_enc)) {
	if (save_putch && save_getch) {
	    TRACE(("vl_close_mbterm\n"));
	    term.putch = save_putch;
	    term.getch = save_getch;

	    term.set_enc(enc_POSIX);

	    save_putch = 0;
	    save_getch = 0;
	}
    }
}
#endif /* USE_MBTERM */

#if NO_LEAKS
void
eightbit_leaks(void)
{
    int n;

#if OPT_ICONV_FUNCS
    close_encoding();
#endif

    for (n = 0; n < N_chars; ++n) {
	if (table_8bit_utf8[n].text != 0) {
	    free(table_8bit_utf8[n].text);
	    table_8bit_utf8[n].text = 0;
	}
    }
    FreeIfNeeded(vl_wide_enc.locale);
    FreeIfNeeded(vl_narrow_enc.locale);
}
#endif
