#ifndef _PARMS_INCLUDED
#define _PARMS_INCLUDED

/* static char *parms_h_sccsid = "%Z% %M% %I% - %G% %U% "; */

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
*  Include parms.h
*
*  Defintion of parm string variables and the parsing
*  structures.
*
*  These variables are used globally.  They contain data
*  collected by the main program from the parm list and
*  .Xdefaults type data.
*
**************************************************************
**  NOTE:  If we ever add an option which makes "-ve" not
**  uniquely identify -version, execute.c must be updated
**  to check for the minimum unique substring of -version as
**  the special case for "ce -ver".
**************************************************************
*
***************************************************************/

#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#include "Xresource.h"         /* Local Copy  WIN32ONLY */
#else
#include "X11/Xlib.h"          /* "/usr/include/X11/Xlib.h"      */
#include "X11/Xresource.h"     /* "/usr/include/X11/Xresource.h" */
#endif
#else
#include <X11/Xlib.h>          /* "/usr/include/X11/Xlib.h"      */
#include <X11/Xresource.h>     /* "/usr/include/X11/Xresource.h" */
#endif


#ifdef WIN32
#define OPTION_COUNT 72
#else
#define OPTION_COUNT 69
#endif

#ifdef _MAIN_

/***************************************************************
*  
*  The following is the Xlib description of a parm list.
*  <X11/Xresource.h> must be included, but only in the main.
*  
***************************************************************/

XrmOptionDescRec cmd_options[OPTION_COUNT] = {
/* option             resource name                   type                  used only if type is XrmoptionNoArg */

{"-background",     ".background",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  0   */
{"-bgc",            ".background",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  1   */
{"-display",        ".internaldpy",                 XrmoptionSepArg,        (caddr_t) NULL},    /*  2   */
{"-fgc",            ".foreground",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  3   */
{"-font",           ".font",                        XrmoptionSepArg,        (caddr_t) NULL},    /*  4   */
{"-foreground",     ".foreground",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  5   */
{"-geometry",       ".geometry",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  6   */
{"-sc",             ".caseSensitive",               XrmoptionSepArg,        (caddr_t) NULL},    /*  7   */
{"-bkuptype",       ".bkuptype",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  8   */
{"-lineno",         ".lineno",                      XrmoptionSepArg,        (caddr_t) NULL},    /*  9   */
{"-nb",             ".bkuptype",                    XrmoptionNoArg,         (caddr_t) "none"},  /*  10  */
{"-expr",           ".expr",                        XrmoptionSepArg,        (caddr_t) NULL},    /*  11  */
{"-w",              ".internalw",                   XrmoptionNoArg,         (caddr_t) "yes"},   /*  12  */
{"-iconic",         ".iconic",                      XrmoptionNoArg,         (caddr_t) "yes"},   /*  13  */
{"-pbd",            ".pasteBuffDir",                XrmoptionSepArg,        (caddr_t) NULL},    /*  14  */
{"-autosave",       ".autosave",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  15  */
{"-version",        ".internalver",                 XrmoptionNoArg,         (caddr_t) "none"},  /*  16  */
{"-help",           ".internalhlp",                 XrmoptionNoArg,         (caddr_t) "none"},  /*  17  */
{"-padding",        ".padding",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  18  */
{"-internalk",      ".cekeys",                      XrmoptionSepArg,        (caddr_t) NULL},    /*  19  */
{"-tabstops",       ".tabstops",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  20  */
{"-load",           ".internalload",                XrmoptionNoArg,         (caddr_t) "load"},  /*  21  */
{"-reload",         ".internalreload",              XrmoptionNoArg,         (caddr_t) "reload"},/*  22 ref in getxopts */
{"-man",            ".man",                         XrmoptionNoArg,         (caddr_t) "yes"},   /*  23  */
{"-isolatin",       ".isolatin",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  24  */
{"-cmd",            ".cmd",                         XrmoptionSepArg,        (caddr_t) NULL},    /*  25  */
{"-mouse",          ".mouse",                       XrmoptionSepArg,        (caddr_t) NULL},    /*  26  */
{"-autoclose",      ".autoclose",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  27  */
{"-scroll",         ".scroll",                      XrmoptionSepArg,        (caddr_t) NULL},    /*  28  */
{"-autohold",       ".autohold",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  29  */
{"-stdin",          ".internalstdin",               XrmoptionNoArg,         (caddr_t) "yes"},   /*  30  */
{"-title",          ".title",                       XrmoptionSepArg,        (caddr_t) NULL},    /*  31  */
{"-ut",             ".utmpInhibit",                 XrmoptionNoArg,         (caddr_t) "True"},  /*  32  */
{"-DebuG",          ".DebuG",                       XrmoptionSepArg,        (caddr_t) NULL},    /*  33  */
{"-wmadjx",         ".wmAdjustX",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  34  */
{"-wmadjy",         ".wmAdjustY",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  35  */
{"-noreadahead",    ".noreadahead",                 XrmoptionNoArg,         (caddr_t) "yes"},   /*  36  */
{"-name",           ".internalname",                XrmoptionSepArg,        (caddr_t) NULL},    /*  37  */
{"-vt",             ".vt",                          XrmoptionSepArg,        (caddr_t) NULL},    /*  38  */
{"-ls",             ".loginShell",                  XrmoptionNoArg,         (caddr_t) "yes"},   /*  39  */
{"-readlock",       ".internalrl",                  XrmoptionNoArg,         (caddr_t) "yes"},   /*  40  */
{"-envar",          ".envar",                       XrmoptionSepArg,        (caddr_t) NULL},    /*  41  */
{"-ib",             ".iconBitmap",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  42  */
{"-dpb",            ".dfltPasteBuf",                XrmoptionSepArg,        (caddr_t) NULL},    /*  43  */
{"-findbrdr",       ".findbrdr",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  44  */
{"-termtype",       ".termtype",                    XrmoptionSepArg,        (caddr_t) NULL},    /*  45  */
{"-transpad",       ".internaltp",                  XrmoptionNoArg,         (caddr_t) "yes"},   /*  46  */
{"-offdspl",        ".offdspl",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  47  */
{"-dotmode",        ".dotmode",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  48  */
{"-lockf",          ".lockf",                       XrmoptionSepArg,        (caddr_t) NULL},    /*  49  */
{"-tbf",            ".titlebarfont",                XrmoptionSepArg,        (caddr_t) NULL},    /*  50  */
{"-kdp",            ".keydefProp",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  51  */
{"-sb",             ".scrollBar",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  52  */
{"-sbcolors",       ".scrollBarColors",             XrmoptionSepArg,        (caddr_t) NULL},    /*  53  */
{"-sbwidth",        ".scrollBarWidth",              XrmoptionSepArg,        (caddr_t) NULL},    /*  54  */
{"-oplist",         ".internalOP",                  XrmoptionNoArg,         (caddr_t) "yes"},   /*  55  */
{"-xrm",            ".internalXRM",                 XrmoptionResArg,        (caddr_t) NULL},    /*  56  */
{"-pdm",            ".pdm",                         XrmoptionSepArg,        (caddr_t) NULL},    /*  57  */
{"-CEHELPDIR",      ".CEHELPDIR",                   XrmoptionSepArg,        (caddr_t) NULL},    /*  58  */
{"-mousecursor",    ".mouseCursor",                 XrmoptionSepArg,        (caddr_t) NULL},    /*  59  */
{"-lsf",            ".lsf",                         XrmoptionSepArg,        (caddr_t) NULL},    /*  60  */
{"-linemax",        ".linemax",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  61  */
{"-autocut",        ".autocut",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  62  */
{"-XSync",          ".XSynchronize",                XrmoptionSepArg,        (caddr_t) NULL},    /*  63  */
{"-DELAY",          ".internalDL",                  XrmoptionSepArg,        (caddr_t) NULL},    /*  64  */
{"-vcolors",        ".vcolors",                     XrmoptionSepArg,        (caddr_t) NULL},    /*  65  */
{"-bell",           ".bell",                        XrmoptionSepArg,        (caddr_t) "yes"},   /*  66  */
{"-sm_client_id",   ".internalSM_CLIENT_ID",        XrmoptionSepArg,        (caddr_t) NULL},    /*  67  */
{"-ws",             ".internalWorkspaceNum",        XrmoptionSepArg,        (caddr_t) NULL},    /*  68  */
#ifdef WIN32
{"-browse",         ".internalBROWSE",              XrmoptionNoArg,         (caddr_t) "yes"},   /*  69  */
{"-edit",           ".internalEDIT",                XrmoptionNoArg,         (caddr_t) "yes"},   /*  70  */
{"-term",           ".internalTERM",                XrmoptionNoArg,         (caddr_t) "yes"},   /*  71  */
#endif
};

char  *defaults[OPTION_COUNT] = {
             NULL,            /* 0  default for background, filled in in program */
             NULL,            /* 1  irrelevant, second background value */
             NULL,            /* 2  default for display */
             NULL,            /* 3  default for foreground, filled in in program */
             "fixed",         /* 4  default for font */
             NULL,            /* 5  irrelevant, second foreground value */
             NULL,            /* 6  default for geometry  */
             NULL,            /* 7  default for case sensitivity */
             "dm",            /* 8  default for creating .bak files, values dm, vi, or none */
             "no",            /* 9  default for lineno (displaying line numbers) */
             NULL,            /* 10 irrelevant, second backup type value */
             "aegis",         /* 11 default for regular expression type, values aegis or unix */
             "no",            /* 12 wait mode (stay in the command window process */
             NULL,            /* 13 iconic, (start as an icon) */
             "~/.CePaste",    /* 14 default for paste buffer directory */
             NULL,            /* 15 default for autosave */
             NULL,            /* 16 release version number */
             NULL,            /* 17 put up help file */
             NULL,            /* 18 padding */
             NULL,            /* 19 Location of site wide key definitions, loaded before .Cekeys */
             NULL,            /* 20 default tab stop command */
             NULL,            /* 21 default -load value */
             NULL,            /* 22 default -reload value */
             NULL,            /* 23 default -man value */
             NULL,            /* 24 default -isolatin value */
             NULL,            /* 25 default -cmd */
             "on",            /* 26 default -mouse (mouse engaged) on or off (check is on second letter of on ) */
             "no",            /* 27 default -autoclose ceterm windows, yes and no */
             "no",            /* 28 default -scroll in ceterm windows, yes and no */
             "no",            /* 29 default -autohold in ceterm windows, yes and no */
             "no",            /* 30 default -stdin, no alternate source of dm commands */
             NULL,            /* 31 default -title for Motif title, default is prog name and version */
             NULL,            /* 32 default -ut parm, True inhibits putting an entry in the utmp table */
             NULL,            /* 33 default -DebuG hidden option */
             NULL,            /* 34 default -wmadjx, window manager adjust X value */
             NULL,            /* 35 default -wmadjy, window manager adjust Y value */
             NULL,            /* 36 default -noreadahead, turn off reading whole file in background */
             NULL,            /* 37 default -name, use argc */
             NULL,            /* 38 default -vt, on, off, auto, automatically switch to vt100 mode based on echo tty bit, yes/no */
             NULL,            /* 39 default -ls start shell as a login shell, yes/no */
             NULL,            /* 40 default readlock, lock into read mode */
             NULL,            /* 41 default -envar, environment variabler processing on */
             NULL,            /* 42 default -ib, icon bitmap, defaults to default icon */
             "CLIPBOARD",     /* 43 default paste buffer name, defaults to CLIPBOARD */
             NULL,            /* 44 default find border (0) */
             NULL,            /* 45 default -termtype default becomes vt100 */
             "no",            /* 46 default -transpad, stdin should not be treated as transcript pad input */
             NULL,            /* 47 default -offdspl, allow geometries which push the window totally off the display */
             "2",             /* 48 default -dotmode, default is not suppress dot mode */
             NULL,            /* 49 default -lockf, default is file locking on (yes) */
             "fixed",         /* 50 default -tbf, default titlebar font */
             "CeKeys",        /* 51 default -kdp, default key def property */
             "auto",          /* 52 default -sb, scroll bars yes/no/auto */
             NULL,            /* 53 default -sbcolors, "color color color color" */
             "256",           /* 54 default -sbwidth, "number" */
             "no",            /* 55 default -oplist, default is not to print optins and quit  */
             NULL,            /* 56 default -xrm, additional resources  */
             "yes",           /* 57 default -pdm pull down menu bar on, yes and no */
             NULL,            /* 58 default -CEHELPDIR, picked up from app-defaults set up by ce_install  */
             NULL,            /* 69 default .mouseCursor, 1 or 2 numeric values from picked up from cursorfont.h 1st main cursor, 2nd pd cursor  */
             NULL,            /* 70 default .lsf, Default no  */
             NULL,            /* 61 default .autocut, Default is no  */
             NULL,            /* 62 default .linemax, Default 0  */
             NULL,            /* 63 default .XSynchronize, Default no  */
             NULL,            /* 64 default -DELAY default */
             NULL,            /* 65 default -vtcolors default, actual defaults in the parm routine in winsetup.c */
             "yes",           /* 66 default -bell, normal active bell, supports 'n' for no bell and 'v' for visual */
             NULL,            /* 67 used by XSMP (X Session Manager) for restarting */
             NULL,            /* 68 default None, workspace to start in */
#ifdef WIN32
             "no",            /* 69 default -browse, default is not browse  */
             "no",            /* 70 default -edit, default is not edit  */
             "no",            /* 71 default -term, default is not term, figure out from name  */
#endif
                  };

/*char                *option_values[OPTION_COUNT];*/

char                *cmdname;
#ifdef WIN32
char                *cmd_path;
#endif

#else
/***************************************************************
*  
*  Variables visible outside the main.
*  
***************************************************************/

extern XrmOptionDescRec cmd_options[OPTION_COUNT];

extern char  *defaults[OPTION_COUNT];

extern char         *cmdname;
#ifdef WIN32
extern char         *cmd_path;
#endif

#endif  /* not main */

/***************************************************************
*  
*  Indexes into the option_values array for various parm options.
*  
***************************************************************/

#define BACKGROUND_IDX  0
#define DISPLAY_IDX     2
#define FOREGROUND_IDX  3
#define FONT_IDX        4
#define GEOMETRY_IDX    6
#define CASE_IDX        7
#define BACKUP_IDX      8
#define LINENO_IDX      9
#define EXPR_IDX        11
#define WAIT_IDX        12
#define ICONIC_IDX      13
#define PASTE_DIR_IDX   14
#define AUTOSAVE_IDX    15
#define VERSION_IDX     16
#define HELP_IDX        17
#define PADDING_IDX     18
#define KEYDEF_IDX      19
#define TABSTOP_IDX     20
#define LOAD_IDX        21
#define RELOAD_IDX      22
#define MAN_IDX         23
#define ISOLATIN_IDX    24
#define CMD_IDX         25
#define MOUSE_IDX       26
#define AUTOCLOSE_IDX   27
#define SCROLL_IDX      28
#define AUTOHOLD_IDX    29
#define STDIN_IDX       30
#define TITLE_IDX       31
#define UT_IDX          32
#define DebuG_IDX       33
#define WMADJX_IDX      34
#define WMADJY_IDX      35
#define READAHEAD_IDX   36
#define NAME_IDX        37
#define VT_IDX          38
#define LS_IDX          39
#define READLOCK_IDX    40
#define ENVAR_IDX       41
#define IB_IDX          42
#define DPB_IDX         43
#define FINDBRDR_IDX    44
#define TERMTYPE_IDX    45
#define TRANSPAD_IDX    46
#define OFFDSPL_IDX     47
#define DOTMODE_IDX     48
#define LOCKF_IDX       49
#define TBF_IDX         50
#define KDP_IDX         51
#define SB_IDX          52
#define SBCOLOR_IDX     53
#define SBWIDTH_IDX     54
#define OPLIST_IDX      55
#define XRM_IDX         56
#define PDM_IDX         57
#define CEHELPDIR_IDX   58
#define MOUSECURSOR_IDX 59
#define LSF_IDX         60
#define LINEMAX_IDX     61
#define AUTOCUT_IDX     62
#define XSYNC_IDX       63
#define DELAY_IDX       64
#define VTCOLORS_IDX    65
#define BELL_IDX        66
#define SM_CLIENT_IDX   67
#define WS_IDX          68
#ifdef WIN32
#define BROWSE_IDX      69
#define EDIT_IDX        70
#define TERM_IDX        71
#endif

#define OPTION_VALUES     dspl_descr->option_values
#define OPTION_FROM_PARM  dspl_descr->option_from_parm

#define BACKGROUND_CLR (OPTION_VALUES[BACKGROUND_IDX])
#define DISPLAY_NAME   (OPTION_VALUES[DISPLAY_IDX])
#define FOREGROUND_CLR (OPTION_VALUES[FOREGROUND_IDX])
#define FONT_NAME      (OPTION_VALUES[FONT_IDX])
#define GEOMETRY       (OPTION_VALUES[GEOMETRY_IDX])
#define CASE_SENSITIVE (OPTION_VALUES[CASE_IDX] && ((OPTION_VALUES[CASE_IDX][0] | 0x20) == 'y'))
#define BACKUP_TYPE    (OPTION_VALUES[BACKUP_IDX])
#define LINENO_PARM    (OPTION_VALUES[LINENO_IDX])
#define UNIXEXPR       (OPTION_VALUES[EXPR_IDX] && (((OPTION_VALUES[EXPR_IDX][0] | 0x20) == 'u')  || (OPTION_VALUES[EXPR_IDX][0] == '1')))
#define UNIXEXPR_ERR   (OPTION_VALUES[EXPR_IDX] && (OPTION_VALUES[EXPR_IDX][0] == '1'))
#define WAIT_PARM      (OPTION_VALUES[WAIT_IDX] && ((OPTION_VALUES[WAIT_IDX][0] | 0x20) == 'y'))
#define START_ICONIC   (OPTION_VALUES[ICONIC_IDX] && ((OPTION_VALUES[ICONIC_IDX][0] | 0x20) == 'y'))
#define PASTE_DIR      (OPTION_VALUES[PASTE_DIR_IDX])
#define AUTOSAVE       (OPTION_VALUES[AUTOSAVE_IDX])
#define VERSION_PARM   (OPTION_VALUES[VERSION_IDX])
#define HELP_PARM      (OPTION_VALUES[HELP_IDX])
#define LINE_PADDING   (OPTION_VALUES[PADDING_IDX])
#define APP_KEYDEF     (OPTION_VALUES[KEYDEF_IDX])
#define TABSTOPS       (OPTION_VALUES[TABSTOP_IDX])
#define LOAD_PARM      (OPTION_VALUES[LOAD_IDX])
#define RELOAD_PARM    (OPTION_VALUES[RELOAD_IDX]) 
#define MANFORMAT      (OPTION_VALUES[MAN_IDX] && ((OPTION_VALUES[MAN_IDX][0] | 0x20) == 'y'))
#define ISOLATIN       (OPTION_VALUES[ISOLATIN_IDX] && ((OPTION_VALUES[ISOLATIN_IDX][0] | 0x20) == 'y'))
#define CMDS_FROM_ARG  (OPTION_VALUES[CMD_IDX])
#define MOUSEON        (OPTION_VALUES[MOUSE_IDX] && ((OPTION_VALUES[MOUSE_IDX][1] | 0x20) == 'n'))
#define AUTOCLOSE      (OPTION_VALUES[AUTOCLOSE_IDX] && ((OPTION_VALUES[AUTOCLOSE_IDX][0] | 0x20) == 'y'))
#define SCROLL         (OPTION_VALUES[SCROLL_IDX] && ((OPTION_VALUES[SCROLL_IDX][0] | 0x20) == 'y'))
#define AUTOHOLD       (OPTION_VALUES[AUTOHOLD_IDX] && ((OPTION_VALUES[AUTOHOLD_IDX][0] | 0x20) == 'y'))
#define STDIN_CMDS     (OPTION_VALUES[STDIN_IDX] && ((OPTION_VALUES[STDIN_IDX][0] | 0x20) == 'y'))
#define WM_TITLE       (OPTION_VALUES[TITLE_IDX])
#define MAKE_UTMP      (!OPTION_VALUES[UT_IDX] || ((OPTION_VALUES[UT_IDX][0] | 0x20) == 'f') || (OPTION_VALUES[UT_IDX][0] == '0'))
#define DebuG_PARM     (OPTION_VALUES[DebuG_IDX])
#define WMADJX_PARM    (OPTION_VALUES[WMADJX_IDX])
#define WMADJY_PARM    (OPTION_VALUES[WMADJY_IDX])
#define READAHEAD      ((!OPTION_VALUES[READAHEAD_IDX]) || ((OPTION_VALUES[READAHEAD_IDX][0] | 0x20) != 'y'))
#define RESOURCE_NAME  (OPTION_VALUES[NAME_IDX]) 
#define AUTOVT         (OPTION_VALUES[VT_IDX] && ((OPTION_VALUES[VT_IDX][0] | 0x20) == 'a'))
#define VTMODE         (OPTION_VALUES[VT_IDX] && ((OPTION_VALUES[VT_IDX][1] | 0x20) == 'n'))
#define LOGINSHELL     (OPTION_VALUES[LS_IDX] && ((OPTION_VALUES[LS_IDX][0] | 0x20) == 'y'))
#define READLOCK       (OPTION_VALUES[READLOCK_IDX] && ((OPTION_VALUES[READLOCK_IDX][0] | 0x20) == 'y'))
#define ENV_VARS       (!(OPTION_VALUES[ENVAR_IDX] && ((OPTION_VALUES[ENVAR_IDX][0] | 0x20) == 'n')))
#define ICON_BITMAP    (OPTION_VALUES[IB_IDX])
#define LSHOST         (OPTION_VALUES[LSHOST_IDX])
#define DFLT_PASTE     (OPTION_VALUES[DPB_IDX])
#define FINDBRDR       (OPTION_VALUES[FINDBRDR_IDX])
#define TERMTYPE       (OPTION_VALUES[TERMTYPE_IDX])
#define TRANSPAD       (OPTION_VALUES[TRANSPAD_IDX] && ((OPTION_VALUES[TRANSPAD_IDX][0] | 0x20) == 'y'))
#define OFFDSPL        (OPTION_VALUES[OFFDSPL_IDX] && ((OPTION_VALUES[OFFDSPL_IDX][0] | 0x20) == 'y'))
#define DOTMODE        (OPTION_VALUES[DOTMODE_IDX])
#define NODOTS         (OPTION_VALUES[DOTMODE_IDX] && (OPTION_VALUES[DOTMODE_IDX][0] == '0'))
#define LOCKF          ((!OPTION_VALUES[LOCKF_IDX]) || ((OPTION_VALUES[LOCKF_IDX][0] | 0x20) == 'y'))
#define TITLEBARFONT   (OPTION_VALUES[TBF_IDX])
#define KEYDEF_PROP    (OPTION_VALUES[KDP_IDX])
#define SCROLLBARS     (OPTION_VALUES[SB_IDX])
#define SBCOLORS       (OPTION_VALUES[SBCOLOR_IDX])
#define SBWIDTH        (OPTION_VALUES[SBWIDTH_IDX])
#define OPLIST         (OPTION_VALUES[OPLIST_IDX] && ((OPTION_VALUES[OPLIST_IDX][0] | 0x20) == 'y'))
#define XRM            NULL /* -xrm handled by XrmParseCommand */
#define PDM            ((!OPTION_VALUES[PDM_IDX]) || ((OPTION_VALUES[PDM_IDX][0] | 0x20) != 'n'))
#define CEHELPDIR      (OPTION_VALUES[CEHELPDIR_IDX])
#define MOUSECURSOR    (OPTION_VALUES[MOUSECURSOR_IDX])
#define LSF            (OPTION_VALUES[LSF_IDX] && ((OPTION_VALUES[LSF_IDX][0] | 0x20) == 'y'))
#define LINEMAX        (OPTION_VALUES[LINEMAX_IDX])
#define AUTOCUT        (OPTION_VALUES[AUTOCUT_IDX] && ((OPTION_VALUES[AUTOCUT_IDX][0] | 0x20) == 'y'))
#define XSYNCHRONIZE   (OPTION_VALUES[XSYNC_IDX] && ((OPTION_VALUES[XSYNC_IDX][0] | 0x20) == 'y'))
#define DELAY_PARM     (OPTION_VALUES[DELAY_IDX])
#define VTCOLORS_PARM  (OPTION_VALUES[VTCOLORS_IDX])
#define VISUAL_BELL    (OPTION_VALUES[BELL_IDX] && ((OPTION_VALUES[BELL_IDX][0] | 0x20) == 'v'))
#define VISUAL_BELL_UC (OPTION_VALUES[BELL_IDX] && (OPTION_VALUES[BELL_IDX][0] == 'V'))
#define SUPPRESS_BELL  (OPTION_VALUES[BELL_IDX] && ((OPTION_VALUES[BELL_IDX][0] | 0x20) == 'n'))
#define SM_CLIENT_ID   (OPTION_VALUES[SM_CLIENT_IDX])
#define WS_NUM         (OPTION_VALUES[WS_IDX])
#ifdef WIN32
#define BROWSE_MODE    (OPTION_VALUES[BROWSE_IDX] && ((OPTION_VALUES[BROWSE_IDX][0] | 0x20) == 'y'))
#define EDIT_MODE      (OPTION_VALUES[EDIT_IDX] && ((OPTION_VALUES[EDIT_IDX][0] | 0x20) == 'y'))
#define TERM_MODE      (OPTION_VALUES[TERM_IDX] && ((OPTION_VALUES[TERM_IDX][0] | 0x20) == 'y'))
#endif

#endif

