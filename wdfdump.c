/*
lab7 -font '*-r-*-240-*-p-*'  -s 'this is a test'

*/
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <limits.h>         /* /usr/include/limits.h     */


#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#include <X11/Xatom.h>      /* /usr/include/X11/Xatom.h */

#define _MAIN_

 /* taken from wdf.c */

/***************************************************************
*  
*  Structure used to describe the CE_WDF_LIST property saved in the
*  server.  These are all strings.
*  
*  WD_WINDOW_COUNT  is the array size of CE_WD_LIST elements.
*
***************************************************************/

#define WD_WINDOW_COUNT 12
#define WD_MAXLEN       50

typedef struct {
   char               wd_number;                      /* one byte int, 1 to WD_WINDOW_COUNT   */
   char               data[WD_MAXLEN];                /* geometry string or color string      */
} CE_WD_LIST;



/***************************************************************
*  
*  Atoms for communicating with other windows
*  CE_WDF_LIST is the property used to contain the geometry strings
*              it is set by dm_wdf and read by wdf_get_geometry.
*  CE_WDF_Q    is the property used by wdf_get_geometry to 
*              determine who gets wich window geometry.
*  CE_WDF_LASTWIN is the property used by to save the last geometry
*
*  CE_WDC_LIST is the property used to contain the colorset strings
*              it is set by dm_wdc and read by wdc_get_colors
*  CE_WDC_Q    is the property used by wdc_get_colors to 
*              determine who gets which window color set.
*  CE_WDC_LASTWIN is the property used by to save the last color set.
*
*  The atom declarations are held in the wdf.h file since they are
*  needed in the main event loop.
*  
***************************************************************/

#define  CE_WDF_LIST_NAME        "CE_WDF_LIST"
#define  CE_WDF_Q                "CE_WDF_Q"
#define  CE_WDF_LASTWIN          "CE_WDF_LASTWIN"

#define  CE_WDC_LIST_NAME        "CE_WDC_LIST"
#define  CE_WDC_Q                "CE_WDC_Q"
#define  CE_WDC_LASTWIN          "CE_WDC_LASTWIN"

Atom  ce_wdf_wq_atom = 0;
Atom  ce_wdf_lastwin_atom = 0;

#define WD_MAXLEN       50

char                  type = '\0';   /* from -c or -f */

int getopt();
extern char *optarg;
extern int optind, opterr;


char *malloc(int size);
static int  ce_error_bad_window_flag;

#include <X11/Xresource.h>     /* "/usr/include/X11/Xresource.h" */

static void dump_wd_list(char   *buff,
                          int     buff_len,
                          int     color_atom);


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
char                 *buff_pos;
char                 *que_end;
int                  *q_pos;
char                 *p;
int                   pid_index;
Atom                  list_atom = 0;
char                  ch;
char                 *type_name;


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


   while ((ch = getopt(argc, argv, "cf")) != EOF)
      switch (ch)
      {
      case 'c' :   
         if (type && (type != 'f'))
            {
               fprintf(stderr, "Specify either -f for default positions (wdf) or -c for default colors (wdc), but not both\n");
               exit(1);
            }
         else
            type = 'c';
         break;

      case 'f' :   
         if (type && (type != 'c'))
            {
               fprintf(stderr, "Specify either -f for default positions (wdf) or -c for default colors (wdc), but not both\n");
               exit(1);
            }
         else
            type = 'f';
         break;


      default  :
         fprintf(stderr, "Specify either -f for default positions (wdf) or -c for default colors (wdc), but not both\n");
         exit (1);
      } /* end of switch on ch */


if (type == '\0')
   {
      fprintf(stderr, "Specify either -f for default positions (wdf) or -c for default colors (wdc), but not both\n");
      exit(1);
   }

type_name = ((type == 'f') ? "WDF" : "WDC");


/***************************************************************
*  
*  First do the WDF / WDC list
*  
***************************************************************/


if (type == 'f')
   list_atom        =  XInternAtom(display, CE_WDF_LIST_NAME, False);
else
   list_atom        =  XInternAtom(display, CE_WDC_LIST_NAME, False);


XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   list_atom,
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
fprintf(stdout, "wdf_dump - %s list:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                type_name, keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);

if (keydef_property_size > 0)
   {
      dump_wd_list(buff, keydef_property_size, (type == 'c'));
      XFree(buff);
   }
else
   fprintf(stdout, "No %s list present\n", type_name);


/***************************************************************
*  
* Next do the last window list
*  
***************************************************************/

if (type == 'f')
   ce_wdf_lastwin_atom  = XInternAtom(display, CE_WDF_LASTWIN, False);
else
   ce_wdf_lastwin_atom  = XInternAtom(display, CE_WDC_LASTWIN, False);

XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   ce_wdf_lastwin_atom,
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
fprintf(stdout, "%s - lastwin:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                type_name, keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);

if (keydef_property_size > 0)
   {
      if (type == 'f')
         fprintf(stderr, "last window geometry: \"%s\"\n", buff);
      else
         fprintf(stderr, "last color set: \"%s\"\n", buff);
      XFree(buff);
   }
else
   fprintf(stdout, "No last %s present\n", ((type == 'f') ? "window geometry" : "color set"));


/***************************************************************
*  
*  Do the pid que
*  
***************************************************************/

if (type == 'f')
   list_atom  = XInternAtom(display, CE_WDF_Q, False);
else
   list_atom  = XInternAtom(display, CE_WDC_Q, False);

XGetWindowProperty(display,
                   RootWindow(display, DefaultScreen(display)),
                   list_atom,
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
que_end = buff + keydef_property_size;

if (actual_type != None)
   p = XGetAtomName(display, actual_type);
else
   p = "None";
fprintf(stdout, "%s - pid que:  Size = %d, actual_type = %d (%s), actual_format = %d, nitems = %d, &bytes_after = %d\n",
                type_name, keydef_property_size, actual_type, p,
                actual_format, nitems, bytes_after);
if (actual_type != None)
   XFree(p);

pid_index = 0;
if (keydef_property_size > 0)
   {
      for (q_pos = (int *)buff; (char *)q_pos < que_end; q_pos++)
      {
         fprintf(stderr, "pid #%2d = %d\n", pid_index, *q_pos);
         pid_index++;
      }
      XFree(buff);
   }
else
   fprintf(stdout, "No pid queue present\n");

exit(0);

}  /* end main   */


/************************************************************************

NAME:      dump_wd_list  -   debugging routine to dump the wdf list structure.


PURPOSE:    This routine dumps the passed wdf list structure

PARAMETERS:

   1.  buff        - pointer to char (INPUT)
                     This is the buffer containing the WDF list.

   2.  buff_len    - int (INPUT)
                     This is the length of buffer buff.

   3.  color_atom  - int (INPUT)
                     This flag is True if we are dumping the color list
                     False for the geometry list.


FUNCTIONS :

   1.   For each WDF_LIST entry dump the geometry


*************************************************************************/

static void dump_wd_list(char   *buff,
                          int     buff_len,
                          int     color_atom)
{
char             *buff_pos;
char             *buff_end = buff + buff_len;
int               i;
CE_WD_LIST       *wd_ent;

fprintf(stderr, "dump_wd_list: PID %d, len = %d\n", getpid(), buff_len);

while ((char *)buff < buff_end)
{
   wd_ent = (CE_WD_LIST *)buff;
   if (color_atom)
      {
         if (wd_ent->data[0] == '\0')
            fprintf(stderr, "Color set %2d : NULL\n", wd_ent->wd_number);
         else
            fprintf(stderr, "Color set %2d : %s\n", wd_ent->wd_number, wd_ent->data);

      }
   else
      {
         if (wd_ent->data[0] == '\0')
            fprintf(stderr, "Geometry %2d : NULL\n", wd_ent->wd_number);
         else
            fprintf(stderr, "Geometry %2d : %s\n", wd_ent->wd_number, wd_ent->data);
      }

   buff = buff + sizeof(CE_WD_LIST);
}

} /* end of dump_wd_list */


