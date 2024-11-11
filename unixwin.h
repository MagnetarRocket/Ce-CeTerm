#ifndef _UNIXWIN_INCLUDED
#define _UNIXWIN_INCLUDED

/* static char *unixwin_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in unixwin.c
*         get_unix_window       - get the unix command window.
*         resize_unix_window    - handle a resize of the main window to the client.
*         unix_subarea          - Build the subarea drawing description for the unix command window.
*         need_unixwin_resize   - Check if we have to resize the Unix command window.
*         set_unix_prompt       - Prompt for input at the DM input window.
*         scroll_unix_prompt    - scroll the dm input window prompt.
*         draw_unix_prompt      - Draw the unix prompt in the unix command window.
*         get_unix_prompt       - Return the unix prompt address, set by set unix prompt.
*         get_drawn_unix_prompt - Return the unix to draw, set by set unix prompt or dm_sp
*         dm_sp                 - set prompt command
*         setprompt_mode        - Are we in setprompt mode
*         dm_prefix             - ceterm prefix command for passed DM commands
*         ceterm_prefix_cmds    - ceterm check for dm commands passed from shell.
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*
*  This is the maximum number of lines the unix command line
*  can grow to.
*
***************************************************************/

#define   MAX_UNIX_LINES                5

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/


void  get_unix_window(DISPLAY_DESCR    *dspl_descr);          /* input / output  */


void  resize_unix_window(DISPLAY_DESCR    *dspl_descr,
                         int               window_lines);        /* input   */

void  unix_subarea(DRAWABLE_DESCR   *unix_window,           /* input  */
                   DRAWABLE_DESCR   *unixcmd_pix_subarea);  /* output  */

int need_unixwin_resize(PAD_DESCR   *unixcmd_cur_descr);

void  set_unix_prompt(DISPLAY_DESCR    *dspl_descr,  /* input  */
                      char             *text);       /* input  */

void  dm_sp(DISPLAY_DESCR    *dspl_descr,  /* input  */
            char             *text);       /* input  */

int scroll_unix_prompt(int     scroll_chars);

void draw_unix_prompt(Display        *display,
                      Pixmap          x_pixmap,
                      DRAWABLE_DESCR *unixcmd_pix_subarea);

char  *get_unix_prompt(void);

char  *get_drawn_unix_prompt(void);

int setprompt_mode(void);

void  dm_prefix(char             *text);        /* input  */


int  ceterm_prefix_cmds(DISPLAY_DESCR    *dspl_descr,  /* input  */
                        char             *line);       /* input  */

#endif

