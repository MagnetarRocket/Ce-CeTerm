#ifndef _SHMATCH_H_INCLUDED
#define _SHMATCH_H_INCLUDED

/* "%Z% %M% %I% - %G% %U% " */

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

#ifdef  __cplusplus
extern "C" {
#endif


int   shell_match(const char    *test_str,
                  const char    *pattern);

#ifdef  __cplusplus
}
#endif



#endif

