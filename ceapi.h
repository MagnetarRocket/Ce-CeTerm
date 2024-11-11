#ifndef _CEAPI_H_INCLUDED
#define _CEAPI_H_INCLUDED

/* static char *ceapi_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

/***************************************************************
*  
*  ARPUS/Ce text editor and terminal emulator modeled after the
*  Apollo(r) Domain systems.
*  Copyright 1988 - 2002 Enabling Technologies Group
*  Copyright 2003 - 2005 Robert Styma Consulting
*  
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*  
*  Original Authors:  Robert Styma and Kevin Plyler
*  Email:  styma@swlink.net
*  
***************************************************************/

/**************************************************************
*
*     ceapi.h - Header definition for Ce application programming interface.
*
*     ceApiInit             -   Initialization routine to find the ce window.
*     ceGetDisplay          -   Get the open display pointer
*     ceXdmc                -   Send a dm command to a window
*     ceGetLines            -   Get one or more lines
*     cePutLines            -   Insert or replace one or more lines
*     ceDelLines            -   Delete one or more lines
*     ceGetMsg              -   Get the contents of the message window.
*     ceGetStats            -   Get the current ce stats (line count, current position, etc)
*     ceReadPasteBuff       -   Get the current value of a Ce paste buffer
*
*     The following external symbols are defined in libCeapi.a but
*     are not part of the external interface.  Calls to these
*     routines will produced unpredictable results.
*
***************************************************************/

/***************************************************************
*  
*  Stats buffer returned by ce_GetStats.  This buffer is in static
*  storage.  If this structure ever grows it will be by adding
*  new fields to the bottom.  This will insure backward compatibility.
*  
***************************************************************/

typedef struct {
   int             total_lines;            /* total lines in the file being edited           */
   int             current_line;           /* Current line number in the file                */
   short           current_col;            /* Current column number in the file              */
   short           writable;               /* Flag - True means file is currently read/write */

} CeStats;


/***************************************************************
*  
*  Function prototypes
*  
***************************************************************/

extern int  ceApiInit(
#if NeedFunctionPrototypes
                 int   cePid
#endif
);

/* only include ce_GetDisplay if the user has included <X11/Xlib.h>, contains definition of Display */
#ifdef _XLIB_H_
extern Display   *ceGetDisplay(
#if NeedFunctionPrototypes
                 void
#endif
);
#endif

extern int  ceXdmc(
#if NeedFunctionPrototypes
             char    *cmdlist,
             int      synchronous
#endif
);

extern char  *ceGetLines(
#if NeedFunctionPrototypes
             int        lineno,
             int        count
#endif
);

extern int   cePutLines(
#if NeedFunctionPrototypes
             int        lineno,
             int        count,
             int        insert,
             char      *data
#endif
);

extern CeStats  *ceGetStats(
#if NeedFunctionPrototypes
                 void
#endif
);

extern int   ceDelLines(
#if NeedFunctionPrototypes
             int        lineno,
             int        count
#endif
);

extern char  *ceGetMsg(
#if NeedFunctionPrototypes
             char    *cmdlist
#endif
);

extern char  *ceReadPasteBuff(
#if NeedFunctionPrototypes
             char      *name
#endif
);

#endif

