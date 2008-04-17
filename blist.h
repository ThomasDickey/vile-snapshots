/*
 * $Id: blist.h,v 1.4 2007/08/13 22:59:08 tom Exp $
 *
 * Interface for blist.c
 */

#ifndef BLIST_H
#define BLIST_H 1

typedef struct BLIST {
	const void *theList;
	unsigned itemSize;
	int itemCount;
} BLIST;

#define init_blist(list) { list, sizeof(list[0]), -1 }

#ifdef __cplusplus
extern "C" {
#endif

extern int blist_count (BLIST *data);
extern int blist_match (BLIST *data, const char *name);
extern int blist_pmatch (BLIST *data, const char *name, int len);
extern void blist_reset (BLIST *data, const void *list);
extern void blist_summary (void);

#ifdef __cplusplus
}
#endif

#endif /* BLIST_H */
