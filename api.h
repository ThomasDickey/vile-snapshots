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
extern	int	api_dline(SCR *, int);
extern	int	api_gline(SCR *, int, char **, int *);
extern	int	api_sline(SCR *, int, char *, int);
extern	int	api_iline(SCR *, int, char *, int);
extern	int	api_lline(SCR *, int *);
