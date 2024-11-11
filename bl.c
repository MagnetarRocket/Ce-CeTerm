/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
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

/***************************************************************
*
*  module bl.c
*
*  Routines:
*     dm_bl       -   Balance matching delimiter identifiers
*
*     After return from these routines. A screen redraw may
*     be required, and or a warp cursor.
*
*
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h             */
#include <string.h>         /* /bsd4.3/usr/include/string.h     */
#include <errno.h>          /* /usr/include/errno.h             */
#include <ctype.h>          /* /usr/include/ctype.h             */

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"          /* /usr/include/X11/X.h      */
#include "X11/Xlib.h"       /* /usr/include/X11/Xlib.h   */
#include "X11/Xutil.h"      /* /usr/include/X11/Xutil.h  */
#include "X11/keysym.h"     /* /usr/include/X11/keysym.h */
#endif
#else
#include <X11/X.h>          /* /usr/include/X11/X.h      */
#include <X11/Xlib.h>       /* /usr/include/X11/Xlib.h   */
#include <X11/Xutil.h>      /* /usr/include/X11/Xutil.h  */
#include <X11/keysym.h>     /* /usr/include/X11/keysym.h */
#endif

#include "debug.h"
#include "dmc.h"
#include "dmsyms.h"
#include "dmwin.h"
#include "memdata.h"
#include "mvcursor.h"
#include "typing.h"
#include "bl.h"
#include "undo.h"
#include "search.h"


/************************************************************************
*
* NAME:      dm_bl  -  DM bl (Balance) command
*
* PURPOSE:    This routine will balance matching identifiers
*
* PARAMETERS:  bl [ -c | -n | -d ] [l_ident] [r_ident]
*
*   -c - dont move cursor and dont add a matching identifier
*   -n - no back (do not return to the original mark [Not Implemented] 
*   -d - defaults take the defaults list of matches
*
*   1.  cursor_buff   - pointer to BUFF_DESCR (INPUT / OUTPUT)
*               This is a pointer to the buffer / window description
*               showing the current state of the window.
*
*   2.  mark1 - pointer to MARK_REGION (INPUT)
*               If set, this is the most current mark.  It is matched
*               with tdm_mark or cursor_buff to get the line range.
*
*   3.  dmc   - Pointer to DMC (INPUT)
*               This is a pointer to the DM command structure
*               for a DMC_bl command.
*
*
* FUNCTIONS :
*
*
*   1.  Position window over character to be found.
*
*   2.  Position cursor over character to be found 
*
*   3.  Insert appropriate characters
*        
*    (   bl -c )  ) {  )  }  )
*
*************************************************************************/

#define MAX_BL 10   /* number of pre defined balanced pairs */
static char *bl_pairs[MAX_BL*2] =  { "(", ")",
                                     "{", "}",
                                     "[", "]",
                                     "<", ">",
                                     "/*", "*/",
                                     "(*", "*)",
                                     "#ifdef", "#endif",
                                     "begin", "end",
                                     "do", "done",
                                     "case", "esac" };


#define UP   0
#define DOWN 1

static int  has_alpha(char *string);
static int  sindex(char *str1, char *str2);
static int  rsindex(char *str1, char *str2, int end);
static char *on(char *location, char *line, char *token);
static int  bl_look(DATA_TOKEN *token, char *l_ident, char *r_ident, 
                    int direction, int line, int offset, int *row, int *col);

static int case_s;

int dm_bl(BUFF_DESCR *cursor_buff, DMC *dmc, int case_sensetivity)
{

DATA_TOKEN *token;

int i = 0;
int line;
int text_len;
int llen, rlen;
int dist_row = -1, dist_col = -1, dist_save = 0;

char *ptr;
char *text;
char *location;
char msg[256];
char d_left[MAX_LINE], d_right[MAX_LINE];
char temp_line[MAX_LINE+2];

int offset;
int found = 0;
int position =  0;
int redraw_needed = 0;
int t_row = 0, t_col, b_row = 0, b_col = 0; /* row column of found delimeters */


DEBUG13(fprintf(stderr, "@Bl: \n");)

/* 
 *  init search stuff
 */

case_s = case_sensetivity;  /* make it global */

token = cursor_buff->current_win_buff->token;

if (dmc->bl.dash_d)
   snprintf(msg, sizeof(msg), "(BL) - No balancing default identifiers found.");
else
   snprintf(msg, sizeof(msg), "(BL) \"%s %s\" - No balancing identifier found.",
          (dmc->bl.l_ident ? dmc->bl.l_ident : "("),
          (dmc->bl.r_ident ? dmc->bl.r_ident : ")"));

line = cursor_buff->current_win_buff->file_line_no;
offset = cursor_buff->current_win_buff->file_col_no;
text = cursor_buff->current_win_buff->buff_ptr;

if (!case_s){
    strcpy(temp_line, text); 
    text = temp_line;
    lower_case(text);
}

text_len = strlen(text); 

if (offset > text_len) 
    offset = text_len;

location = text + offset;

DEBUG13(fprintf(stderr, " current_char='%c'[offset=%d] line:\n", *location, offset, text);)

/*
 *  weed out eror conditions
 */
       /* same and not NULL is an error */
if (dmc->bl.l_ident && !strcmp(dmc->bl.l_ident, dmc->bl.r_ident)){ 
    dm_error("(BL) Command syntax error, Left and Right must be different", DM_ERROR_BEEP); 
    return(0);
}
           /* -d with and arg is an error */
if (dmc->bl.dash_d && (dmc->bl.l_ident || dmc->bl.r_ident)){ 
    dm_error("(BL) Command syntax error, -d does not take arguments", DM_ERROR_BEEP); 
    return(0);
}

if (!dmc->bl.dash_d &&   /* only one arg is an error */
   (!dmc->bl.l_ident || !dmc->bl.r_ident)  &&
   (dmc->bl.l_ident  || dmc->bl.r_ident)){
      dm_error("(BL) Command syntax error, missing delimiter", DM_ERROR_BEEP);
      return(0);
}

/*
 *  determine what we are searching for
 */

if (dmc->bl.dash_d){    /* -d */
    for (i = 0; i < (MAX_BL*2); i++) /*  -d  */
         if (ptr = on(location, text, bl_pairs[i])){
             if (i & 1) i--;
             location = ptr;
             offset = location - text;  
             strcpy(d_left,  bl_pairs[i]);
             strcpy(d_right ,bl_pairs[i+1]);
             break;
         }
}else{
    if (!dmc->bl.l_ident && !dmc->bl.r_ident){
        strcpy(d_left,  bl_pairs[0]);  /* look for ( ) */
        strcpy(d_right ,bl_pairs[1]);
    }else{
        strcpy(d_left, dmc->bl.l_ident);
        strcpy(d_right, dmc->bl.r_ident);
    }
}

if (i == (MAX_BL*2)) goto use_defaults;

/*
 *  Look for the identifiers
 */

llen = strlen(d_left);
rlen = strlen(d_right);

if (ptr = on(location, text, d_right)){ /* on a right, so look for a left */
    offset = ptr - text; /* added 11/5/97 for bl do done */
    if (!strncmp(location, d_left, llen) && (llen > rlen)) /* l/r is substr of r/l */ 
       /* EMPTY */; /* nop (logic is easier to follow this way) */
    else{
       found = bl_look(token, d_left, d_right, UP, line, offset, &t_row, &t_col);
       b_row = line; /* for fit calculation */
       goto rest;
    }
}
 
if (ptr = on(location, text, d_left)){ /* on a left, so look for a right */
    offset = ptr - text; /* added 11/5/97 */
    found = bl_look(token, d_left, d_right, DOWN, line, offset, &b_row, &b_col);
    position = DOWN;  /* position bottom case */
    t_row = line; /* for fit calculation */
    goto rest;
}

if (!dmc->bl.dash_d){ /* not on either so look for '(' ')' */
    found = bl_look(token, d_left, d_right, DOWN, line, offset, &b_row, &b_col);
    if (found)
       found = bl_look(token, d_left, d_right, UP, line, offset, &t_row, &t_col);
    goto rest;
}

/* all else, look for one of the defaults */

use_defaults:

for (i = 0; i < (MAX_BL*2); i+=2){
     strcpy(d_left,  bl_pairs[i]);
     strcpy(d_right, bl_pairs[i+1]);

     found = bl_look(token, d_left, d_right, UP, line, offset, &t_row, &t_col);

     if (found && (((ABSVALUE(t_row - line) == ABSVALUE(dist_row - line)) &&
                   ((ABSVALUE(t_col - line) >  ABSVALUE(dist_col - line)))) ||
                   ((ABSVALUE(t_row - line) <  ABSVALUE(dist_row - line))))){
                       dist_row = t_row;    /* finds the closest forward token */
                       dist_col = t_col;
                       dist_save = i+1;    /* i can be 0, which is bad */
     }
}   /* for */                                                                              


if (dist_save){
    found = bl_look(token, bl_pairs[dist_save -1], bl_pairs[dist_save], DOWN, line, offset, &b_row, &b_col);
    if (!found) dm_error("(BL) Unmatched delimiter found", DM_ERROR_BEEP);
    b_row = dist_row;
    t_row = dist_row;
    b_col = dist_col;
    t_col = dist_col;
    found++;
}else{
    dist_save = 0; dist_row = -1; dist_col = -1;
    for (i = 0; i < (MAX_BL*2); i+=2){                                               
         strcpy(d_left,  bl_pairs[i]);                                               
         strcpy(d_right, bl_pairs[i+1]);                                             
                                                                                     
         found = bl_look(token, d_left, d_right, DOWN, line, offset, &t_row, &t_col);  
                                                                                     
         if (found && (((ABSVALUE(t_row - line) == ABSVALUE(dist_row - line)) &&               
                       ((ABSVALUE(t_col - line) >  ABSVALUE(dist_col - line)))) ||               
                       ((ABSVALUE(t_row - line)  < ABSVALUE(dist_row - line))))){               
                           dist_row = t_row;    /* finds the closest forward token */
                           dist_col = t_col;                                         
                           dist_save = i+1;                                          
         }                                                                           
    }  /* for */                                                                              
    if (dist_save){
        b_row = dist_row;
        t_row = dist_row;
        b_col = dist_col;
        t_col = dist_col;
        dm_error("(BL) Unmatched delimiter found", DM_ERROR_BEEP);
        found++;
    }else
        found = 0;
}


rest:  /* could have used cascading elseif... but this is prettier */
  
if (!found){
    dm_error(msg, DM_ERROR_BEEP);  /* failed to find in range */
    return(0);
}

DEBUG13( fprintf(stderr, " BL [%d, %d] and [%d, %d]\n", t_row, t_col, b_row, b_col);)

redraw_needed |= dm_position(cursor_buff, cursor_buff->current_win_buff, position ? b_row:t_row, position ? b_col:t_col);

return(redraw_needed);

} /* end of dm_bl */

/************************************************************************
*
*   bl_look - search for a string UP/DOWN in the database and 
*              return its location
*              'count' is used to keep track of imbedded matching delimeters
*              an ident containing alpha/num characters must be delimited
*              by {end-of-line, alnum, NULL, begin-of-line}.
*
************************************************************************/

static int bl_look(DATA_TOKEN *token, char *l_ident, char *r_ident, int direction, int line, int offset, int *row, int *col)
{

int rlen, llen;
int count = 1;  /* n means we have n left(right) and need n right(left) */
int first_pass = 0;  
int lfound;
int rfound;
int really_found  = -1;
int lalpha, ralpha;  /* ident contains alpha/num characters*/
int idx;  /* temporary index */

char *text;
char temp_line[MAX_LINE+2];

DEBUG13( fprintf(stderr, " bl_look  delimiters('%s','%s')\n", l_ident, r_ident);)

llen = strlen(l_ident);
rlen = strlen(r_ident);

lalpha = has_alpha(l_ident);
ralpha = has_alpha(r_ident);

if (direction) 
    offset += llen; 
else
    offset--; 

while (!((really_found != -1) && !count)){
     really_found = -1;
     if (direction){  /* IF DOWN */

/**** THEN ****/

         text = get_line_by_num(token, line++);
         if (!case_s){
             strcpy(temp_line, text); 
             text = temp_line;
             lower_case(text);
         }

 
down:    lfound = sindex(text + offset, l_ident);
         rfound = sindex(text + offset, r_ident);
 
         if (lalpha)
             while ((lfound != -1) && !(!isalnum(text[lfound + offset + llen]) && (!lfound || isspace(text[lfound + offset -1])))){
                   lfound += llen;                                  
                   idx = sindex(text + offset + lfound, l_ident);
                   if (idx == -1) 
                       lfound = idx;
                   else
                       lfound += idx;
         }

         if (ralpha)
             while ((rfound != -1) && !(!isalnum(text[rfound + offset + rlen]) && (!rfound || isspace(text[rfound + offset -1])))){
                   rfound += rlen;
                   idx = sindex(text + offset + rfound, r_ident);
                   if (idx == -1) 
                       rfound = idx;
                   else
                       rfound += idx;
         }

          /*
           *  Four cases found :    left + no right,    left + right, 
           *                     no left + no right, no left + right
           */
 
         if (lfound != -1){
             if (rfound != -1){   /* l + r */
                 if (lfound < rfound){
                     count++;
                     rfound += offset; /* rfound is true offset now */
                     offset += lfound + llen;
                     while ((lfound = sindex(text + offset, l_ident)) != -1){
                          if ((lfound + offset) > rfound) break;
                          offset += lfound + llen;
                          count++;
                     }
                     
                 }else{
                     count--;
                     really_found = offset + rfound;
                     offset = really_found + rlen;
                     if (!count) break;  /* perfect match! */
                 }
                 goto down;    /* process rest of line like it was a fresh line */
             }else{               /* l + nr */
                 count++;
                 offset += lfound + llen;
                 while ((lfound = sindex(text + offset, l_ident)) != -1){
                      offset += lfound + llen;
                      count++;
                 }
             }
         }else{
             if (rfound != -1){  /* nl + r */
                 count--;
                 offset += rfound + rlen;
                 while (count && ((rfound = sindex(text + offset, r_ident)) != -1)){
                       count--;
                       offset += rfound + rlen;
                 }
                 really_found = offset - rlen;
             }
         }                      /* nl + nr       is a nop */

         if (line > total_lines(token)) break; 
         offset = 0;

/**** ELSE ****/

     }else{           /* ELSE UP */

         text = get_line_by_num(token, line--);
         first_pass++;
         if (!case_s){
             strcpy(temp_line, text); 
             text = temp_line;
             lower_case(text);
         }
         if (offset == -99) offset = strlen(text);

up:      lfound = rsindex(text, l_ident, offset);
         rfound = rsindex(text, r_ident, offset);

         if ((lfound == rfound) && (lfound != -1)){ /* l/r is substr of r/l */
            if (llen > rlen)
               rfound = rsindex(text, r_ident, rfound -1);
            else
               lfound = rsindex(text, l_ident, lfound -1);
         }

         if (lalpha)
             while ((lfound != -1) && !(!isalnum(text[lfound + llen]) && (!lfound || isspace(text[lfound -1]))))
                   lfound = rsindex(text, l_ident, lfound -1);

         if (ralpha)
             while ((rfound != -1) && !(!isalnum(text[rfound + rlen]) && (!rfound || isspace(text[rfound -1]))))
                   rfound = rsindex(text, r_ident, rfound -1);

          /*
           *  Four cases found :    left + no right,    left + right, 
           *                     no left + no right, no left + right
           */
 
         if (lfound != -1){
             if (rfound != -1){   /* l + r */
                 if (lfound < rfound){
                     count++;
                     offset = rfound -1;
                     while ((rfound = rsindex(text, r_ident, offset)) != -1){
                          if (rfound < lfound) break;
                          offset = rfound -1;
                          count++;
                     }
                     
                 }else{
                     count--;
                     offset = lfound;
                     really_found = offset--;
                     if (!count) break;  /* perfect match! */
                 }
                 goto up;    /* process rest of line like it was a fresh line */
             }else{               /* l + nr */
                 count--;
                 offset = lfound -1;
                 while (count && ((lfound = rsindex(text, l_ident, offset)) != -1)){
                      offset = lfound -1;
                      count--;        
                 }
                 really_found = lfound;
             }
         }else{                       /* was >= 12/19/95 bug: '   (\n ))' with cursor on last pren fails */ 
             if ((rfound != -1) && !((rfound > offset) && (first_pass == 1))){  /* nl + r */  /* handles case 0 also */
                 count++;
                 offset = rfound -1;
                 while ((rfound = rsindex(text, r_ident, offset)) != -1){
                       offset = rfound -1;
                       count++; /* was -- BUG: (\n ))\n)  on last paren */
                 }
                 really_found = offset;
             }                   /* else  nl + nr   is a nop */
         }

         if (line < 0) break; 
         offset = -99; /* -99 is a flag that says to reset it after the next get_lin() */

     }  /* direction */

/*****************/ 
} /* while more lines */


if ((really_found != -1) && !count){
    *row = direction ? line-1 : line+1;
    *col = really_found;
    return(1);
}

return(0); /* failed */

} /* bl_look() */

/************************************************************************
*
* on - returns 0 if the cursor is not on a delimiter
*      it returns the address of the begining of the delimiter if you are on one
*      
*      location - tells me where we are in a line.
*      line - is the line of text. 
*      token - is the string being looked for 
*
************************************************************************/

static char *on(char *location, char *line, char *token)
{

char *ptr, *tptr, *hptr;
int alpha;

DEBUG13( fprintf(stderr, " BL- @ON() delimiter(%s)\n", token);)

alpha = has_alpha(token);

ptr = line;

while (*ptr && (*ptr != *token))
    ptr++;  

while (*ptr && (ptr <= location)){ 
   hptr = ptr;
   tptr = token;

   while (*tptr && (*ptr == *tptr)){ 
       tptr++; ptr++;
   }

   if (!*tptr && (ptr > location))
       if (!alpha || (alpha && !isalnum(*ptr)) && ((hptr == line) || isspace(*(hptr-1)))){
           DEBUG13( fprintf(stderr, " BL- ON() a delimiter: '%s'\n", hptr);)
           return(hptr);
       }

   while (*ptr && (*ptr != *token))
       ptr++;  
}

DEBUG13( fprintf(stderr, " BL- not ON() a delimiter: '%s'\n", location);)

return(NULL); /* not found */

} /* on_token() */   


/**************************************************************
*
*   Routine sindex
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the first occurance of the second
*    string in the first and returns the offset of the
*    second string.
*
**************************************************************/

static int sindex(char *str1, char *str2)
{

char   c   = *str2;
char   *s1 = str1;
char   *s2 = str2;

int    len1 = strlen(s1);
int    len2 = strlen(s2);

while (len2 <= len1--){
   if (c == *s1)
      if (strncmp(s1, s2, len2) == 0)
         return(s1 - str1);
   s1++;
}

return(-1);

} /* sindex() */

/**************************************************************
*
*   Routine rsindex
*
*   Taken from liba.a, which is missing on this system.
*
*    This routine finds the last occurance of the second
*    string in the first and returns the offset of the
*    second string.  Start_col is the point to start looking backward from.
*
**************************************************************/

static int rsindex(char *str1, char *str2, int start_col)
{
char   *s1;
char   *s2 = str2;
int    len1 = strlen(str1);
int    len2 = strlen(s2);
char   c   = *str2;
char   *start;

#ifdef blah
if (start_col == 0)
   start = &str1[len1-len2];
else  
#endif

   if (len1-len2 > start_col)
      start = &str1[start_col];
   else
      start = &str1[len1-len2];

for (s1 = start;
     s1 >= str1;
     s1--)
{
   if (c == *s1)
      if (strncmp(s1, s2, len2) == 0)
         return(s1 - str1);
}
return(-1);

} /* rsindex() */


/**************************************************************
*
*   Routine has_alpha
*
*       returns true is the string has an alpha in it
*
**************************************************************/

static int has_alpha(char *s)
{

while (*s){
  if (isalpha((int)*s))
      return(1);
  s++;
}

return(0);

} /* has alpha */
