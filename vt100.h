#ifndef _VT100_H_INCLUDED
#define _VT100_H_INCLUDED

/* static char *vt100_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in vt100.c
*     dm_vt100         -   Swap in and out of vt100 mode
*     vt100_parse      -   Parse data returned by the shell for display on the screen
*     vt100_pad2shell  -   Send data to the shell when in vt100 mode.
*     vt100_key_trans  -   Translate keys in vt100 keypad application mode
*     vt100_resize     -   Respond to resize events.
*     free_drawable    -   Delete fancy line data
*     vt100_color_line -   Parse a line with VT100 colorization data
*
***************************************************************/

/***************************************************************
*  
*  Includes needed for types used in prototypes.
*  
***************************************************************/

#include "dmc.h"
#include "buffer.h"
#include "memdata.h"

/*
 *   VT100 tokens
 */

#define VT_BL    007
#define VT_BS    010
#define VT_CN    030
#define VT_CR    015
#define VT_FF    014
#define VT_HT    011
#define VT_LF    012
#define VT_SI    017
#define VT_SO    016
#define VT_SP    040
#define VT_VT    013
#define VT__    0137

#define VT_ESC   033


/***************************************************************
*  
*  Prototypes for routines in cc.c
*  
***************************************************************/

int  dm_vt100(DMC             *dmc,
              DISPLAY_DESCR   *dspl_descr);

void vt100_parse(int            len,
                 char          *line,
                 DISPLAY_DESCR *dspl_descr);

int  vt100_pad2shell(PAD_DESCR    *main_window_cur_descr,
                     char         *line,
                     int           newline);

void  vt100_key_trans(KeySym       keysym,
                      char        *str);


void  vt100_resize(PAD_DESCR    *main_window_cur_descr);

int free_drawable(void   *drawable_data);

int  vt100_color_line(DISPLAY_DESCR   *dspl_descr,
                      char            *line,
                      char            *color_line,
                      int              max_color_line);

#endif

