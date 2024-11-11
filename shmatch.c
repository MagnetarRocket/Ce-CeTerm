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

/*******************************************************************

MODULE: shell_match   -  perform shell pattern matching

DESCRIPTION:
      This routine accepts a test string an a shell pattern (see below).
      It returns true if the pattern matches and false if it does not.

USAGE:   
      match = shell_match(file_name, pattern);
      
      char  file_name[]  = "crpad.c";
      char  pattern[]    = "*.[ch]";
      int   match;


PARAMETERS: 
   1.  test_str -  pointer to char (INPUT)
                   This is the string to be tested.

   2.  pattern  -  pointer to character
                   This is the shell pattern which to match against the
                   test string.  If the pattern contains no shell wild
                   card values, this call becomes a strcmp.

FUNCTIONS:
   1.   See if there are any shell wild card characters in the string.  If not,
        do a strcmp and return the result.

   2.   Compare the test string to the pattern piecemeal until a match or
        lack of a match is confirmed.

RETURNED VALUE:
   match   -  int
              True  -  Passed test string matches the pattern
              False -  Passed test string does not match the pattern.

NOTES:

  Taken from the shell manual page.
  File Name Generation
     Before a command is executed, each command word is scanned for the
     characters *, ?, and [.  If one of these characters appears the word is
     regarded as a pattern.  The word is replaced with alphabetically sorted
     file names that match the pattern.  If no file name is found that matches
     the pattern, the word is left unchanged.  The character . at the start of
     a file name or immediately following a /, as well as the character /
     itself, must be matched explicitly.

          *    Matches any string, including the null string.
          ?    Matches any single character.
          [...]
               Matches any one of the enclosed characters.  A pair of
               characters separated by - matches any character lexically
               between the pair, inclusive.  If the first character following
               the opening ``['' is a ``!'' any character not enclosed is
               matched.

 
*******************************************************************/

#include <stdio.h>   /*  /usr/include/stdio.h  */
#include <string.h>  /*  /usr/include/string.h */

#include "shmatch.h"

/***************************************************************
*  
*  Local prototypes
*  
***************************************************************/

static const char *pat_compare(const char    *test_str,
                               const char    *pattern);

static int   shell_match_l(const char    *test_str,
                           const char    *pattern,
                           int      outermost_recursion);


/***************************************************************
*  
*  Routine shell_match
*  Pass the parameters to the internal routine to do the work.
*  
***************************************************************/

int   shell_match(const char    *test_str,
                  const char    *pattern)
{
return(shell_match_l(test_str, pattern, 1));
}  /* end of shell_match */


/***************************************************************
*  
*  Match the test string against the pattern.
*  This routine can be recursive.
*  
***************************************************************/

static int   shell_match_l(const char    *test_str,
                           const char    *pattern,
                           int      outermost_recursion)
{

const char  *pat_pos = pattern;
const char  *str_pos = test_str;
char        *star_search;
int          after_slash = outermost_recursion;


if (strpbrk(pattern, "*?[") == NULL)
   return(strcmp(test_str, pattern) == 0);
else
   while(*pat_pos)
   {
      if (after_slash)
         {
            if (*str_pos == '.')
               if (*pat_pos != '.')
                  return(0);
               else
                  {
                     str_pos++;
                     pat_pos++;
                  }
            after_slash = 0;
         }
      switch(*pat_pos)
      {
      case '\0':
         break; 

      case '?':
         pat_pos++;
         str_pos++;
         break;

      case '[':
         pat_pos = pat_compare(str_pos, pat_pos);
         if (!pat_pos)
            return(0);
         str_pos++;
         break;

      case '*':
         pat_pos++;
         if (!(*pat_pos))
            return(1); /* star at the end matches all */
         while((star_search = strchr(str_pos, *pat_pos)) != NULL)
            if (shell_match_l(star_search, pat_pos, after_slash))
               return(1);
            else
               str_pos = star_search + 1;
         return(0);
         break;

      case '\\':
         pat_pos++;
         if (!*pat_pos)
            break;

      default:
         if (*pat_pos != *str_pos)
            return(0);
         if (*str_pos == '/')
            after_slash = 1;
         pat_pos++;
         str_pos++;
         break;

      } /* end of switch on pattern char */
   }

if (*str_pos) /* string not done yet */
   return(0);
else
   return(1);

} /* end of shell_match_l */


/***************************************************************
*  
*  Process single character compare of bracketed characters.
*  
***************************************************************/

static const char *pat_compare(const char    *test_str,
                               const char    *pattern)
{
char           pat[32];   /* 256 bits */
int            index;
int            shift;
int            first = 1;
unsigned char  c;

memset(pat, 0, 32);

pattern++; /* skip the [ */

while(*pattern && (*pattern != ']'))
{
   switch(*pattern)
   {
   case '-':
      if (!first)
         {
            for (c = ((unsigned char)*(pattern-1))+1; c <= (unsigned char)*(pattern+1); c++)
            {
               index = c / 8;
               shift = c % 8;
               pat[index] |= 1 << shift;   
            }
            pattern += 2;
         }
      else
         {
            /* same as default */
            index = *pattern / 8;
            shift = *pattern % 8;
            pat[index] |= 1 << shift;   
            pattern++;
         }
      break;

   case '\\':
      pattern++;
      if (!*pattern)
         break;

   default:
      index = *pattern / 8;
      shift = *pattern % 8;
      pat[index] |= 1 << shift;   
      pattern++;
      break;
   } /* end of switch */

   first = 0;
}

if (!(*pattern))
   {
      fprintf(stderr, "Bad pattern no closing ]\n");
      return(NULL);
   }

*pattern++; /* move past the ']' */

index = *test_str / 8;
shift = *test_str % 8;
if (pat[index] & (1 << shift))
   return(pattern);
else
   return(NULL);

} /* end of pat_compare */



