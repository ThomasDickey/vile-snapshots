================================
Visual Studio - Vile Integration
================================

Copyright (C) 1998 Clark Morgan

VisVile is a Visual Studio add-in that permits Winvile to replace the
default text editor in certain situations.  Complete documentation of this
add-in may be found in the file ../doc/visvile.doc .

VisVile is based upon VisVim, written by Heiko Erhardt (Copyright (C) 1997
Heiko Erhardt).  VisVim is based upon VisEmacs, written by Christopher
Payne (Copyright (C) Christopher Payne 1997).

VisVile is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

VisVile is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

- Clark Morgan, Aug 1998

Change Log
==========
v1.01, released 9/07/98

- if visvile disabled, no other dialog options are editable
- added more document type checking to avoid occasionally opening such
  things as dialog box resource scripts.
- implemented "CWD set from opened document's path" option
- implemented "Write all modified buffers to disk prior to build" option.
  This feature doesn't work as well as it could due to a DevStudio (v5)
  bug.  Refer to ../doc/visvile.doc for further details.
- implemented "Build log -> errbuf if build reports errs/warnings"
  option.

v1.0, released 8/98
