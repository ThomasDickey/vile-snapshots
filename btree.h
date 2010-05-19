/*
 * $Id: btree.h,v 1.6 2010/05/18 22:59:38 tom Exp $
 *
 * Interface for btree.c
 */
#ifndef BTREE_H
#define BTREE_H 1

#ifndef BI_DATA
#define BI_DATA struct _bi_data
 	BI_DATA {
	char	*bi_key;	/* the key */
	void	*data;		/* ...associated data */
	};
#endif

#define BI_NODE struct _bi_node
	BI_NODE	{
	BI_NODE	*links[2];
	int	 balance;	/* holds 0, -1, +1 */
	BI_DATA  value;
	};

#define BI_TREE struct _bi_tree
	BI_TREE	{
	BI_NODE*(*allocat) (BI_DATA *a);
	void	(*dealloc) (BI_NODE *a);
	void	(*display) (BI_NODE *a, int level);
	void	(*exchang) (BI_NODE *a, BI_NODE *b);
	int	depth;
	int	count;
	BI_NODE	head;		/* root data, on end to ease initialization */
	};

#define	BI_KEY(p)	(p)->value.bi_key
#define	BI_LEFT(p)	(p)->links[0]
#define	BI_RIGHT(p)	(p)->links[1]

#ifdef __cplusplus
extern "C" {
#endif

extern	int	 btree_delete(BI_TREE *tree, const char *data);
extern	int	 btree_freeup(BI_TREE *tree);
extern	BI_DATA *btree_insert(BI_TREE *tree, BI_DATA *data);
extern	const char **btree_parray(BI_TREE *tree, char *name, size_t len);
extern	BI_NODE *btree_pmatch(BI_NODE *n, const int mode, const char *name);
extern	void	 btree_printf(BI_TREE *tree);
extern	BI_DATA *btree_search(BI_TREE *tree, const char *data);

#ifdef __cplusplus
}
#endif

#endif /* BTREE_H */
