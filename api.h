/*
 * $Header: /users/source/archives/vile.vcs/RCS/api.h,v 1.6 1998/03/24 10:14:15 kev Exp $
 */
typedef struct {
	BUFFER    * bp; 
	WINDOW    * fwp;		/* fake window pointer */ 
	REGION      region;		/* Region to traverse */ 
	REGIONSHAPE regionshape; 
	int         inplace_edit;	/* Delete after get? */ 
	int         dot_inited;		/* Has dot been initialized 
	                                   for api_dotgline? */ 
	int         changed;		/* Were any changes done? */ 
	int         dot_changed;	/* DOT explicitly changed -- 
					   implies that DOT should 
					   be propogated */ 
#if OPT_PERL
	void      * perl_handle;	/* perl visible handle to this 
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
extern	int	api_dotinsert(SCR *sp, char *text, int len); 
extern	int	api_dotgline(SCR *, char **, int *); 
extern	int	api_gotoline(SCR *sp, int lno); 
extern	void	api_setup_fake_win(SCR *sp); 

#define sp2bp(sp) (((SCR *)(sp))->bp)
#define bp2sp(bp) ((SCR *) (bp)->b_api_private)
