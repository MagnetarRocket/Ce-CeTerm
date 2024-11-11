#ifndef _UNIXPAD_INCLUDED
#define _UNIXPAD_INCLUDED

/* static char *unixpad_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  These routines do not fall into pad.c which handles shell
*  commumication, nor unixwin.c which is mostly drawing routines.
*
*  Routines in unixpad.c
*         send_to_shell         - send lines to the shell
*         dm_eef                - send end of file to the shell.
*         dm_dq                 - Send a quit signal to a shell
*         wait_for_input        - Wait on the Xserver and shell sockets
*         scroll_some           - scroll shell output onto the window
*         transpad_input        - WIN32 only, process -TRANSPAD or -STDIN input
*
***************************************************************/

#include "buffer.h"

/***************************************************************
*  
*  Prototypes for the functions
*  
***************************************************************/

int   send_to_shell(int           count,
                    PAD_DESCR    *unixcmd_cur_descr);

int  dm_eef(DMC        *dmc,
            BUFF_DESCR *cursor_buff);

int  dm_dq(DMC             *dmc,                    /* input  */
           DISPLAY_DESCR   *dspl_descr);            /* input  */

DISPLAY_DESCR *wait_for_input(DISPLAY_DESCR   *dspl_descr,
                              int               Shell_socket,
                              int              *cmd_fd);

int scroll_some(int              *lines_copied,
                DISPLAY_DESCR    *dspl_descr,
                int               lines_displayed);

#ifdef WIN32
void transpad_input(DISPLAY_DESCR   *dspl_descr);
#endif

#endif

