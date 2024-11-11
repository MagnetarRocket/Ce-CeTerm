#ifndef _EXECUTE__INCLUDED
#define _EXECUTE__INCLUDED

/* "%Z% %M% %I% - %G% %U% " */


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
*  Routines in execute.c
*     dm_ce              - Execute the ce, cv, cp and ceterm commands
*     dm_cpo             - Execute the cpo and cps commands
*     dm_bang            - ! command
*     dm_bang_lite       - The ! command used from .Cekeys
*     spoon              - Fork the edit session to background
*     dm_env             - DM env command, do a putenv
*     reap_child_process - Clean up terminated child processes
*     is_executable      - Test if a path is executable
*
***************************************************************/

#include "dmc.h"          /* needed for DMC        */
#include "buffer.h"       /* needed for BUFF_DESCR */

                                            
int   dm_ce(DMC             *dmc,                    /* input  */
            DISPLAY_DESCR   *dspl_descr);            /* input / output  in child process */

void dm_cpo(DMC               *dmc,
            DISPLAY_DESCR     *dspl_descr);

DMC  *dm_bang(DMC               *dmc,                /* input   */
              DISPLAY_DESCR     *dspl_descr);         /* input   */

DMC *dm_bang_lite(DMC               *dmc,                /* input   */
                  DISPLAY_DESCR     *dspl_descr);        /* input   */

int   spoon(void);   /* input  */

void  dm_env(char  *assignment);

void reap_child_process();

#ifdef WIN32
int is_executable(char   *path);

#endif

#endif

