/*
lab7 -font '*-r-*-240-*-p-*'  -s 'this is a test'

*/
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <limits.h>         /* /usr/include/limits.h     */


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h */

#define PGM_XDMC
#define _MAIN_

#define dm_error(m,j) fprintf(stderr, "%s\n", m)
#define CE_MALLOC(size) malloc(size)
#define XERRORPOS ;

#include  "cc.h"
#include  "debug.h"


char *malloc(int size);
static int  ce_error_bad_window_flag;

#include <X11/Xresource.h>     /* "/usr/include/X11/Xresource.h" */

#include "cc.c"


void main (argc, argv)
int   argc;
char **argv;
{

Display              *display = NULL;
char                 *name_of_display = NULL;
Atom                  actual_type;
int                   actual_format;
int                   keydef_property_size;
unsigned long         nitems;
unsigned long         bytes_after;
char                 *buff = NULL;
char                 *p;
Atom                  ce_cclist_atom;

if ((display = XOpenDisplay(name_of_display)) == NULL)
   {
      perror("XOpenDisplay");
      fprintf(stdout,
              "%s: cannot connect to X server %s\n",
              argv[0],
              XDisplayName(name_of_display)
              );
      exit (1);
   }


ce_cclist_atom  = XInternAtom(display, CE_CCLIST_ATOM_NAME, False);

XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   ce_cclist_atom,
                   0, /* offset zero from start of property */
                   INT_MAX / 4,
                   False, /* do not delete the property */
                   AnyPropertyType,
                   &actual_type,
                   &actual_format,
                   &nitems,
                   &bytes_after,
                   (unsigned char **)&buff);

keydef_property_size = nitems * (actual_format / 8);

if (actual_type != None)
   p = XGetAtomName(display, actual_type);
else
   p = "None";
fprintf(stdout, "update_cc_list:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);

#ifdef CE_LITTLE_ENDIAN
to_host_cc_list((CC_LIST *)buff, keydef_property_size);
#endif

if (keydef_property_size > 0)
   dump_cc_list(buff, keydef_property_size);
else
   fprintf(stdout, "No cc list present\n");

exit(0);

}  /* end main   */


