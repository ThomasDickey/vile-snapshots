/*
 * $Id: api.h,v 1.16 2020/01/17 23:11:59 tom Exp $
 */

/*
 * The VileBuf structure is used by an extension language (such
 * as Perl) interface to provide an interface to BUFFER.  In this
 * structure, we have a pointer to a BUFFER as well as other fields.
 *
 * In particular, the fwp (fake window pointer) is needed because
 * most of vile's editing operations will not work right unless there
 * is also an associated window.  We don't necessarily want to change
 * the user visible windows, so we create a fake window (and then
 * later destroy it) for the express purpose of fooling the rest of
 * vile into thinking that it's working on a real window.  Care
 * must be taken not to enter any display code, however, since these
 * fake windows use negative numbers to denote the top line of the
 * screen.
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
					   be propagated */
	B_COUNT	    ndel;		/* number of characters to delete upon
					   setup; related to the inplace_edit
					   stuff */
#if OPT_PERL
	void      * perl_handle;	/* perl visible handle to this
					   data structure */
#endif
} VileBuf;

/*
 * A VileWin is a good deal simpler.  It is simply a Window *.  But
 * to perl (and other extension languages as well), the window is
 * represented by its id.  When the actual window is needed, we use
 * id2win (see window.c) to convert the id to the window.
 */
typedef WINDOW *VileWin;

extern	int	api_aline(VileBuf *, int, char *, int);
extern	int	api_edit(char *fname, VileBuf **retspp);
extern	int	api_dline(VileBuf *, int);
extern	int	api_gline(VileBuf *, int, char **, int *);
extern	int	api_sline(VileBuf *, int, char *, int);
extern	int	api_iline(VileBuf *, int, char *, int);
extern	int	api_lline(VileBuf *, int *);
extern	int	api_swscreen(VileBuf *, VileBuf *);
extern	VileBuf *api_fscreen(char *);
extern	VileBuf *api_bp2vbp(BUFFER *bp);
extern	void	api_command_cleanup(void);
extern	int	api_dotinsert(VileBuf *sp, char *text, int len);
extern	int	api_dotgline(VileBuf *, char **, B_COUNT *, int *);
extern	int	api_gotoline(VileBuf *sp, int lno);
extern	void	api_setup_fake_win(VileBuf *sp, int do_delete);
extern	int	api_delregion(VileBuf *vbp);
extern	int	api_motion(VileBuf *vbp, char *mstr);
extern	void	api_update(void);

extern	WINDOW *curwp_visible;

#define vbp2bp(sp) (((VileBuf *)(sp))->bp)
#define bp2vbp(bp) ((VileBuf *) (bp)->b_api_private)
