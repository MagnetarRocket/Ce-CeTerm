#ifndef _LABEL_INCLUDED
#define _LABEL_INCLUDED

/* static char *label_h_sccsid = "%Z% %M% %I% - %G% %U% "; */
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
*  Routines in label.c
*     dm_lbl                  - Set label at the current location
*     dm_glb                  - Goto a label
*     find_label              - Scan the memdata database for a label
*     dm_dlb                  - Delete labels in an area
*
***************************************************************/


#include "dmc.h"
#include "buffer.h"

 
int   dm_lbl(DMC               *dmc,                /* input   */
             BUFF_DESCR         *cursor_buff,       /* input   */
             int                lineno,             /* input flag  */
             int                just_delete);       /* input flag  */

DMC   *dm_glb(DMC               *dmc,                /* input   */
              BUFF_DESCR        *cursor_buff,        /* input   */
              PAD_DESCR         *main_pad);          /* input   */

int find_label(DATA_TOKEN   *token,               /* input   */
               int           top_line,            /* input   */
               int           bottom_line,         /* input   */
               char         *label,               /* output  */
               int           max_label);          /* input   */


int  dm_dlb(MARK_REGION       *mark1,              /* input   */
            BUFF_DESCR        *cursor_buff,        /* input   */
            int                echo_mode,          /* input   */
            int                lineno);            /* input   */
#endif


