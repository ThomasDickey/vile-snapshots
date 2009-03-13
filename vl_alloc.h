/*
 * $Header: /users/source/archives/vile.vcs/RCS/vl_alloc.h,v 1.1 2005/11/18 01:27:00 tom Exp $
 *
 * Copyright 2005, Thomas E. Dickey and Paul G. Fox
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, distribute with modifications, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef VL_ALLOC_H_incl
#define VL_ALLOC_H_incl 1

/* structure-allocate, for appeasing strict compilers */
#define	castalloc(cast,nbytes)		(cast *)malloc(nbytes)
#define	castrealloc(cast,ptr,nbytes)	(cast *)realloc((ptr),(nbytes))
#define	typecalloc(cast)		(cast *)calloc(sizeof(cast),1)
#define	typecallocn(cast,ntypes)	(cast *)calloc(sizeof(cast),ntypes)
#define	typealloc(cast)			(cast *)malloc(sizeof(cast))
#define	typeallocn(cast,ntypes)		(cast *)malloc((ntypes)*sizeof(cast))
#define	typereallocn(cast,ptr,ntypes)	(cast *)realloc((ptr),\
							(ntypes)*sizeof(cast))
#define	typeallocplus(cast,extra)	(cast *)calloc((extra)+sizeof(cast),1)

#define	FreeAndNull(p)	if ((p) != 0)	{ free(p); p = 0; }
#define	FreeIfNeeded(p)	if ((p) != 0)	free(p)

#endif /* VL_ALLOC_H_incl */
