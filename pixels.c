static char *sccsid = "%Z% %M% %I% - %G% %U% ";

/**************************************************************
*
*  MODULE pixels - Show all pixels (256)
*
*  OVERVIEW
*      This program builds a window with a square for each
*      pixel value.  Free pixel values have an X in the square
*      and a small square in the lower right corner of the square 
*      with whatever is in the color map for that color.
*
*  ARGUMENTS:
*      -s <num>      -   Size of each square in the window
*      -p            -   Print the color information about each pixel
*                        the specified hex window id (use -list to find it)
*      -display <node:0.0> -  open the specified node.
*
***************************************************************/

#include <stdio.h>
#include <string.h>         /* /usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h   /usr/include/sys/errno.h      */
#include <ctype.h>          /* /usr/include/ctype.h      */
#include <stdlib.h>         /* /usr/include/stdlib.h     */
#include <limits.h>         /* /usr/include/limits.h     */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif


#include <X11/X.h>          /* /usr/openwin/include/X11/X.h      /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/openwin/include/X11/Xlib.h   /usr/include/X11/Xlib.h   */
#include <X11/Xatom.h>      /* /usr/openwin/include/X11/Xatom.h  /usr/include/X11/Xatom.h  */
#include <X11/Xutil.h>      /* /usr/openwin/include/X11/Xutil.h  /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h /usr/openwin/include/X11/keysymdef.h */

void draw_squares(int        square_size,
                  Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC         string_gc,
                  GC         inv_gc,
                  int       *free_colors);

void   create_gcs(Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC        *string_gc,
                  GC        *inv_gc);

int  find_free_colors(Display   *display,
                      int       *free_colors);

char  *name_the_color(int r, int g, int b);

char   *shared_color(Display        *display,
                     XColor         *color_parm,
                     int             free_color,
                     unsigned long   index);

static void build_picture(Display    *display,
                          int        *free_colors,
                          XColor     *colors,
                          int         print_rgb);


int getopt();
extern char *optarg;
extern int optind, opterr; 

char             window_name[256] = "Pixel Map";

void main(int argc,
          char *argv[])
{
int              ch;
int              i;
int              square_size = 0;
GC               gc[256];
GC               string_gc;
GC               inv_gc;
XColor           colors[256];
int              free_colors[256];
int              free_colors_count;
int              print_rgb = 0;
int              square_num;
char            *p;
Display         *display;
char             display_name[128];
Window           prop_window = None;
char             junk[128];
XEvent           event_union;      /* The event structure filled in by X as events occur            */
int              unmapped = False;
XTextProperty    windowName;
char            *window_name_p = window_name;
int              getting_cursor_num = False;
int              cursor_num = 0;
int              max_colors;
Cursor           pointer_cursor = None;
Atom             atom_list[2];
Atom             wm_protocols_atom;
Atom             save_yourself_atom;
Atom             delete_window_atom;

 
display_name[0] = '\0';

 while ((ch = getopt(argc, argv, "d:s:p")) != EOF)
    switch (ch)
    {
    case 'd':
       strncpy(display_name, optarg, sizeof(display_name)); 
       if (strchr(display_name, ':') == NULL)
          strcat(display_name, ":0.0");
       break;

    case 's':
       square_size = atoi(optarg);
       break;

    case 'p':
       print_rgb++;
       break;

    default  :                                                     
       fprintf(stderr, "Usage:  pixels [-d<display>] [-a] [-n] [-p] [-s<size>]\n");
       fprintf(stderr, "        -a  -  X out unallocated pixels\n");
       fprintf(stderr, "        -d  -  Specify a different display\n");
       fprintf(stderr, "        -n  -  Put the pixel number in each square\n");
       fprintf(stderr, "        -p  -  Print the color data to stdout (rgb values)\n");
       fprintf(stderr, "        -s  -  Specify initial pixel size for each square\n");
       exit(1);

    }   /* end of switch on ch */

if (square_size == 0)
    square_size = 28;

if (!(display = XOpenDisplay(display_name)))
   {
      (void) fprintf(stderr, "%s: Can't open display '%s'\n",
                     argv[0], XDisplayName(display_name));
      exit(1);
   }

if (print_rgb)
   {
      max_colors = DisplayCells(display, DefaultScreen(display));
      fprintf(stderr, "Colors in default color map = %d\n", max_colors);
   }

prop_window = XCreateSimpleWindow(display, 
                                  RootWindow(display, DefaultScreen(display)),
                                  100, 0, (square_size+1)*16, (square_size+1)*16,
                                  1, BlackPixel(display, DefaultScreen(display)), 0);
if (prop_window == None)
   {
      fprintf(stderr, "Could not create a window\n");
      XCloseDisplay(display);
      exit(1);
   }

/***************************************************************
*  Set up for the save yourself client message and the delete
*  window message.  Delete window shows up when the user uses
*  the window manager pulldown to close the window.
***************************************************************/
save_yourself_atom = XInternAtom(display, "WM_SAVE_YOURSELF", False);
delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
wm_protocols_atom  = XInternAtom(display, "WM_PROTOCOLS", False);

atom_list[0] = save_yourself_atom;
atom_list[1] = delete_window_atom;

XSetWMProtocols(display, prop_window, atom_list, 2);



XSelectInput(display,
             prop_window,
             ExposureMask
             | VisibilityChangeMask
             | StructureNotifyMask
             | KeyPressMask
             | EnterWindowMask
             | LeaveWindowMask
             | ButtonPressMask);


free_colors_count = find_free_colors(display, free_colors);

create_gcs(display, prop_window, gc, colors, &string_gc, &inv_gc);
if (print_rgb)
   fprintf(stdout, "%d free color map entries available (pixels)\n\n", free_colors_count);

for (i = 0; i < 256; i++)
{
   if (print_rgb)
      fprintf(stdout, "Pixel  %3d, r = %5d, g = %5d, b = %5d, flags = 0x%X\n", i, colors[i].red>>8, colors[i].green>>8, colors[i].blue>>8, colors[i].flags);
   if (free_colors[i])
      {
         colors[i].red   = 0;  /* unallocated colors are not drawn */
         colors[i].green = 0;
         colors[i].blue  = 0;
      }
}
fflush(stdout);

XStringListToTextProperty(
      &window_name_p,
      1,
      &windowName);


XSetWMName(display, prop_window, &windowName);


XMapWindow(display, prop_window);

/* CONSTCOND */
while(1)
{
   XNextEvent(display, &event_union);

   /***************************************************************
   *  
   *  Switch on the type of event.  This separates screen things
   *  such as expose (pop), configure (resize and move), from
   *  user input such as keypress and mouse press.
   *  
   ***************************************************************/

   switch (event_union.type)
   {
   /***************************************************************
   *  In the case of an expose event, we need to copy the pixmap
   *  to the screen.  Copy the portion needed.
   ***************************************************************/
   case Expose:
      draw_squares(square_size, display, prop_window, gc, colors, string_gc, inv_gc, free_colors);
      break;


   case MapNotify:
      if (unmapped)
         build_picture(display, free_colors, colors, print_rgb);

#ifdef blah
         {
            free_colors_count = find_free_colors(display, free_colors);
            if (print_rgb)
               fprintf(stdout, "%d free color map entries available (pixels)\n\n", free_colors_count);
            fflush(stdout);
            XQueryColors(display, 
                         DefaultColormap(display, DefaultScreen(display)),
                         colors,
                         256);
            for (i = 0; i < 256; i++)
               if (free_colors[i])
                  {
                     colors[i].red   = 0;  /* unallocated colors are not drawn */
                     colors[i].green = 0;
                     colors[i].blue  = 0;
                  }
         }
#endif
      unmapped = False;
      break;

   case UnmapNotify:
      unmapped = True;
      break;



   /***************************************************************
   *
   *  The window changed size (or location).  Recalculate the
   *  size of the squares.
   *
   ***************************************************************/
   case ConfigureNotify:
      if (event_union.xconfigure.width < event_union.xconfigure.height)
         i = event_union.xconfigure.width;
      else
         i = event_union.xconfigure.height;
      square_size = (i / 16) - 1;
      break;

   /***************************************************************
   *
   *  If we get a MappingNotify event, the keyboard mapping has
   *  changed.  We call XRefreshKeyboardMapping to fix some fields
   *  in the Display structure.  This is one of the "Nobrainers"
   *  that the instructor talked about.
   *
   ***************************************************************/
   case MappingNotify:
      XRefreshKeyboardMapping((XMappingEvent *)&event_union);
      break;

   /***************************************************************
   *
   *  We only look at a few keypresses.  ^c, ^d, and ^q cause
   *  normal termination of the program.
   *  The letter c in a square causes the RGB values for that
   *  square to be written to stdout.
   *  An exclamation mark, followed by digits, followed by a
   *  Return, cause the cursor to be changed to that cursor number.
   *  This is just for playing with cursors.  Use a bad number
   *  and you will abort the program.  The list of cursors is 
   *  in /usr/include/X11/cursorfont.h (/usr/openwin/include/X11/cursorfont.h)
   *
   ***************************************************************/
   case KeyPress:
   {
      KeySym           keysym;
      char             key_val[2];
      int              len;
      int              start_x;
      char             ch;
      XHostAddress    *hosts;
      XHostAddress     new_host;
      int              nhosts;
      Bool             enabled;
      char             host_addr[4];
      XSetWindowAttributes    win_attr;

      len = XLookupString((XKeyEvent *)&event_union,
                          key_val,
                          sizeof(key_val),
                          &keysym,
                          NULL);
      key_val[len] = '\0';
      ch = key_val[0];
      
      switch (ch)
      {
      case '\r': case '\n':
        if (getting_cursor_num)
           {
              if (pointer_cursor != None)
                 XFreeCursor(display, pointer_cursor);
              pointer_cursor = XCreateFontCursor(display, cursor_num);
              win_attr.cursor = pointer_cursor;
              XChangeWindowAttributes(display,
                                      prop_window,
                                      CWCursor,
                                      &win_attr);
           }
        getting_cursor_num = False;
        break;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
         cursor_num = (cursor_num * 10) + (ch & 0x0f);
         break;

      case '!':
        getting_cursor_num = True;
        cursor_num = 0;
        break;

      case 'c':
        square_num = which_square(square_size, event_union.xkey.x, event_union.xkey.y);
        fprintf(stdout, "(%d) RGB = %d, %d, %d\n", square_num, colors[square_num].red>>8,
                        colors[square_num].green>>8,
                        colors[square_num].blue>>8);
        fflush(stdout);
        break;

      case 0x03:
      case 0x04:
      case 0x11:
         /* exit on ^c */
         XCloseDisplay(display);
         exit(0);
         break;

      default:
         break;
      } /* end of switch on character */
      if (keysym == XK_F1)
         {
            build_picture(display, free_colors, colors, print_rgb);
            draw_squares(square_size, display, prop_window, gc, colors, string_gc, inv_gc, free_colors);
         }
   } /* end of keypress group */
      break;


   /***************************************************************
   *
   *  A buttonpress in a square causes the rgb values and the
   *  color name (if we can find it) to be placed in the
   *  window manager title.
   *
   ***************************************************************/
   case ButtonPress:
      square_num = which_square(square_size, event_union.xkey.x, event_union.xkey.y);
      sprintf(window_name, "(%d%s) RGB = %d %d %d%s\n",
                        square_num,
                        shared_color(display, &colors[square_num], free_colors[square_num], square_num), 
                        colors[square_num].red>>8,
                        colors[square_num].green>>8,
                        colors[square_num].blue>>8,
                        name_the_color(colors[square_num].red>>8, colors[square_num].green>>8, colors[square_num].blue>>8));
      XSetStandardProperties(display,
                             prop_window,
                             window_name,
                             window_name,
                             None, /* pixmap for icon */
                             NULL, /* argv */
                             0,    /* argc */
                             NULL);

      break;

   /***************************************************************
   *
   *  If we get a ClientMessage, It may be the SAVE_YOURSELF message
   *  from the window manager.  If it is, do a pad write if we
   *  are in edit mode and the file is dirty.  Then confirm that
   *  we are ready with a zero length append to the XA_WM_COMMMAND property.
   *
   ***************************************************************/
   case ClientMessage:
      if (event_union.xclient.message_type == wm_protocols_atom)
         {
            if (*event_union.xclient.data.l == save_yourself_atom)
               {
                  XSetCommand(display, prop_window, argv, argc);
               }
            if (*event_union.xclient.data.l == delete_window_atom)
               {
                  XCloseDisplay(display);
                  exit(0);
               }
         }
      break;

 

   }     /* end switch on event_union.type */
}     /* end while 1  */



/*XCloseDisplay(display);*/

} /* end of main */


void draw_squares(int        square_size,
                  Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC         string_gc,
                  GC         inv_gc,
                  int       *free_colors)
{
int       row;
int       col; 
int       idx;
char      num[5];
unsigned long  white_on;
XPoint    dx[5];
int       sq = square_size - 1;

white_on = (colors[WhitePixel(display, DefaultScreen(display))].red +
            colors[WhitePixel(display, DefaultScreen(display))].green +
            colors[WhitePixel(display, DefaultScreen(display))].blue);

white_on = white_on / 2;

dx[1].x = sq;
dx[1].y = sq;
dx[2].x = 0;
dx[2].y = -sq;
dx[3].x = -sq;
dx[3].y = sq;
dx[4].x = 0;
dx[4].y = -sq;

for (row = 0; row < 16; row++)
   for (col = 0; col < 16; col++)
   {                                      
      idx = (row * 16) + col;
      sprintf(num, "%d", idx);

      if (free_colors && free_colors[idx]) 
         {
            /* entry is free, put a white x in a black box */
            XFillRectangle(display, window, inv_gc,
                           col * (square_size+1),
                           row * (square_size+1),
                           square_size, square_size);
            XFillRectangle(display, window, gc[idx],
                          (col * (square_size+1)) + (square_size/2),
                          (row * (square_size+1)) + (square_size/2),
                          square_size/2, square_size/2);
            dx[0].x = col * (square_size+1);
            dx[0].y = row * (square_size+1);
            XDrawLines(display, window, string_gc,
                       dx, 5, CoordModePrevious);
         }
      else
         XFillRectangle(display, window, gc[idx],
                        col * (square_size+1),
                        row * (square_size+1),
                        square_size, square_size);

      if ((colors[idx].red + colors[idx].green + colors[idx].blue) < white_on)
         XDrawString(display, window, string_gc,
                     (col * (square_size+1)) + 4,
                     (row * (square_size+1)) + (square_size/2),
                     num, strlen(num));
      else
         XDrawString(display, window, inv_gc,
                     (col * (square_size+1)) + 4,
                     (row * (square_size+1)) + (square_size/2),
                     num, strlen(num));

   }

} /* end of draw_squares */

void   create_gcs(Display   *display,
                  Window     window,
                  GC        *gc,
                  XColor    *colors,
                  GC        *string_gc,
                  GC        *inv_gc)
{
unsigned long         temp_pixel;
unsigned long         gc_mask;
XGCValues             gcvalues;
int                   i;
XFontStruct          *font;

/***************************************************************
*  
*  Some sanity checking.  Make sure foreground and background are different.
*  
***************************************************************/

memset((char *)&gcvalues, 0, sizeof(XGCValues));

/****************************************************************
*
*   Build the gc for the screen and pixmap.
*
****************************************************************/

gc_mask = GCForeground
        | GCBackground;


for (i = 0; i < 256; i++)
{
   gcvalues.foreground = i;
   gcvalues.background = i;
   colors[i].pixel = i;


   gc[i] = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

}


XQueryColors(display, 
             DefaultColormap(display, DefaultScreen(display)),
             colors,
             256);


font = XLoadQueryFont(display, "fixed");

gc_mask = GCForeground
        | GCBackground
        | GCFont;
gcvalues.foreground = WhitePixel(display, DefaultScreen(display));
gcvalues.background = BlackPixel(display, DefaultScreen(display));
gcvalues.font       = font->fid;

*string_gc = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

gcvalues.foreground = BlackPixel(display, DefaultScreen(display));
gcvalues.background = WhitePixel(display, DefaultScreen(display));
*inv_gc = XCreateGC(display,
                     window,
                     gc_mask,
                     &gcvalues
                    );

}  /* end of create_gcs */

int  find_free_colors(Display   *display,
                      int       *free_colors)
{
int           rc;
int           i;
int           count = 125;
int           last_worked = 0;
int           last_failed = 256;
int           done = False;
unsigned long free_pixels[256];

memset((char *)free_colors, 0, sizeof(int)*256);

while(!done)
{
   rc = XAllocColorCells(display,   
                         DefaultColormap(display, DefaultScreen(display)),
                         False,
                         NULL, NULL,  /* no plane masks */
                         free_pixels,
                         count);
   if (rc == 0) /* failure */
      {
         last_failed = count;
         if (last_worked + 1 == count) 
            done = True;
         else
            {
               i = count -  (count - last_worked) / 2;
               if (i == count)
                  count--;
               else
                  count = i;
            }
      }
   else
      {
         XFreeColors(display,
                     DefaultColormap(display, DefaultScreen(display)),
                     free_pixels,
                     count,
                     0); /* no planes */
         last_worked = count;
         if (last_failed - 1 == count) 
            done = True;
         else
            {
               i = count +  (last_failed - count) / 2;
               if (i == count)
                  count++;
               else
                  count = i;
            }
      }
}            

for (i = 0; i < last_worked; i++)
   free_colors[free_pixels[i]] = True;

return(last_worked);

} /* end of find_free_colors */


static int which_square(int        square_size,
                        int        x,
                        int        y)
{
int     row;
int     col;

row  = y / (square_size+1);
col  = x / (square_size+1);

return((row * 16) + col);
                        
} /* end of which_square */




struct COLOR_ENT  ;

struct COLOR_ENT {
        struct COLOR_ENT       *next;              /* next data in list                          */
        char                    name[80];          /* paste buffer name                          */
        int                     r;                 /* red                                        */
        int                     g;                 /* green                                      */
        int                     b;                 /* blue                                       */
  };


static struct COLOR_ENT  *load_colors(void)
{
struct COLOR_ENT        *new;
struct COLOR_ENT        *head = NULL;
struct COLOR_ENT        *last = NULL;
FILE                    *stream;
char                     line[256];
int                      r,g,b;
char                     name[80];
char                     junk[80];
int                      count;
static struct COLOR_ENT  static_head;

if ((stream = fopen("/usr/lib/X11/rgb.txt", "r")) == NULL)
   if ((stream = fopen("/usr/openwin/lib/rgb.txt", "r")) == NULL)
      {
         fprintf(stderr, "Cannot open /usr/lib/X11/rgb.txt or /usr/openwin/lib/rgb.txt for input (%s)\n", strerror(errno));
         head = &static_head;
         memset((char *)head, 0, sizeof(struct COLOR_ENT));
         return(head);
      }

while(fgets(line, sizeof(line), stream) != NULL)
{
   
   count = sscanf(line, "%d %d %d %s %s", &r, &g, &b, name, junk);
   if (count != 4)
      continue;
   new = malloc(sizeof(struct COLOR_ENT));
   if (!new)
      {
         fprintf(stderr, "Out of memory");
         fclose(stream);
         break;
      }
   memset((char *)new, 0, sizeof(struct COLOR_ENT));
   new->r = r;
   new->g = g;
   new->b = b;
   strcpy(new->name, name); 
   if (head)
      last->next = new;
   else
      head = new;
   last = new;

}

return(head);

} /* end of  load_colors */


char  *name_the_color(int r, int g, int b)
{
static struct COLOR_ENT  *head = NULL;
struct COLOR_ENT         *cur;
static char               buff[256];

if (!head)
   head = load_colors();

for (cur = head; cur; cur = cur->next)
   if ((r == cur->r) && (g == cur->g) && (b == cur->b))
      {
         sprintf(buff, " (%s)", cur->name);
         return(buff);
      }

sprintf(buff, " (#%02X%02X%02X)", r, g, b);
return(buff);

} /* end of name_the_color */

static void save_restart_state(Display    display,
                               int        argc,
                               char      *argv[])
{
} /* end of save_restart_state */



char   *shared_color(Display        *display,
                     XColor         *color_parm,
                     int             free_color,
                     unsigned long   index)
{
unsigned long         pixel;
char                 *returned_val;
XColor                color;
int                   success;

if (free_color)
   returned_val = "";
else
   {
      memcpy((char *)&color, (char *)color_parm, sizeof(color));

      success = XAllocColor(display,  DefaultColormap(display, DefaultScreen(display)),  &color);
      if (!success) /* zero means failure */
         returned_val = ":pvt";  /* private color */
      else
         {
            if (color.pixel == index)
               returned_val = ":shr";  /* shared color */
            else
               returned_val = ":pvt";  /* private color */

            XFreeColors(display,
                  DefaultColormap(display, DefaultScreen(display)),
                  &color.pixel,
                  1,                      /* one pixel value to free */
                  0);
         }
   }

return(returned_val);

} /* end of shared_color */

static void build_picture(Display    *display,
                          int        *free_colors,
                          XColor     *colors,
                          int         print_rgb)

{
int              free_colors_count;
int              i;

free_colors_count = find_free_colors(display, free_colors);
if (print_rgb)
   fprintf(stdout, "%d free color map entries available (pixels)\n\n", free_colors_count);
fflush(stdout);
XQueryColors(display, 
             DefaultColormap(display, DefaultScreen(display)),
             colors,
             256);

for (i = 0; i < 256; i++)
   if (free_colors[i])
      {
         colors[i].red   = 0;  /* unallocated colors are not drawn */
         colors[i].green = 0;
         colors[i].blue  = 0;
      }
} /* end of build_picture */

