#ifndef _CDGC__INCLUDED
#define _CDGC__INCLUDED

/* static char *cdgc_h_sccsid = "%Z% %M% %I% - %G% %U% "; */
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
*  Routines in cdgc.c
*     cdgc_bgc           - Set up default background and foreground colors
*     get_gc             - Get the GC for a background/forground combination, create if necessary.
*     new_font_gc        - Process a font change in all known gc's
*     expand_cdgc        - Take GC data from the string data and create the linked list
*     dup_cdgc           - Create a copy of a linked list cdgc element.
*     flatten_cdgc       - Turn GC data in the linked list to the string format
*     GC_to_colors       - Return the colors for a GC.
*     free_cdgc          - Free a GC data linked list
*     extract_gc         - Find the correct GC for a given column
*     line_is_xed        - See if a line is totally x'ed out.
*     chk_color_overflow - See if a string cdgc flows to the next line
*     cdgc_split         - Split color data info into 2 pieces or insert chars
*     find_cdgc          - Scan the memdata database for a color/label data
*
***************************************************************/

#include "drawable.h"    /* needed for FANCY_LINE */
#include "memdata.h"     /* needed for MAX_LINE */

/***************************************************************
*  Special values returned by extract_gc and get_gc
***************************************************************/
#ifdef EXCLUDEP 
#define  XCLUDE_GC ((GC)-1)
#endif
#define  DEFAULT_GC ((GC)0)
/***************************************************************
*  Same thing in string format
***************************************************************/
#ifdef EXCLUDEP 
#define  XCLUDE_BGFB  "XCLUDE"
#endif

#define  DEFAULT_BGFG "DEFAULT"
#define  DEFAULT_BG   "DEF_BG"
#define  DEFAULT_FG   "DEF_FG"

/***************************************************************
*  Special background color for zero size mark regions stored
*  with the color data.  The data is stored as LABEL_BG/label
***************************************************************/
#define  LABEL_BG      "|MARK|"
#define  IS_LABEL(x)  (((x)[0] == '|') && \
                       ((x)[1] == 'M') && \
                       ((x)[2] == 'A') && \
                       ((x)[3] == 'R') && \
                       ((x)[4] == 'K') && \
                       ((x)[5] == '|'))

/***************************************************************
*  Special value for data which overflows lines.
***************************************************************/
#define COLOR_OVERFLOW_COL MAX_LINE+1


void   cdgc_bgc(Display       *display,           /* input  */
                unsigned long  bg_pix,            /* input  */
                unsigned long  fg_pix,            /* input  */
                void         **gc_private_data);  /* output */

GC     get_gc(Display         *display,           /* input  */
              Window           main_x_window,     /* input  */
              char            *bg_fg,             /* input  */
              Font             font,              /* input  */
              void           **gc_private_data);  /* input/output */

void   new_font_gc(Display         *display,           /* input  */
                   Font             font,              /* input  */
                   void           **gc_private_data);  /* input/output */

FANCY_LINE *expand_cdgc(Display       *display,             /* input  -> if we have to make any GC's */
                        Window           main_x_window,     /* input  -> if we have to make any GC's */
                        char            *cdgc,              /* input  */
                        Font             font,              /* input  -> if we have to make any GC's */
                        void           **gc_private_data);  /* input/output */

FANCY_LINE *dup_cdgc(FANCY_LINE         *orig);

void  flatten_cdgc(FANCY_LINE      *cdgc,              /* input */
                   char            *target_area,       /* output*/
                   int              max_target,        /* input */
                   int              just_1);           /* input */

char *GC_to_colors(GC               gc,                /* input */
                   void            *gc_private);       /* input */

void  free_cdgc(FANCY_LINE      *cdgc);


GC    extract_gc(FANCY_LINE      *cdgc,              /* input */
                 int              cdgc_line,         /* input */
                 int              line_no,           /* input */
                 int              col,               /* input */
                 int             *end_col);          /* output */

/***************************************************************
*  Values for the fourth parameter to cdgc_split
***************************************************************/
#define  CDGC_SPLIT_NOBREAK          0
#define  CDGC_SPLIT_BREAK            1


void cdgc_split(char   *color_line,
                int     split_col,
                int     insert_chars,
                int     text_len,
                int     insert_type,
                char   *target_buff,   /* should be MAX_LINE+1 long  or NULL*/
                char   *back_target);  /* should be MAX_LINE+1 long  or NULL*/

#ifdef EXCLUDEP
int   line_is_xed(char            *cdgc,              /* input */
                  int              cdgc_line,         /* input */
                  int              line_no);          /* input */
#endif

int   chk_color_overflow(char      *cdgc);            /* input */

/***************************************************************
*  Values for the 7th parameter to find_cdgc
***************************************************************/
#define  FIND_CDGC_ANY               0
#define  FIND_CDGC_COLOR             1
#define  FIND_CDGC_LABEL             2

FANCY_LINE *find_cdgc(DATA_TOKEN   *token,               /* input   */
                      char         *bgfg,                /* input   */
                      int           top_line,            /* input   */
                      int           bottom_line,         /* input   */
                      int           search_down,         /* input   */
                      int           full_match,          /* input   */
                      int           type,                /* input   */
                      int          *found_line,          /* output  */
                      int          *found_col,           /* output  */
                      FANCY_LINE  **found_cdgc);         /* output  */

#endif

