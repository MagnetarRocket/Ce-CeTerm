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
*  Routines in textflow.c
*     dm_tf  - text flow a region.
*
*  Internal routines:
*  
***************************************************************/

#include <stdio.h>          /* /usr/include/stdio.h      */
#include <errno.h>          /* /usr/include/errno.h      */
#include <limits.h>         /* /usr/include/limits.h     */
#include <string.h>         /* /usr/include/string.h     */

#include "cd.h"
#include "debug.h"
#include "dmwin.h"
#include "dmc.h"
#include "dmsyms.h"
#include "mark.h"
#include "memdata.h"
#include "textflow.h"
#include "txcursor.h"
#include "mvcursor.h"

#define BLOCK_LIMIT 80    /* % the last line must be full before it is blocked */

#define DELIMITERS " \t"

#define WHITE_SPACE(c) ((c == ' ') || (c == '\t'))
#define NZ(t) ((t) ? (t) : 1) /* NOT ZERO */

static int  blank_fill(char *text, int need);
static void trim_leading_blanks(char *ptr);
static void trim_trailing_blanks(char *ptr);
static void trim_internal_blanks(char *ptr);

int dm_tf(DMC *dmc, MARK_REGION *mark1, BUFF_DESCR *cursor_buff, int echo_mode)
{

int rc;
int len;
int t_line;
int b_line;
int width;

int dash_l, dash_r;

int redraw_needed = 0;
int changed = 1;
int block = 1;  /* block the current line */

int b_col, t_col; /* dummy declares */

char *buf; 
char *ptr; 

char *limit;
char *word_end;
char *last_word;
char *word_begin;

char text[(MAX_LINE*2)+2];
char line[(MAX_LINE*2)+2];
char next_line[(MAX_LINE*2)+2];

PAD_DESCR *buffer;
DATA_TOKEN *token;

rc = range_setup(mark1,            /* input   */
                 cursor_buff,      /* input   */
                 &t_line,          /* output  */
                 &t_col,           /* output  */
                 &b_line,          /* output  */
                 &b_col,           /* output  */
                 &buffer);         /* output  */


if (rc){
    dm_error("(tf) Invalid Text Range", DM_ERROR_BEEP);
    return(0);
}

if ((echo_mode == RECTANGULAR_HIGHLIGHT) && (b_col != t_col)){
    dash_l  =  MIN(t_col, b_col)+1;
    dash_r  =  MAX(t_col, b_col);
}else{
    dash_l  =  dmc->tf.dash_l;
    dash_r  =  dmc->tf.dash_r;
}

DEBUG13( fprintf(stderr, " @TF top:%d, bot: %d, -l(%d), -r(%d), -c(%d), -b(%d)\n",
       t_line, b_line, dash_l, dash_r, dmc->tf.dash_c, dmc->tf.dash_b) ;)

/*
 *  Sanity checks
 */

if ((dash_l < 0) ||   
    (dash_r < 0)){
    dm_error("(tf) margin(s) must be greater than 0.", DM_ERROR_BEEP);
    return(0);
}

if (!dash_r && !dash_l && !dmc->tf.dash_c)
    return(0);   /* nothing to do! */

if (dash_r && (dash_r <= dash_l)){
   dm_error("(tf) margins may not overlap", DM_ERROR_BEEP);
   return(0);
}

if (dmc->tf.dash_c && !dash_r){
   dm_error("(tf) -c requires left and right margins", DM_ERROR_BEEP);
   return(0);
}

if (dmc->tf.dash_b && !dash_r){
   dm_error("(tf) blank insertion requires a right margin", DM_ERROR_BEEP);
   return(0);
}

if (dmc->tf.dash_b && dmc->tf.dash_c){
   dm_error("(tf) blank insertion and centering are mutually exclusive", DM_ERROR_BEEP);
   return(0);
}

if ((dash_l) > MAX_LINE || 
    (dash_r  > MAX_LINE)){
   dm_error("(tf) margin(s) too large", DM_ERROR_BEEP);
   return(0);
}

/*
 *  loop for each line in the region and make the appropriate change in formatt
 */

width = dash_r - dash_l; 
token = cursor_buff->current_win_buff->token;

for ( buf = get_line_by_num(token, t_line); 
     (buf != NULL) && (t_line <= b_line); 
      buf = get_line_by_num(token, t_line)){

        strcpy(line, buf);
        trim_leading_blanks(line);
        trim_trailing_blanks(line);   
        if (dmc->tf.dash_p) trim_internal_blanks(line);   

        buf = line;
        if (!(*buf)){
           put_line_by_num(token, t_line++, "", OVERWRITE);
           continue;
        }

        if (!dmc->tf.dash_c && (int)strlen(buf) < width) /* lengthen line */
           while (((int)strlen(buf) < width) && 
                  (t_line != total_lines(token)) &&
                  (t_line < b_line)){

                 DEBUG13( fprintf(stderr, "PJG([%d]'%s')\n", t_line, text);)

                 ptr = get_line_by_num(token, t_line +1);
                 strcpy(next_line, ptr);

                 trim_leading_blanks(next_line);  
                 trim_trailing_blanks(next_line);
                 if (dmc->tf.dash_p) trim_internal_blanks(next_line);   

                 if (!strlen(next_line)){
                   if (((int)strlen(line) * 100  / width) < BLOCK_LIMIT)
                       block = 0; /* don't block last line in paragraph */
                   put_line_by_num(token, t_line, line, OVERWRITE);
                   break;
                 }

                 strcat(line, " ");
                 put_line_by_num(token, t_line, line, OVERWRITE);
                 put_line_by_num(token, t_line + 1, next_line, OVERWRITE);

                 if (COLORED(token))
                    cd_join_line(token, t_line, strlen(line));
                 join_line(token, t_line);
                 buf = get_line_by_num(token, t_line);
                 b_line--;
                 strcpy(line, buf);
           }

        len = strlen(buf);
        ptr = text;

        if ((len + dash_l) >= MAX_LINE){
            dm_error("(tf) line too large", DM_ERROR_BEEP);
            return(0);
        }

        if (dash_l || dmc->tf.dash_c){  /* left/center */
             if (dmc->tf.dash_c && (len <= width)){
                memset(ptr, ' ', dash_l + ((width - len) / 2));
                ptr = text + dash_l + ((width - len) / 2) -1;
             }else{
                memset(ptr, ' ', NZ(dash_l) -1);
                ptr = text + NZ(dash_l) -1;
             }
        }

        strcpy(ptr, buf);    /* copy line into my buffer */
     
        if (dash_r && !dmc->tf.dash_c){                  /* right */
           last_word = word_begin = ptr;
           word_end =  word_begin + strcspn(word_begin, DELIMITERS);
           limit = text + dash_r -1;
           while (word_begin != word_end){ 
               if (word_begin > limit){
                  len = last_word - text;
                  put_line_by_num(token, t_line, text, OVERWRITE); 
                  split_line(token, t_line, len--, 1); /* 1=split, 0=truncate */ 
                  strcpy(text, get_line_by_num(token, t_line));
                  if (WHITE_SPACE(*(text + len))){
                      len--;
                      while(WHITE_SPACE(*(text + len)) && len) len--;
                      *(text + len +1) = '\0';
                      put_line_by_num(token, t_line, text, OVERWRITE);
                  }
                  b_line++;   /* bottom is moved down */
                  break;
               }
               if (((word_end-1) > limit) && (word_begin != ptr)){ /* except first pass */
                  len = word_begin - text;
                  put_line_by_num(token, t_line, text, OVERWRITE);
                  split_line(token, t_line, len--, 1); /* 1=split, 0=truncate */ 
                  strcpy(text, get_line_by_num(token, t_line));
                  if (WHITE_SPACE(*(text + len))){
                      len--;
                      while(WHITE_SPACE(*(text + len)) && len) len--; 
                      *(text + len +1) = '\0';
                      put_line_by_num(token, t_line, text, OVERWRITE);
                  }
                  b_line++;   /* bottom is moved down */
                  break;
               }
               last_word =  word_end;
               word_begin = word_end + strspn(word_end, DELIMITERS);
               word_end = word_begin + strcspn(word_begin, DELIMITERS);
           } /* while */
        }

        put_line_by_num(token, t_line, text, OVERWRITE); /* over kill */

        if (dmc->tf.dash_b && (t_line < b_line) && block){   /* blank insertion */
           len = strlen(text) - dash_l;
           blank_fill(text + NZ(dash_l) -1, width - len);
           put_line_by_num(token, t_line, text, OVERWRITE); 
        }

        DEBUG13( fprintf(stderr, " new line[%d-%d]: '%s'\n", t_line, b_line, text);)
        t_line++;

        block++;

} /* for all lines */

if (changed)
      redraw_needed = cursor_buff->current_win_buff->redraw_mask & FULL_REDRAW;

redraw_needed |= dm_position(cursor_buff, cursor_buff->current_win_buff, b_line, 0);

DEBUG13( fprintf(stderr, " TF completed\n");)

mark1->mark_set = False;
return(redraw_needed);

} /* dm_tf */

/****************************************************************
*   blank_fill - add needed blanks to pad out a line
*****************************************************************/

#define ADD_BLANK(c) {strcpy(buf, (c)); *(c) = ' '; strcpy((c)+1, buf);}

static int blank_fill(char *text, int need)
{

int which = 0;

char *ptr;
char *end;
char *begin;
char *text_end;

char  buf[(MAX_LINE*2)+2];

if (!need) return(0);

text_end = text + strlen(text);

DEBUG13( fprintf(stderr, " @Blank_fill({%d}[len=%d]%s)\n", need, strlen(text), text);) 

begin = text;
while (begin = strstr(begin, ". ")){ /* look for periods first */
       begin++;
       if (((begin+1) >= text_end) || *(begin+1) == ' ')
          continue;                
       ADD_BLANK(begin);
       need--;
}

DEBUG13( fprintf(stderr, " Blank.fill([%d]%s)\n", need, text);) 

end = strrchr(text, ' ');
begin = strchr(text, ' ');

if (!begin) return(need); /* RES 6/23/95 Add due to lint message, guessing that need is the thing to return */

while ((begin || end) && (need > 0)){

    TOGLE(which);
 
    if (which){
        if (begin){
            for (ptr = begin; *ptr == ' '; ptr++);
            if ((ptr-1) != end){   /* passing case */
                ADD_BLANK(begin)
                text_end++;
                begin += 2;
                need--;
            }
            if (end && (end > (ptr-1)))  /* passed end */
                end++;
            while (*begin == ' ') begin++;
            if (begin >= text_end)
                begin = NULL;
            else
                begin = strchr(begin, ' ');
        }
    }else{
        if (end){
            while ((*end == ' ') && end > text) end--;
            if ((end+1) != begin){  /* passing case */
                ADD_BLANK(end+1) 
                text_end++;
                need--;
            }
            if (begin && (begin > (end+1))) /* passed begin */
                begin++;
            if (end <= text){
                end = NULL;
            }else{
                while ((*end != ' ') && end > text) end--;
                if (end <= text) end = NULL;
            }
        }
    }     /* which */
}    /* while more needed */

while (need > 0){ /* one pass was not enough, go recursive */
     DEBUG13( fprintf(stderr, " R-Blankfill([%d]%s)\n", need, text);) 
     need = blank_fill(text, need);
}

DEBUG13( fprintf(stderr, " Blankfill_done({%d}[%d]%s)\n", need, strlen(text), text);) 

return(need);

}  /* blank_fill() */

/****************************************************************
*   trim_trailing_blanks -  obvious
*****************************************************************/

static void trim_trailing_blanks(char *ptr)
{

char *p;
int len = strlen(ptr);  

p = ptr + len -1;

while ((p >= ptr) && WHITE_SPACE(*p))
        p--;

*(p+1) = '\0';

}  /* trim_trailing_blanks() */

/****************************************************************
*   trim_leading_blanks -    obvious
*****************************************************************/

static void trim_leading_blanks(char *ptr)
{

char *nb;

nb = ptr + strspn(ptr, DELIMITERS);

while (*ptr++ = *nb++)
      ; /* NOP */

}  /* trim_leading_blanks() */  

/****************************************************************
*   trim_internal_blanks -    obvious
*****************************************************************/

static void trim_internal_blanks(char *ptr)
{

char *nb;

nb = ptr + strspn(ptr, DELIMITERS);

while (*ptr = *nb){
      if (!(WHITE_SPACE(*nb) && WHITE_SPACE(*(nb+1))))
         ptr++;
      nb++;
}

}  /* trim_internal_blanks() */


