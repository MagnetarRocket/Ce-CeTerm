#ifndef _DEFKDS_INCLUDED
#define _DEFKDS_INCLUDED

/* static char *defkds_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Default key definitions in case we cannot find anything else.
*
*  This array of strings can only hold kd command, no other
*  type of command is allowed.  Putting in other types of 
*  commands will cause a program error because the run time
*  code does not look for errors in this .h file.
*
*  The NULL at the end of the array is the delimiter.
*
***************************************************************/

char *default_key_definitions[] = {
    "kd ^t pt;tt;tl ke",
    "kd ^b pb;tb;tr ke",
    "kd TabS thl ke",
    "kd Tab th ke",
    "kd Escape tdm ke",
    "kd F1 dr;echo ke",
    "kd F1S dr;echo -r ke",
    "kd F2 xc ke",
    "kd F2S xc -r ke",
    "kd F3 xd ke",
    "kd F3S xd -r ke",
    "kd F4 xp ke",
    "kd F4S xp -r ke",
    "kd F5 cms;tl;xd -l line_del ke",
    "kd F5S es ' ';ee;dr;tr;xd -l eol_del;tl;tr ke",
    "kd ^F5 dr;/[~a-z0-9$_]/;xd -l word_del ke",
    "kd F6 xp -l line_del ke",
    "kd F6S xp -l eol_del ke",
    "kd ^F6 xp -l word_del ke",
    "kd F7 undo ke",
    "kd F7S redo ke",
    "kd F8 wc ke",
    "kd F8S pw;ro ke",
    "kd ^F8 wc -q ke",
    "kd Delete ed ke",
    "kd BackSpace ee ke",
    "kd Return en ke",
    "kd ^Tab er 09 ke",
    "kd ^m ro ke",
    "kd ^w pw ke",
    "kd ^i ei ke",
    "kd ^y pw;wc -q ke",
    "kd ^n wc -q ke",
    "kd ^r // ke",
    "kd ^u \\\\ ke",
    "kd ^x xd PRIMARY ke",
    "kd ^c cntlc -h 03 PRIMARY ke",
    "kd ^v xp PRIMARY ke",
    "kd ^space abrt ke",
    "kd m1 sic;dr;echo ke",
    "kd m2 xp X ke",
    "kd m3 sic;/[~a-zA-Z._@@-$0-9@@/@@\\~]/dr; \\[~a-zA-Z._@@-$0-9@@/@@\\~]\\;/?/xc cv_file; ti;tl;xd -l junk;es 'cv @@'';xp cv_file;tr;es '@@'';en ke",
    "kd m1u xc X ke; ",
    "kd m1us sic;xc -r X ke; ",
    "kd m2u ke",
    "kd m3u ke",
    "kd Down ad ke",
    "kd Left al ke",
    "kd Right ar ke",
    "kd Up au ke",
    "kd Home tl ke",
    "kd End tr ke",
    "kd Prior pp -0.5 ke",
    "kd Next pp 0.5 ke",
    "kd UpS pv -1 ke",
    "kd DownS pv 1 ke",
    "kd LeftS ph -1 ke",
    "kd RightS ph 1 ke",
    "kd ^Escape tmb ke",

    "mi File/0 'Save         (^w)'pw;msg 'File saved' ke",
    "mi File/1 'Save As'pn -c -r &'New Name:';ro -off;pw ke",
    "mi File/2 'Print'lbl;rd;1,$! -c -m lp;glb ke   # Tailor this line to your print command",
    "mi File/3 'Close/Save   (^y)'wc ke",
    "mi File/4 'Close/NoSave (^n)'wc -q ke",
    "mi File/5 ' ' ke",
    "mi File/6 'Exit'wc -f ke",

    "mi Edit/0 'Copy      (F2)'xc ke",
    "mi Edit/1 'Cut       (F3)'xd ke",
    "mi Edit/2 'Paste     (F4)'xp ke",
    "mi Edit/3 'Undo      (F7)'undo ke",
    "mi Edit/4 'Redo      (F7S)'redo ke",
    "mi Edit/5 'Text Flow (*t)'tf ke",
    "mi Edit/6 'Find Matching Delim'bl -d ke",
    "mi Edit/7 'Uppercase area'case -u ke",

    "mi Modes/0 'Case'pd Case ke",
    "mi Modes/1 'Insert'pd Insert ke",
    "mi Modes/2 'hex'pd Hex ke",
    "mi Modes/3 'Lineno'pd Lineno ke",
    "mi Modes/4 'ScrollBar'pd ScrollBr ke",
    "mi Modes/5 'Vt100'pd vt ke",
    "mi Modes/6 'Track Mouse'pd Mouse ke",
    "mi Modes/7 'Read-Write'ro ke",

    "mi Macro/0 'No Macros Defined' ke",

    "mi Help/0 'Ce'cv -envar y $CEHELPDIR/ce.hlp ke",
    "mi Help/1 'Intro'cv -envar y $CEHELPDIR/.hlp ke",
    "mi Help/2 'Menubar'cv -envar y $CEHELPDIR/menubarCon.hlp ke",
    "mi Help/3 'Commands'cv -envar y $CEHELPDIR/commands.hlp ke",
    "mi Help/4 'options'cv -envar y $CEHELPDIR/xresources.hlp ke",
    "mi Help/5 'whats new'cv -envar y $CEHELPDIR/rlse.hlp ke",

    "mi Case/0 'Sensitive find'sc -on ke",
    "mi Case/1 'Insensitive find'sc -off ke",

    "mi Insert/0 'Insert'ei -on ke",
    "mi Insert/1 'Overstrike'ei -off ke",

    "mi Hex/0 'On'hex -on ke",
    "mi Hex/1 'Off'hex -off ke",

    "mi Lineno/0 'On'lineno -on ke",
    "mi Lineno/1 'Off'lineno -off ke",

    "mi ScrollBr/0 'On'sb -on ke",
    "mi ScrollBr/1 'Off'sb -off ke",
    "mi ScrollBr/2 'Auto'sb -auto ke",

    "mi vt/0 'On'vt -on ke",
    "mi vt/1 'Off'vt -off ke",
    "mi vt/2 'Auto'vt -auto ke",

    "mi Mouse/0 'On'mouse -on ke",
    "mi Mouse/1 'Off'mouse -off ke",

    "mi Menubar/0 'File'pd File ke",
    "mi Menubar/1 'Edit'pd Edit ke",
    "mi Menubar/2 'Modes'pd Modes ke",
    "mi Menubar/3 'Macro'pd Macro ke",

    "alias  help       cv $CEHELPDIR/$1.hlp ke",
    "alias  quit       wc -q ke",
    "alias  exit       wc -q ke",
    "alias  end        wc    ke",
    "alias  save       pw    ke",
    "alias  top        pt;tt;tl ke",
    "alias  bottom     pb;tb;tr ke",

    NULL};

#endif
