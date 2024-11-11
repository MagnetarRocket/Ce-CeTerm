/*static char *sccsid = "%Z% %M% %I% - %G% %U% ";*/
#include <stdio.h>          /* /usr/include/stdio.h      */
#include <string.h>         /* /usr/include/string.h     */

#include "debug.h"
#include "memdata.h"
#include "search.h"


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

/***********************************************************************
*
*  a_to_re - aegis_to_regular_expression (as in 'ed') converter
*
*  Parameter 1: Aegis string to be replaced. Output (the corresponding
*               Bourne RE) is returned in this parameter.
*
*  Parameter 2: 'replace' flag. If 1, input string acts as replacement
*               string in substitute expression (e.g., string2 in 
*               's/string1/string2/').
*
*  Conversion Table (characters not listed map to themselves):
*
*      input           output when   output when   inside [] &
*      string          replace = 0   replace = 1   replace = 0    
*      ------          -----------   -----------   -----------    
*      % as 1st char   ^             %             N/A                R=1 was ^ 7/11/96
*      % by itself     ^             \%            N/A
*      % otherwise     %             %             %              
*
*      ?               .             ?             ?
*      @?              \?            \?            ?
*                                                                 
*      ~               ~             ~             ^   (a)
*      [~              [^            [~            [~  (b)
*                                                                 
*      @t              (tab)         (tab)         (tab)          
*      @f              (formfeed)    (formfeed)    (formfeed)     
*      @n              (newline)     (newline)     NOTHING! (per KBP 11/6/92)
*      @n$             $             (newline)$    "" ""       ""   ""
*                                                                 
*      @               \             \             
*      @@              @             @             @              
*                                                                 
*      {               \(             {            {
*      @{              {              {            {             
*      }               \)             }            }
*      @}               }             }            }             
*                                                                 
*      \               \\            \\            \
*      @\              \\            \\            \              
*                                                                 
*      .               \.            .             .             
*      ^               \^            ^             ^   (e)
*                                                                 
*      @[~             \[~           \[~           [~
*      @@[~            @[^           @[^           @[~ (b)
*      @@@             @\            @\            @
*      
*      @-              \-            \-            -   (c)
*      @]              \]            \]            ]   (d)
*
*  Notes:                                                               
*      (a): 1st char only; otherwise '~'.
*      (b): Aegis would actually generate a syntax error on the input.
*      (c): '-' must be moved to end of [...] pair.
*      (d): ']' must be moved to beginning of [...] pair (after '^', if any).
*      (e): If first char in [...] pair, it must be moved.
*
*  Weird Cases:
*      [^] --> /^/
*      [^@-] --> [-^] (dash NOT at end). Same with [@-^]. Same without @.
*      [~@-] --> [^-] (just for comparison)
*      [-a@]] --> []a-]
*      [-z-] --> [z-]
*
*  Not worth dealing with right now:
*      [^-z] --> [_-z^] 
*      [a-@]] --> []a-\]
*      [--z@]] --> [].-z-]
*
***********************************************************************/

int a_to_re(char *t, int replace) 
{

    static char s[EBUF_MAX];  /* copy of input string */
    int i;             /* s index  */
    int j;             /* t index  */
    int k;             /* temp t index */
    int len;           /* length of s */
    int inverse = 0;   /* in a [^] */
    int brackloc = 0;  /* index of left bracket in t */
    int brack = 0;     /* flag: prev char was '[' */
    int esc = 0;       /* flag: prev char was '@' */
    int dollar = 0;    /* flag: last char in RE is non-escaped '$' */
    int inbrack = 0;   /* bitflag (OR'd): 
                          001 ==> current char is within [brackets] 
                          010  ==> [brackets] contain a real (escaped) '-'
                          100  ==> [ followed immediately by non-translated '^'
                       */

    if (!t)
        return 0;
    
    strcpy(s, t);
    len = strlen(s);
    
    DEBUG13( fprintf(stderr, " Converting Aegis to Regular expressions(%s,", s); ) 
    
    for (i = 0, j = -1; i < len; i++) {

        dollar = ((i == len - 1) && (s[i] == '$') && !esc);

        switch (s[i]) {
            case '%':  
            /* no need to check esc/brack since substitution can only occur on 1st char */
    
                if (replace && !strcmp(s, "%")) {
                    t[++j] = '\\';
                    t[++j] = '%';
                }
                else  
                    t[++j] = (i || replace) ? '%' : '^';
                
                esc = brack = 0;
                break;

            case '?':
                t[++j] = (esc || inbrack || replace) ? '?' : '.';
                esc = brack = 0;
                break;

            case '~':
                t[++j] = brack ? '^' : '~';
                inverse++;
                brackloc += brack;    /* [^ ==> bump brackloc pointer */
                esc = brack = 0;
                break;
        
            case 't':
                if (esc)
                    t[j] = 9;
                else
                    t[++j] = 't';
                esc = brack = 0;
                break;

            case 'f':
                if (esc)
                    t[j] = 12;
                else
                    t[++j] = 'f';
                esc = brack = 0;
                break;

            case 'n':
                if (esc)
                    t[j] = 10; 
                else
                    t[++j] = 'n';
                esc = brack = 0;
                break;

            case '@':
                if (esc) {
                    esc = 0;
                    t[j] = '@';
                }
                else {
                    if (inbrack) {

                        ++i;
                        switch (s[i]) {
                            case ']':
                      
                                /* painful anomaly; bracket must be moved up front: */
                      
                                for (k = ++j; k > brackloc + 1; k--) 
                                    t[k] = t[k - 1];
                                t[k] = ']';
                                break;
                      
                            case '-':  /* another anomaly; move to end */
                                inbrack |= 2;
                                break;

                            case 't': 
                                t[++j] = 9;
                                break;

                            case 'f': 
                                t[++j] = 12;
                                break;

                            case 'n': 
                                if (!inverse) t[++j] = 10 | 0200; /* per KBP 11/16/92 */
                                /* compile_error(51); */ /* KBP 11/9/93 */
                                break;
                      
                            default:
                                t[++j] = s[i];  /* don't escape, even if '@' */
                        }
                    }
                    else { 

                        /* not between [brackets]; process escapes normally */

                        esc = 1;
                        t[++j] = '\\';
                    }
                }
    
                brack = 0;
                break;

            case '-':
                if (inbrack && s[i + 1] != '-' && 
                  (s[i + 1] == ']' ||                          /* [@-z-] shouldn't become [z--] */
                    (j == brackloc && 
                      (s[i - 1] == '[' ||                      /* [-a@]] shouldn't become []-a] */
                         (s[i - 1] == '~' && s[i - 2] == '[')  /* [~-a@]] shouldn't become [^]-a] */
                      )
                    )
                  )
                )
                    inbrack |= 2;  /* move '-' to end */
                else
                    t[++j] = '-'; 

                esc = brack = 0;
                break;

            case '[':
                t[++j] = '[';
                brack = !esc & !replace;

                if (!inbrack) 
                    brackloc = j * (inbrack = brack & !replace);

                esc = 0;
                break;
    
            case ']': /* see inbrack declaration for meaning of inbrack masks */
                if (inbrack & 4) 

                    /* exceptional case:  [^], '^' untranslated */

                    if (~inbrack & 2 && j == brackloc && t[j] == '[') {

                        /* output: \^ */

                        t[j] = '\\';
                        t[++j] = '^';
                        inbrack = brackloc = esc = brack = 0;
                        break;
                    }

                    else  /* normal case */
                        t[++j] = '^';

                if (inbrack & 2)
                
                    /* exceptional case:  [^@-], '^' untranslated */

                    if (inbrack & 4 && j == brackloc + 1 && t[j - 1] == '[') { 

                        /* output: [-^] */

                        t[j] = '-';
                        t[++j] = '^';
                    }

                    else  /* normal case */
                        t[++j] = '-';


                t[++j] = ']';
                inbrack = brackloc = esc = brack = 0;
                inverse = 0; /* KBP 2/22/94 */
                break;

            case '{':
            case '}':
                if (inbrack || replace)
                    t[++j] = s[i];
 
                else if (esc)
                    t[j] = s[i]; /* clobber escape char */
    
                else {
                    t[++j] = '\\';
                    t[++j] = (s[i] == '{' ? '(' : ')');
                }
    
                esc = brack = 0;
                break;
    
            case '\\':  /* this is a single backslash */
                t[++j] = '\\';

                if (!esc && !inbrack)
                    t[++j] = '\\';
    
                esc = brack = 0;
                break;
    
            case '.':
            case '^':
                if (!replace && !inbrack) {
                    t[++j] = '\\';
                    t[++j] = s[i];
                }

                else if (s[i] == '^' && inbrack) /* Can't just say 'if (brack)', since '[@-^]' would fail */
                    inbrack |= 4; /* To be safe, ALWAYS putting '^' at end (but before '-') */

                else
                    t[++j] = s[i];

                esc = brack = 0;
                break;

            default: 
                t[++j] = s[i];
                esc = brack = 0;

        }  /* SWITCH */
    }  /* FOR */
    
    t[++j] = '\0';
    
    if (inbrack)
        compile_error(49); /* no closing bracket */

    if (dollar && t[j - 2] == 10) { /* special case: @n$ => $ */
        t[j - 2] = '$';
        t[--j] = '\0';
    }

    DEBUG13( fprintf(stderr, "%s)\n", t); ) 
        
    return dollar;

}  /* a_to_re() */


