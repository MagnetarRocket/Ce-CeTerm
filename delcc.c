static char *sccsid = "%Z% %M% %I% - %G% %U% ";
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

/*
lab7 -font '*-r-*-240-*-p-*'  -s 'this is a test'

*/
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <limits.h>         /* /usr/include/limits.h     */


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h */

#define  CE_CCLIST_ATOM_NAME     "CE_CCLIST"


void main (argc, argv)
int   argc;
char **argv;
{

Display              *display;
char                 *name_of_display = NULL;
int                   screen;
int                   i;

Atom                  ce_cclist_atom;
Atom             actual_type;
int              actual_format;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff;


char                  key_name[16];

/****************************************************************
*
*   Open the display so we can start getting information about it.
*
****************************************************************/

 if ((display = XOpenDisplay(name_of_display)) == NULL)
    {
       perror("XOpenDisplay");
       fprintf(stderr,
               "%s: cannot connect to X server %s\n",
               argv[0],
               XDisplayName(name_of_display)
               );
       exit (1);
    }
screen = DefaultScreen(display);

/***************************************************************
*  
*  See if the property exists,  If not,  bail out.  Note the
*  True parm to XInternAtom means the property must already exist.
*  
***************************************************************/

ce_cclist_atom = XInternAtom(display, CE_CCLIST_ATOM_NAME, True);

if (ce_cclist_atom == None)
   {
      fprintf(stderr, "Atom not found\n");
      exit(1);
   }


XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   ce_cclist_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   True, /* do delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

fprintf(stderr, "property gotten, length was %d\n", nitems * (actual_format / 8));

XCloseDisplay(display);


}  /* end main   */



