#ifndef _GETXOPTS__INCLUDED
#define _GETXOPTS__INCLUDED

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

/* static char *getxopts_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#include "Xresource.h"
#else
#include "X11/Xutil.h"         /*  /usr/include/X11/Xutil.h       */
#include "X11/Xresource.h"     /*  /usr/include/X11/Xresource.h   */
#endif
#else
#include <X11/Xutil.h>         /*  /usr/include/X11/Xutil.h       */
#include <X11/Xresource.h>     /*  /usr/include/X11/Xresource.h   */
#endif


/**************************************************************
*
*  Routines in getxopts.c
*
*  getxopts
*
*  PARAMETERS:
*  
*     1.  opTable   - array of XrmOptionDescRec (INPUT)
*                     This is the description of the parameters.  It describes
*                     parms and their defaults.  Each entry contains the
*                     argument header, the resource specification name (eg .background)
*                     the type of argument, and a default value.  Note that the
*                     resource specification name must start with a '.'.  The '*'
*                     prefix does not always work.  For an example, see the test
*                     main routine at the end of this file.
*  
*     2.  op_count -  int (INPUT)
*                     This is the count of the number of array elements in opTable.
*  
*     3.  defaults -  pointer to array of pointer to char (INPUT)
*                     These values are loaded into output parm parm_values as defaults.
*                     The referenced array matches element for element with optTable and
*                     with parm_values.
*  
*     4.  argc     -  pointer to int (INPUT / OUTPUT)
*                     This is the argc from the command line.  It is modified by
*                     the X routines to get rid of parms they process.
*  
*     5.  argv     -  pointer to array of char (INPUT / OUTPUT)
*                     This is the argv from the command line.  It is modified by
*                     the X routines to get rid of parms they process.
*  
*     6.  parm_values  -  pointer to array of pointer to char (OUTPUT)
*                     The values are loaded into this array.  If a resource name
*                     appears twice in opTable, both parm_values point to the
*                     same place.  This place, is in malloc'ed storage.  An
*                     example of this is -bg and -background are both parms for
*                     the background color and set resource "background".
*                     This array matches element for element with array opTable
*
*     7.  from_parm  -  pointer to array of char (OUTPUT)
*                     The bytes in this array are used as flags.  The bytes
*                     correspond 1 for 1 with the elements in parm_values.
*                     A non-zero byte in this array indicates that the
*                     corresponding parm value came from the parm list.
*
*     8.  display  -  pointer to pointer to Display (OUTPUT)
*                     This routine has to open the display.  The display pointer
*                     returned by XOpenDisplay is returned in this parm.
*  
*     9.  display_idx -  int (INPUT)
*                     This is the index into the opTable array of the parm for the
*                     display name.  This is needed to extract the display name from
*                     the parm string if it was provided on the parm string.  If
*                     -1 is passed, it is assumed that the display name cannot be
*                     specified on the command line.
*
*    10.  name_idx -  int (INPUT)
*                     This is the index into the opTable array of the parm for the
*                     X -name parameter.  This is needed to extract the name to look up
*                     resources under if it was provided on the parm string.  If
*                     -1 is passed, it is assumed that the base name of argv[0] is the
*                     name to use.
*
*    11.  class    -  pointer to char (INPUT)
*                     If non-null this is the "X Class" class for resource retrieval.
*                     If this parm is NULL or points to a null string, the basename
*                     of argv[0] is used with the first letter capitalized.
*  
*
*  set_wm_class_hints
*
*  PARAMETERS:
*  
*  
*     1.  display       - pointer to Display (INPUT)
*                         This is the display pointer for the current display.
*  
*     2.  window        - Window (INPUT)
*                         This is the window to set the property on.
*  
*     3.  arg0          - pointer to char (INPUT)
*                         This is the command name from argv[0].
*  
*     4. dash_name      - pointer to char (INPUT)
*                         This is the value from the -name parm.  It may be null or
*                         point to a null string.
*  
*     5. class          - pointer to char (INPUT)
*                         This is the class name to be used.  It may be null or point to a
*                         null string.
*  
*  
*
***************************************************************/


void  getxopts(XrmOptionDescRec    opTable[],        /*  input   */
               int                 op_count,         /*  input   */
               char               *defaults[],       /*  input   */
               int                *argc,             /*  input / output   */
               char               *argv[],           /*  input / output   */
               char               *parm_values[],    /*  output  */
               char               *from_parm,        /*  output  */
               Display           **display,          /*  output  */
               int                 display_idx,      /*  input   */
               int                 name_idx,         /*  input   */
               char               *class);           /*  input   */
                                                    

void set_wm_class_hints(Display           *display,
                        Window             window,
                        char              *arg0,
                        char              *dash_name,
                        char              *class);


#endif
