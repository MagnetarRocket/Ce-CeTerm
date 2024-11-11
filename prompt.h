#ifndef _PROMPT_INCLUDED
#define _PROMPT_INCLUDED

/*  "%Z% %M% %I% - %G% %U% " */


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
*  Routines in prompt.c
*     dm_prompt           - Put a prompt request on the stack
*     prompt_in_progress  - Query if a prompt is requested      
*     complete_prompt     - Fill in the response to a prompt
*     process_prompt      - complete the prompt process and return the command
*     prompt_prescan      - Prescan a command line in a keydef for prompts.
*     clear_prompt_stack  - Remove all entries from the prompt stack.
*     prompt_target_merge - Merge target strings in key defs with multiple prompts.
*
***************************************************************/

#include "dmc.h"
#include "xutil.h"
#include "mvcursor.h"


int    dm_prompt(DMC             *dmc,                      /* input  */
                 DISPLAY_DESCR   *dspl_descr);              /* input  */


int    prompt_in_progress(DISPLAY_DESCR   *dspl_descr);

int    complete_prompt(DISPLAY_DESCR   *dspl_descr,     /* input  */
                       char            *response,       /* input  */
                       MARK_REGION     *prompt_mark);   /* output */

DMC   *process_prompt(DISPLAY_DESCR   *dspl_descr);

DMC  *prompt_prescan(char   *cmd_in,
                     int     temp_flag,
                     char    escape_char);

void   clear_prompt_stack(DISPLAY_DESCR   *dspl_descr);

void   prompt_target_merge(DMC             *dmc);       /* input  */


#endif

