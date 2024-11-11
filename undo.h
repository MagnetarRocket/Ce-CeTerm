#ifndef _UNDO_H_INCLUDED
#define _UNDO_H_INCLUDED

/* static char *undo_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */

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

/**************************************************************************
*
*   This is the Undo header file, to be included by any one wanting to 
*   make undo  calls.
*
****************************************************************************/

#define PL_EVENT  0  /* Put_line                                   */
#define DL_EVENT  1  /* Delete_line                                */
#define RB_EVENT  2  /* Read_block                                 */
#define ES_EVENT  3  /* Enter_(char)_source  (implies a keystroke) */
#define DS_EVENT  4  /* Delete_(char)_source (implies a keystroke) */
#define KS_EVENT  5  /* Key Stroke event                           */
#define SL_EVENT  6  /* Split_line                                 */
#define PC_EVENT  7  /* Put Color                                  */
#define PW_EVENT  8  /* Pad Write event                            */

#define ON  1
#define OFF 0

#ifdef _UNDO_
int undo_semafor = OFF;
#else
extern int undo_semafor;
#endif

void un_do(DATA_TOKEN *token, int *line_out);
                      
void re_do(DATA_TOKEN *token, int *line_out);

void event_do(DATA_TOKEN *token,  
              int event,        /* type of database mnodifying event {PL, DL, ...}  */
              int line,         /* line it occured on */
              int column,       /* column it occured on */
              int paste_type,   /* if paste, was it OVERWRITE or INSERT */
              char *data);      /* data being deleted ... */

void undo_init(DATA_TOKEN *token);    /* used by memdata  */

int kill_event_dlist(DATA_TOKEN *token);

#endif

