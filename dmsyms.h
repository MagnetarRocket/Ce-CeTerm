#ifndef _DMSYMS_INCLUDED
#define _DMSYMS_INCLUDED

/* static char *dmsyms_h_sccsid = "%Z% %M% %I% - %G% %U% ";  */

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
*  Table with definitions of DM commands
*
*  Table dmsyms      - It is an array of type DMSYM indexed by
*                      the defined DM_<name> values.
*
*  FIELDS:
*
*        name           - (pointer to char)
*                         This is the text name of the command.  It is
*                         used in parsedm.c to parse the command and
*                         elsewhere in error message generation.  dump_kd
*                         uses it to regenerate the DM commands.
*
*        supported      - (char)
*                         This flag is true (1) for supported DM commands.
*                         Some DM commands are recognized, but not supported.
*                         Used only in parsedm.c
*
*        modifies_buff  - (char)
*                         This flag is true if the command modifies the work
*                         buffer.  Used only in main event loop in main.
*
*        needs_flush    - (char)
*                         This flag is true if the command needs
*                         the work buffer to be written back to to the memory file
*                         before execution.  An example of this is the find command,
*                         which searches in the memory file data.  Any changes in
*                         the work buffer must be flushed back before the change occurs.
*                         Used only in main event loop in main.
*
*        special_delim  - (char)
*                         This flag is true if the command requires special processing
*                         in finding the end of the command.  This is true of commands
*                         such as find.  This field is used only in parsedm.c
*                         This flag is a char to preserve word alignment.
*
*        uses_cursor_pos - (char)
*                         This command indicates that the command operates over a marked
*                         range of text or targets the current cursor position.
*
*        vt100_ok       - (char)
*                         This flag has 3 values:
*                         VT100_OK    -  command is ok in vt100 mode
*                         VT100_DM    -  command is ok in vt100 mode only in the dminput window
*                         VT100_NEVER -  command is never ok in vt100 mode
*
*        autocut        - (char)
*                         This flag is true for commands, which, when the only command in
*                         a key definition will cause a highlighted area to be cut automatically
*                         like on the Netscape editor or a windows editor.  The X resource
*                         autocut must be turned on to activate this feature.
*                         This flag has 3 values:
*                         0           -  False
*                         AUTOCUT_DEL -  Command is a remove, like ee and ed
*                         AUTOCUT_ADD -  Command adds text like er, es, and xp
**
*
*  Note: array size is:  DM_MAXDEF
*
*  Note: when you add a command, you must make updates in:
*  parsedm.c - add any needed parsing, free code, and print code
*  serverdef.c - add load and unload code
*  cswitch.c   - add to switch on command id.
*  dmc.h       - add structure DMC_xx for the command and put in the union.
*
***************************************************************/



/***************************************************************
*
*  Defines for the DM keywords for use in switch statements.
*
***************************************************************/

#define  DM_NULL          0
#define  DM_find          1
#define  DM_rfind         2
#define  DM_markp         3
#define  DM_markc         4
#define  DM_num           5
#define  DM_aa            6
#define  DM_abrt          7
#define  DM_ad            8
#define  DM_al            9
#define  DM_ap            10
#define  DM_ar            11
#define  DM_as            12
#define  DM_au            13
#define  DM_bgc           14
#define  DM_bl            15
#define  DM_case          16
#define  DM_cc            17
#define  DM_cdm           18
#define  DM_ce            19
#define  DM_cmdf          20
#define  DM_cms           21
#define  DM_cp            22
#define  DM_cpb           23
#define  DM_cpo           24
#define  DM_cps           25
#define  DM_curs          26
#define  DM_cv            27
#define  DM_dc            28
#define  DM_dq            29
#define  DM_dr            30
#define  DM_ds            31
#define  DM_echo          32
#define  DM_ed            33
#define  DM_ee            34
#define  DM_eef           35
#define  DM_ei            36
#define  DM_en            37
#define  DM_env           38
#define  DM_er            39
#define  DM_es            40
#define  DM_ex            41
#define  DM_fl            42
#define  DM_gm            43
#define  DM_icon          44
#define  DM_idf           45
#define  DM_inv           46
#define  DM_kbd           47
#define  DM_kd            48
#define  DM_l             49
#define  DM_mono          50
#define  DM_msg           51
#define  DM_pb            52
#define  DM_ph            53
#define  DM_pn            54
#define  DM_pp            55
#define  DM_pt            56
#define  DM_pv            57
#define  DM_pw            58
#define  DM_rm            59
#define  DM_ro            60
#define  DM_rs            61
#define  DM_rw            62
#define  DM_s             63
#define  DM_sc            64
#define  DM_shut          65
#define  DM_so            66
#define  DM_sq            67
#define  DM_tb            68
#define  DM_tdm           69
#define  DM_th            70
#define  DM_thl           71
#define  DM_ti            72
#define  DM_tl            73
#define  DM_tlw           74
#define  DM_tn            75
#define  DM_tni           76
#define  DM_tr            77
#define  DM_ts            78
#define  DM_tt            79
#define  DM_twb           80
#define  DM_undo          81
#define  DM_wa            82
#define  DM_wc            83
#define  DM_wdf           84
#define  DM_wg            85
#define  DM_wge           86
#define  DM_wgra          87
#define  DM_wgrr          88
#define  DM_wh            89
#define  DM_wi            90
#define  DM_wm            91
#define  DM_wme           92
#define  DM_wp            93
#define  DM_ws            94
#define  DM_xc            95
#define  DM_xd            96
#define  DM_xi            97
#define  DM_xp            98
#define  DM_equal         99
#define  DM_kk            100
#define  DM_comma         101
#define  DM_dollar        102
#define  DM_redo          103  /* new command in this editor */
#define  DM_lineno        104  /* new command in this editor */
#define  DM_geo           105  /* new command in this editor */
#define  DM_prompt        106  /* internal structure for prompts      */
#define  DM_tdmo          107
#define  DM_debug         108  /* debugging aid */
#define  DM_delsd         109  /* new command, delete X server copy definitions */
#define  DM_fgc           110
#define  DM_keys          111
#define  DM_tf            112
#define  DM_ww            113
#define  DM_bang          114
#define  DM_hex           115
#define  DM_re            116
#define  DM_untab         117
#define  DM_cd            118
#define  DM_sic           119
#define  DM_vt            120
#define  DM_sleep         121
#define  DM_tmw           122
#define  DM_lkd           123
#define  DM_ceterm        124
#define  DM_rd            125
#define  DM_pwd           126
#define  DM_wdc           127
#define  DM_mouse         128
#define  DM_rec           129
#define  DM_bell          130
#define  DM_title         131
#define  DM_envar         132
#define  DM_alias         133
#define  DM_sp            134
#define  DM_prefix        135
#define  DM_xl            136
#define  DM_colon         137
#define  DM_ca            138
#define  DM_corner        139
#define  DM_fbdr          140
#define  DM_sb            141
#define  DM_mi            142
#define  DM_lmi           143
#define  DM_pd            144
#define  DM_pdm           145
#define  DM_tmb           146
#define  DM_nc            147
#define  DM_pc            148
#define  DM_eval          149
#define  DM_ind           150
#define  DM_lsf           151
#define  DM_lbl           152
#define  DM_glb           153
#define  DM_dlb           154
#define  DM_xa            155
#define  DM_f             156 /* becomes DM_find in the parser */
#define  DM_caps          157
#define  DM_sl            158
#define  DM_rl            159
#define  DM_reload        160
#define  DM_cntlc         161


#define  DM_MAXDEF        162


/***************************************************************
*  
*  Definition of a DMSYM values for field vt100_ok
*  We only define the command valid check if the main window
*  define is present. Otherwise we will not need it.
*  (Really it is only used in crpad.c)
*  
***************************************************************/

#define VT100_OK     (unsigned char)1
#define VT100_DM     (unsigned char)0
#define VT100_NEVER  (unsigned char)-1

#ifdef DMINPUT_WINDOW
#define VT100_CMD_VALID(winid, cmdid) \
        ((dmsyms[cmdid].vt100_ok == VT100_OK) || ((winid == DMINPUT_WINDOW) || (winid == DMOUTPUT_WINDOW)) && (dmsyms[cmdid].vt100_ok == VT100_DM))
#endif

/***************************************************************
*  
*  Values for the autocut field
*  
***************************************************************/

/* False or 0 is also valid */
#define AUTOCUT_DEL  (unsigned char)1
#define AUTOCUT_ADD  (unsigned char)2

/***************************************************************
*  
*  Description of a dm symbol entry.
*  This structure holds the data relevant to each command.  It
*  is index by the defined DM_<name> symbols.
*  
***************************************************************/

typedef struct {
   char               *name;
   unsigned char       supported;        /* Command is supported                               */
   unsigned char       modifies_buff;    /* Command modifies the work buffer                   */
   unsigned char       needs_flush;      /* Needs work buffer flushed before running           */
   unsigned char       special_delim;    /* Uses special delimiter when being parsed           */
   unsigned char       uses_cursor_pos ; /* Command operates agaist a marked region of text    */
   unsigned char       autocut;          /* Cut highlighted area for this command              */
   unsigned char       vt100_ok;         /* Command allowed against the main pad in vt100 mode */
} DMSYM;




#ifdef _MAIN_

/***************************************************************
*
*  External variable with the escape character in it.
*
***************************************************************/

DMSYM      dmsyms[DM_MAXDEF] = {

/*  name      supported   modifies     needs    special   cursor    autocut    vt100                                 */
/*                          buff       flush     delim     pos                  ok                                   */
                                                                             
  { "",          1,         0,           0,        1,       0,        0,       VT100_OK    },  /*  0     DM_NULL        */
  { "/",         1,         0,           1,        1,       0,        0,       VT100_DM    },  /*  1     DM_find        */
  { "\\",        1,         0,           1,        1,       0,        0,       VT100_DM    },  /*  2     DM_rfind       */
  { "(",         1,         0,           0,        1,       0,        0,       VT100_DM    },  /*  3     DM_markp       */
  { "[",         1,         0,           1,        1,       0,        0,       VT100_DM    },  /*  4     DM_markc       */
  { "",          1,         0,           1,        1,       0,        0,       0           },  /*  5     DM_num         */
  { "aa",        0,         0,           0,        0,       0,        0,       0           },  /*  6     DM_aa          */
  { "abrt",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  7     DM_abrt        */
  { "ad",        1,         0,           1,        0,       1,        0,       VT100_DM    },  /*  8     DM_ad          */
  { "al",        1,         0,           0,        0,       1,        0,       VT100_DM    },  /*  9     DM_al          */
  { "ap",        0,         0,           0,        0,       0,        0,       0           },  /*  10    DM_ap          */
  { "ar",        1,         0,           0,        0,       1,        0,       VT100_DM    },  /*  11    DM_ar          */
  { "as",        0,         0,           0,        0,       0,        0,       0           },  /*  12    DM_as          */
  { "au",        1,         0,           1,        0,       1,        0,       VT100_DM    },  /*  13    DM_au          */
  { "bgc",       1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  14    DM_bgc         */
  { "bl",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  15    DM_bl          */
  { "case",      1,         1,           1,        0,       1,        0,       VT100_DM    },  /*  16    DM_case        */
  { "cc",        1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  17    DM_cc          */
  { "cdm",       0,         0,           0,        0,       0,        0,       0           },  /*  18    DM_cdm         */
  { "ce",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  19    DM_ce          */
  { "cmdf",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  20    DM_cmdf        */
  { "cms",       1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  21    DM_cms         */
  { "cp",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  22    DM_cp          */
  { "cpb",       0,         0,           0,        0,       0,        0,       0           },  /*  23    DM_cpb         */
  { "cpo",       1,         0,           0,        0,       0,        0,       0           },  /*  24    DM_cpo         */
  { "cps",       1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  25    DM_cps         */
  { "curs",      0,         0,           0,        0,       0,        0,       0           },  /*  26    DM_curs        */
  { "cv",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  27    DM_cv          */
  { "dc",        0,         0,           0,        0,       0,        0,       0           },  /*  28    DM_dc          */
  { "dq",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  29    DM_dq          */
  { "dr",        1,         0,           1,        0,       1,        0,       VT100_OK    },  /*  30    DM_dr          */
  { "ds",        0,         0,           0,        0,       0,        0,       0           },  /*  31    DM_ds          */
  { "echo",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  32    DM_echo        */
  { "ed",        1,         1,           0,        0,       1,    AUTOCUT_DEL, VT100_DM    },  /*  33    DM_ed          */
  { "ee",        1,         1,           0,        0,       1,    AUTOCUT_DEL, VT100_DM    },  /*  34    DM_ee          */
  { "eef",       1,         1,           1,        0,       0,        0,       VT100_NEVER },  /*  35    DM_eef         */
  { "ei",        1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  36    DM_ei          */
  { "en",        1,         1,           0,        0,       1,        0,       VT100_DM    },  /*  37    DM_en          */
  { "env",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  38    DM_env         */
  { "er",        1,         1,           0,        0,       1,    AUTOCUT_ADD, VT100_OK    },  /*  39    DM_er          */
  { "es",        1,         1,           0,        0,       1,    AUTOCUT_ADD, VT100_OK    },  /*  40    DM_es          */
  { "ex",        0,         0,           0,        0,       0,        0,       0           },  /*  41    DM_ex          */
  { "fl",        1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  42    DM_fl          */
  { "gm",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  43    DM_gm          */
  { "icon",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  44    DM_icon        */
  { "idf",       0,         0,           0,        0,       0,        0,       0           },  /*  45    DM_idf         */
  { "inv",       1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  46    DM_inv         */
  { "kbd",       0,         0,           0,        0,       0,        0,       0           },  /*  47    DM_kbd         */
  { "kd",        1,         0,           0,        1,       1,        0,       VT100_OK    },  /*  48    DM_kd          */
  { "l",         0,         0,           0,        0,       0,        0,       0           },  /*  49    DM_l           */
  { "mono",      0,         0,           0,        0,       0,        0,       0           },  /*  50    DM_mono        */
  { "msg",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  51    DM_msg         */
  { "pb",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  52    DM_pb          */
  { "ph",        1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  53    DM_ph          */
  { "pn",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  54    DM_pn          */
  { "pp",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  55    DM_pp          */
  { "pt",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  56    DM_pt          */
  { "pv",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  57    DM_pv          */
  { "pw",        1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  58    DM_pw          */
  { "rm",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  59    DM_rm          */
  { "ro",        1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  60    DM_ro          */
  { "rs",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  61    DM_rs          */
  { "rw",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  62    DM_rw          */
  { "s",         1,         1,           1,        1,       1,        0,       VT100_DM    },  /*  63    DM_s           */
  { "sc",        1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  64    DM_sc          */
  { "shut",      0,         0,           0,        0,       0,        0,       0           },  /*  65    DM_shut        */
  { "so",        1,         1,           1,        1,       1,        0,       VT100_DM    },  /*  66    DM_so          */
  { "sq",        1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  67    DM_sq          */
  { "tb",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  68    DM_tb          */
  { "tdm",       1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  69    DM_tdm         */
  { "th",        1,         0,           0,        0,       1,        0,       VT100_DM    },  /*  70    DM_th          */
  { "thl",       1,         0,           0,        0,       1,        0,       VT100_DM    },  /*  71    DM_thl         */
  { "ti",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  72    DM_ti          */
  { "tl",        1,         0,           0,        0,       1,        0,       VT100_DM    },  /*  73    DM_tl          */
  { "tlw",       0,         0,           0,        0,       0,        0,       0           },  /*  74    DM_tlw         */
  { "tn",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  75    DM_tn          */
  { "tni",       0,         0,           0,        0,       0,        0,       0           },  /*  76    DM_tni         */
  { "tr",        1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  77    DM_tr          */
  { "ts",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  78    DM_ts          */
  { "tt",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  79    DM_tt          */
  { "twb",       1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  80    DM_twb         */
  { "undo",      1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  81    DM_undo       modifies_buff should be zero, does not modify the current buffer, only the backing memdata structure  */
  { "wa",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  82    DM_wa          */
  { "wc",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  83    DM_wc          */
  { "wdf",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  84    DM_wdf         */
  { "wg",        0,         0,           0,        0,       0,        0,       0           },  /*  85    DM_wg          */
  { "wge",       0,         0,           0,        0,       0,        0,       0           },  /*  86    DM_wge         */
  { "wgra",      0,         0,           0,        0,       0,        0,       0           },  /*  87    DM_wgra        */
  { "wgrr",      0,         0,           0,        0,       0,        0,       0           },  /*  88    DM_wgrr        */
  { "wh",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  89    DM_wh          */
  { "wi",        0,         0,           0,        0,       0,        0,       0           },  /*  90    DM_wi          */
  { "wm",        0,         0,           0,        0,       0,        0,       0           },  /*  91    DM_wm          */
  { "wme",       0,         0,           0,        0,       0,        0,       0           },  /*  92    DM_wme         */
  { "wp",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  93    DM_wp          */
  { "ws",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  94    DM_ws          */
  { "xc",        1,         0,           1,        0,       1,        0,       VT100_OK    },  /*  95    DM_xc          */
  { "xd",        1,         1,           1,        0,       1,        0,       VT100_DM    },  /*  96    DM_xd          */
  { "xi",        0,         0,           0,        0,       0,        0,       0           },  /*  97    DM_xi          */
  { "xp",        1,         1,           1,        0,       1,    AUTOCUT_ADD, VT100_OK    },  /*  98    DM_xp          */
  { "=",         1,         0,           0,        1,       1,        0,       VT100_OK    },  /*  99    DM_equal       */
  { "kk",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  100   DM_kk          */
  { ",",         1,         0,           0,        1,       0,        0,       VT100_NEVER },  /*  101   DM_comma       */
  { "$",         1,         0,           1,        1,       0,        0,       VT100_NEVER },  /*  102   DM_dollar      */
  { "redo",      1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  103   DM_redo       modifies_buff should be zero, does not modify the current buffer, only the backing memdata structure  */
  { "lineno",    1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  104   DM_lineno      */
  { "geo",       1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  105   DM_geo         */
  { "",          1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  106   DM_prompt     DM_prompt - internal  */
  { "tdmo",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  107   DM_tdmo        */
  { "debug",     1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  108   DM_debug      debugging aid  */
  { "delsd",     1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  109   DM_delsd       */
  { "fgc",       1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  110   DM_fgc         */
  { "keys",      1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  111   DM_keys        */
  { "tf",        1,         1,           1,        0,       0,        0,       VT100_NEVER },  /*  112   DM_tf          */
  { "ww",        1,         1,           0,        0,       0,        0,       VT100_NEVER },  /*  113   DM_ww          */
  { "!",         1,         0,           1,        0,       1,        0,       VT100_NEVER },  /*  114   DM_bang - sometimes writes, checks itself  */
  { "hex",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  115   DM_hex         */
  { "re",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  116   DM_re          */
  { "untab",     1,         1,           1,        0,       1,        0,       VT100_DM    },  /*  117   DM_untab       */
  { "cd",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  118   DM_cd          */
  { "sic",       1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  119   DM_sic         */
  { "vt",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  120   DM_vt          */
  { "sleep",     1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  121   DM_sleep       */
  { "tmw",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  122   DM_tmw         */
  { "lkd",       1,         0,           0,        1,       0,        0,       VT100_OK    },  /*  123   DM_lkd         */
  { "ceterm",    1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  124   DM_ceterm      */
  { "rd",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  125   DM_rd          */
  { "pwd",       1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  126   DM_pwd         */
  { "wdc",       1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  127   DM_wdc         */
  { "mouse",     1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  128   DM_mouse       */
  { "rec",       1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  129   DM_rec         */
  { "bell",      1,         0,           0,        0,       0,        0,       VT100_OK },     /*  130   DM_bell        */
  { "title",     1,         0,           0,        0,       0,        0,       VT100_OK },     /*  131   DM_title       */
  { "envar",     1,         0,           0,        0,       0,        0,       VT100_OK },     /*  132   DM_envar       */
  { "alias",     1,         0,           0,        1,       0,        0,       VT100_OK },     /*  133   DM_alias       */
  { "sp",        1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  134   DM_sp          */
  { "prefix",    1,         0,           1,        0,       0,        0,       VT100_OK },     /*  135   DM_prefix      */
  { "xl",        1,         0,           1,        0,       0,        0,       VT100_OK    },  /*  136   DM_xl          */
  { ":",         1,         0,           0,        1,       0,        0,       VT100_OK    },  /*  137   DM_colon       */
  { "ca",        1,         0,           1,        0,       1,        0,       VT100_NEVER },  /*  138   DM_ca          */
  { "{",         1,         0,           1,        1,       0,        0,       VT100_DM },     /*  139   DM_corner      */
  { "fbdr",      1,         0,           1,        0,       0,        0,       VT100_OK },     /*  140   DM_fbdr        */
  { "sb",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  141   DM_sb          */
  { "mi",        1,         0,           0,        1,       0,        0,       VT100_OK },     /*  142   DM_mi          */
  { "lmi",       1,         0,           0,        1,       0,        0,       VT100_OK },     /*  143   DM_mi          */
  { "pd",        1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  144   DM_pd          */
  { "pdm",       1,         0,           0,        0,       0,        0,       VT100_OK },     /*  145   DM_pdm         */
  { "tmb",       1,         0,           0,        0,       0,        0,       VT100_OK },     /*  146   DM_tmb         */
  { "nc",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  147   DM_nc          */
  { "pc",        1,         0,           1,        0,       0,        0,       VT100_DM    },  /*  148   DM_pc          */
  { "eval",      1,         0,           0,        1,       0,        0,       VT100_OK    },  /*  149   DM_eval        */
  { "ind",       1,         1,           1,        0,       0,        0,       VT100_NEVER },  /*  150   DM_ind         */
  { "lsf",       1,         0,           0,        1,       0,        0,       VT100_OK },     /*  151   DM_lsf         */
  { "lbl",       1,         0,           0,        0,       1,        0,       VT100_NEVER },  /*  152   DM_lbl         */
  { "glb",       1,         0,           0,        0,       0,        0,       VT100_NEVER },  /*  153   DM_glb         */
  { "dlb",       1,         0,           1,        0,       1,        0,       VT100_NEVER },  /*  154   DM_dlb         */
  { "xa",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  155   DM_xa          */
  { "f",         1,         0,           1,        1,       0,        0,       VT100_DM    },  /*  1     DM_find        */
  { "caps",      1,         0,           0,        0,       0,        0,       VT100_DM    },  /*  157   DM_caps        */
  { "sl",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  158   DM_sl          */
  { "rl",        1,         0,           0,        0,       0,        0,       VT100_OK    },  /*  159   DM_rl          */
  { "reload",    1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  160   DM_reload      */
  { "cntlc",     1,         0,           1,        0,       0,        0,       VT100_NEVER },  /*  161   DM_cntlc       */
                                                                             
/*  name      supported   modifies     needs    special   cursor    autocut    vt100                                 */
/*                          buff       flush     delim     pos                  ok                                   */
                                                               

};


/* else not in main */
#else

extern DMSYM       dmsyms[DM_MAXDEF];


#endif

#endif

