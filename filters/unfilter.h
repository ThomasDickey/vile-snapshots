/*
 * Common definitions for un-filters.
 *
 * $Id: unfilter.h,v 1.2 2003/05/07 00:38:38 tom Exp $
 */

#ifndef UNFILTER_H
#define UNFILTER_H 1
#include <filters.h>

extern void begin_unfilter(FILE *dst);
extern void markup_unfilter(FILE *dst, int attrib);
extern void write_unfilter(FILE *dst, int ch, int attrib);
extern void end_unfilter(FILE *dst);

#define ATR_COLOR	0x0100
#define ATR_BOLD	0x0200
#define ATR_REVERSE	0x0400
#define ATR_UNDERLINE	0x0800
#define ATR_ITALIC	0x1000

#endif /* UNFILTER_H */
