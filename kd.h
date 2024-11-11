#ifndef _KEYTODM_INCLUDED
#define _KEYTODM_INCLUDED

/* static char *keytodm_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */
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
*  Routines in kd.c
*         key_to_dm            - Convert key or mouse press/release event to dm command
*         init_dm_keys         - Init the DM lookup table and start the load.
*         process_some_keydefs - Load some key definitions in a background mode
*         install_keydef       - Put a keydef in the table
*         dm_kd                - Process a parsed kd command.
*         dm_cmdf              - Process a cmdf command.
*         dm_kk                - kk (key key command)
*         add_modifier_chars   - Use an X event modifier mask to add modifier chars to a string
*         parse_hashtable_key  - Parse a keys hash table key into keysym, state, and event.
*         ce_merge             - Merge old and new key definitions.
*         dm_eval              - do symbolic substitution and parse a line of data
*
***************************************************************/

#include "dmsyms.h"
#include "drawable.h"
#include "dmc.h"


/***************************************************************
*  
*  Max length of a key name symbol.
*  
***************************************************************/

#define  MAX_KEY_NAME  32

/***************************************************************
*  
*  Size in chars of the hash key table entry.
*  Needed in alias.c, kd.c, lsf.c, pd.c and serverdef.c.
*  
***************************************************************/

#define  KEY_KEY_SIZE  28

/***************************************************************
*  
*  Size in entries of the hash table.
*  Needed in kd.c and cc.c
*  
***************************************************************/

#define  HASH_SIZE 1024

/***************************************************************
*  
*  Function prototypes
*  
***************************************************************/

void      init_dm_keys(char              *backup_keydefs,
                       char              *tabstop_resource,
                       DISPLAY_DESCR     *dspl_descr);

int       process_some_keydefs(DISPLAY_DESCR     *dspl_descr);

DMC      *key_to_dm(XEvent *event,
                    int     raw_mode,
                    int     isolatin,
                    int     caps_mode,
                    void   *hash_table);

int  install_keydef(char           *the_key,
                    DMC            *dmc,
                    int             update_server,
                    DISPLAY_DESCR  *dspl_descr,
                    DMC            *curr_dm_list);

int   dm_kd(DMC               *dmc,
            int                update_server,
            DISPLAY_DESCR     *dspl_descr);

DMC  *dm_cmdf(DMC             *dmc,
              Display         *display,
              Window           main_x_window,
              char             escape_char,
              void            *hash_table,
              int              prompt_allowed);

void  parse_hashtable_key(char       *keykey, /* input,  hash table entry */
                          char       *cmd,    /* input,  cmd name for error messages */
                          KeySym     *keysym, /* output, X keysym value */ 
                          int        *state,  /* output, State, normal, cntl, etc */
                          int        *event); /* output, KeyPress or KeyRelease */

void  dm_kk(XEvent     *event);

void add_modifier_chars(char            *key_string, /* input  */
                        unsigned int     state,      /* input  */
                        char            *target,     /* output */
                        int              max_target);/* input  */

int  ce_merge(DISPLAY_DESCR *dspl_descr);

DMC   *dm_eval(DMC               *dmc,
               DISPLAY_DESCR     *dspl_descr);

#endif

