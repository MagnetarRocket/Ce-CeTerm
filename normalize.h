#ifndef _NORMALIZE_FILE_NAME_INCLUDED
#define _NORMALIZE_FILE_NAME_INCLUDED    

/* static char *normalize_file_name_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in normalize.c
*         normalize_file_name - Take the .. and . qualifiers out of a path.
*         get_full_file_name  - Translate a partial file name to a full path
*         search_path         - Search $PATH for a command
*         translate_tilde     - fixup a file name which starts with tilde (~)
*         get_home_dir        - extract the home directory on different systems
*         evaluate_soft_link  - Turn a soft link into a path name
*         substitute_env_vars - Replace $variables in a buffer
*
***************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif

void  normalize_file_name(char    *name,
                          short    name_len,
                          char    *norm_name);

void get_full_file_name(char    *edit_file,
                        int      follow_link);

void search_path(char    *cmd,
                 char    *fullpath,
                 char    *env_path_var,
                 char     separator);

void search_path_n(char    *cmd,
                   int      max_fullpath,  /* sizeof(fullpath) */
                   char    *fullpath,
                   char    *env_path_var,
                   char     separator);

char *translate_tilde(char  *file);

char *get_home_dir(char *dest,
                   char *user);

void evaluate_soft_link(char  *hard_path,  /* input/output */
                        char *link);       /* input        */

int  substitute_env_vars(char     *buff,
                         int       max_len,
                         char      escape_char);

#ifdef  __cplusplus
}
#endif

#endif
