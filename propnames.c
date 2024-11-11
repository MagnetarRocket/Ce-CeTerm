#include <stdio.h>
#include "dmc.h"
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "hexdump.h"

int  prop_xerror(Display      *display,
                 XErrorEvent  *err_event);

static void reverse_endian_long(int   *buff,
                                int    count);

static void reverse_endian_short(short int   *buff,
                                 int          count);

int getopt();
extern char *optarg;
extern int optind, opterr; 

int  ran_out_of_atoms = False;

int  main(int argc,
          char *argv[])
{
int              ch;
int              i;
char            *p;
char            *prop_name;
Display         *display;
Atom             actual_type;
int              actual_format;
unsigned long    nitems;
unsigned long    bytes_after;
char            *buff = NULL;
char             display_name[128];
int              print_name = 0;
int              print_value = 0;
int              create_window = 0;
int              property_size = 0;
char             window_string[64];
Window           prop_window = None;
char             junk[128];
/* junk variables needed in call to XQueryPointer */
Window           root_window;
int              root_x;
int              root_y;
int              target_x;
int              target_y;
unsigned int     mask_return;
char             kill_prop[256];
 
display_name[0] = '\0';
window_string[0] = '\0';
kill_prop[0] = '\0';

 while ((ch = getopt(argc, argv, "d:k:npwW:")) != EOF)
    switch (ch)
    {
    case 'd':
       strncpy(display_name, optarg, sizeof(display_name)); 
       if (strchr(display_name, ':') == NULL)
          strcat(display_name, ":0.0");
       break;

    case 'k':
       strncpy(kill_prop, optarg, sizeof(kill_prop)); 
       break;

    case 'n':
       print_name++;
       break;

    case 'p':
       print_value++;
       print_name++;
       break;

    case 'w':
       create_window++;
       print_name++;
       break;

    case 'W':
       strncpy(window_string, optarg, sizeof(window_string)); 
       break;

    default  :                                                     
       fprintf(stderr, "Usage:  propnames [-d<display>] [-n] [-p] [-w] [-W <hex_window>]\n");
       fprintf(stderr, "        -n  -  print the type format and size of the property\n");
       fprintf(stderr, "        -p  -  print the value of the property with a hex dump, implies -n\n");
       fprintf(stderr, "        -w  -  create a window to use, defaults to using the root window, implies -p\n");
       fprintf(stderr, "        -W  -  Get the properties from the window the the passed hex id, implies -p\n");
       exit(1);

    }   /* end of switch on ch */


if (!(display = XOpenDisplay(display_name)))
   {
      (void) fprintf(stderr, "%s: Can't open display '%s'\n",
                     argv[0], XDisplayName(display_name));
      exit(1);
   }

XSetErrorHandler(prop_xerror);

if (window_string[0])
   if (strcmp(window_string, ".") == 0)
      {
         XQueryPointer(display,
                       RootWindow(display, DefaultScreen(display)),
                       &root_window, &prop_window,
                       &root_x, &root_y, &target_x, &target_y, &mask_return);
         print_name++;
         print_value++;
      }
   else
      {
         p = window_string;
         if ((*p == '0') && ((*(p+1) | 0x20) == 'x')) /* look for 0x or 0X and skip */
            p += 2;
         i = sscanf(p, "%x%s", &prop_window, junk);
         if (i == 1)
            {
               print_name++;
               print_value++;
            }
         else
            {
               fprintf(stderr, "Invalid hex window id for -W option %s\n", window_string);
               exit(1);
            }
        }



if (create_window)
   {
      prop_window = XCreateSimpleWindow(display, 
                                        RootWindow(display, DefaultScreen(display)),
                                        0, 0, 100, 100, 1, 1, 0);
      if (prop_window == None)
         {
            fprintf(stderr, "Could not create a window, using root\n");
            prop_window = RootWindow(display, DefaultScreen(display));
         }
      else
         XMapWindow(display, prop_window);
   }
else
   if (prop_window == None)
      prop_window = RootWindow(display, DefaultScreen(display));

if (kill_prop[0])
   {
      Atom             actual_type;
      int              actual_format;
      int              property_size;
      int              i;
      unsigned long    nitems;
      unsigned long    bytes_after;
      char            *buff = NULL;
      Atom             kill_atom;

      kill_atom = XInternAtom(display, kill_prop, True);
      if (kill_atom == None)
         fprintf(stderr, "Atom %s unknown\n", kill_prop);
      else
         XGetWindowProperty(display,
                            prop_window,
                            kill_atom,
                            0, /* offset zero from start of property */
                            1000000, /* a million ints is plenty */
                            True, /* delete the property */
                            XA_STRING,
                            &actual_type,
                            &actual_format,
                            &nitems,
                            &bytes_after,
                            (unsigned char **)&buff);
      fprintf(stdout, "Atom %d, deleted %d bytes\n", kill_atom, nitems * (actual_format / 8));
      XCloseDisplay(display);
      exit(0);
   }

if (print_name)
   fprintf(stderr, "Printing data for window 0x%X\n", prop_window);

fprintf(stdout, "PROPERTIES for WINDOW 0x%X\n", prop_window);

for (i = 1; i < 512 && !ran_out_of_atoms; i++)
{
   prop_name = XGetAtomName(display, (Atom)i);
   if ((prop_name != NULL) && !ran_out_of_atoms)
      {
         fprintf(stdout, "atom %3d is %s", i, prop_name);
         if (print_name)
            {
               XGetWindowProperty(display,
                                  prop_window,
                                  (Atom)i,
                                  0, /* offset zero from start of property */
                                  INT_MAX / 4,
                                  False, /* do not delete the property */
                                  AnyPropertyType,
                                  &actual_type,
                                  &actual_format,
                                  &nitems,
                                  &bytes_after,
                                  (unsigned char **)&buff);

               property_size = nitems * (actual_format / 8);
               if (actual_type != None)
                  {
                     p = XGetAtomName(display, actual_type);
                     fprintf(stdout, "\n    Actual_type = %d (%s), actual_format = %d, len = %d\n",
                                     actual_type, p, actual_format, property_size);
                     XFree(p);
                     if (print_value)
                        {
#ifdef CE_LITTLE_ENDIAN
                           if (actual_format == 32)
                              reverse_endian_long((int *)buff, nitems);
                           if (actual_format == 16)
                              reverse_endian_short((short int *)buff, nitems);
#endif
                           hexdump(stdout, prop_name, buff, property_size, 0);
                        }
                  }
               else
                  fprintf(stdout, "\n");
            }
         else
            fprintf(stdout, "\n");
         XFree(prop_name);
      }
   else
      if (ran_out_of_atoms)
         fprintf(stdout, "atom %3d : Not found, End of list\n", i);
      else
         fprintf(stdout, "atom %3d Not found\n", i);
}

XCloseDisplay(display);

} /* end of main */


static int _XPrintDefaultError (Display     *dpy,
                                XErrorEvent *event,
                                FILE        *fp);


/************************************************************************

NAME:      prop_xerror

PURPOSE:    This routine is identified by a call to XSetErrorHandler
            to be the error handler for protocol requests.
            It currently just calls the message printer and continues.

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the current display parm needed in X calls.

   2.  err_event  - pointer to XErrorEvent (INPUT)
                    This is the Xevent structure for the error event 
                    which caused this routine to get called.

FUNCTIONS :

   1.   Set the badwindow flag if this is a badwindow error.

   2.   Print the message relating to the text so long as the message has not been suppressed.
 
   3.   Save the errno from the X error globally.



*************************************************************************/

int  prop_xerror(Display      *display,
                 XErrorEvent  *err_event)
{
FILE  *stream;

if (err_event->error_code == BadAtom)
   ran_out_of_atoms = True;
else
   _XPrintDefaultError(display, err_event, stderr);

return(0);

} /* end of prop_xerror */





/*
 *	NAME
 *		_XPrintDefaultError
 *
 *	SYNOPSIS
 */
/************************************************************************

NAME:      _XPrintDefaultError

PURPOSE:    This routine is formats the data from an X error event and
            outputs the data to the passed file. 

PARAMETERS:

   1.  display    - pointer to Display (INPUT)
                    This is the current display parm needed in X calls.

   2.  err_event  - pointer to XErrorEvent (INPUT)
                    This is the Xevent structure for the error event 
                    which caused this routine to get called.

   3.  fp         - pointer to FILE (INPUT)
                    This is the stream to write to. 

FUNCTIONS :

   1.   Print the message relating to the text.

NOTES:
   This routine paraphased from the Xlib source


*************************************************************************/

static int _XPrintDefaultError (Display     *dpy,
                                XErrorEvent *event,
                                FILE        *fp)
{
char                    buffer[BUFSIZ];
char                    mesg[BUFSIZ];
char                    number[32];
char                   *mtype = "XlibMessage";

XGetErrorText(dpy, event->error_code, buffer, BUFSIZ);
XGetErrorDatabaseText(dpy, mtype, "Ce", "X Error", mesg, BUFSIZ);
fprintf(fp, "%s:  %s\n  ", mesg, buffer);

XGetErrorDatabaseText(dpy, mtype, "MajorCode", "Request Major code %d", mesg, BUFSIZ);
fprintf(fp, mesg, event->request_code);

sprintf(number, "%d", event->request_code);
XGetErrorDatabaseText(dpy, "XRequest", number, "", buffer, BUFSIZ);

fprintf(fp, " (%s)\n  ", buffer);
if (event->request_code >= 128)
   {
      XGetErrorDatabaseText(dpy, mtype, "MinorCode", "Request Minor code %d", mesg, BUFSIZ);
      fprintf(fp, mesg, event->minor_code);
      fputs("\n  ", fp);
   }

if ((event->error_code == BadWindow) ||
    (event->error_code == BadPixmap) ||
    (event->error_code == BadCursor) ||
    (event->error_code == BadFont) ||
    (event->error_code == BadDrawable) ||
    (event->error_code == BadColor) ||
    (event->error_code == BadGC) ||
    (event->error_code == BadIDChoice) ||
    (event->error_code == BadValue) ||
    (event->error_code == BadAtom))
   {
      if (event->error_code == BadValue)
         XGetErrorDatabaseText(dpy, mtype, "Value", "Value 0x%X", mesg, BUFSIZ);
      else
         if (event->error_code == BadAtom)
            XGetErrorDatabaseText(dpy, mtype, "AtomID", "AtomID 0x%X", mesg, BUFSIZ);
         else
            XGetErrorDatabaseText(dpy, mtype, "ResourceID", "ResourceID 0x%X", mesg, BUFSIZ);
      fprintf(fp, mesg, event->resourceid);
      fputs("\n  ", fp);
   }

XGetErrorDatabaseText(dpy, mtype, "ErrorSerial", "Error Serial #%d", mesg, BUFSIZ);
fprintf(fp, mesg, event->serial);
fputs("\n  ", fp);

XGetErrorDatabaseText(dpy, mtype, "CurrentSerial", "Current Serial #%d", mesg, BUFSIZ);
fprintf(fp, mesg, NextRequest(dpy)-1);
fputs("\n", fp);

if (event->error_code == BadImplementation)
   return 0;
else
   return 1;

} /* end of _XPrintDefaultError */

static void reverse_endian_long(int   *buff,
                                int    count)
{
int              *num;
int               i;


num = buff;

for (i = 0; i < count; i++)
{
   *num = ntohl(*num);
   num++;
}

} /* end of reverse_endian_long */


static void reverse_endian_short(short int   *buff,
                                 int          count)
{
short int        *num;
int               i;


num = buff;

for (i = 0; i < count; i++)
{
   *num = ntohs(*num);
   num++;
}

} /* end of reverse_endian_short */


#include "hexdump.c"
