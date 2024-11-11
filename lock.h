#ifndef _LOCK_H_INCLUDED
#define _LOCK_H_INCLUDED

/* static char *lock_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Routines in pastebuf.c
*     lock_init              -   Set up initial lock data for a file
*     lock_file              -   Issue advisory lock on a file
*     update_lock_data       -   Update stats on a file when it is rewritten
*     unlock_file            -   Release advisory lock on a file
*     lock_rename            -   Deal with pad name
*     save_file_stats        -   Save stats to detect file changed.
*     detect_changed_file    -   See if the passes stat buff matches the last saved
*     file_has_changed       -   Get stats for current file and compare against the saved stats
*
***************************************************************/

/***************************************************************
*
*  Prototypes
*
***************************************************************/

void  lock_init(struct stat    *file_stats);

int   lock_file(char    *edit_file);

void  unlock_file(void);

void  update_lock_data(char    *edit_file);

void  lock_rename(char    *new_edit_file);

void  save_file_stats(struct stat    *file_stats);

int  detect_changed_file(struct stat    *file_stats);

int  file_has_changed(const char *edit_file);

#endif

