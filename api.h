/*
 * $Header: /users/source/archives/vile.vcs/RCS/api.h,v 1.3 1997/12/03 23:35:30 tom Exp $
 */
typedef struct {
	BUFFER * bp;
	WINDOW * fwp;			/* fake window pointer */
	int      changed;		/* Were any changes done? */
#if OPT_PERL
	void   * perl_handle;		/* perl visible handle to this
					   data structure */
#endif
} SCR;

extern	int	api_aline(SCR *, int, char *, int);
extern	int	api_edit(SCR *sp, char *fname, SCR **retspp, int newscreen);
extern	int	api_dline(SCR *, int);
extern	int	api_gline(SCR *, int, char **, int *);
extern	int	api_sline(SCR *, int, char *, int);
extern	int	api_iline(SCR *, int, char *, int);
extern	int	api_lline(SCR *, int *);
extern	int	api_swscreen(SCR *, SCR *);
extern	SCR *	api_fscreen(int, char *);
extern	SCR *	api_bp2sp(BUFFER *bp);
extern	void	api_command_cleanup(void);

#define sp2bp(sp) (((SCR *)(sp))->bp)
#define bp2sp(bp) ((SCR *) (bp)->b_api_private)
