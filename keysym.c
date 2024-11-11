/*static char *sccsid = "%Z% %M% %I% - %G% %U%";*/
#include <string.h>           /* /usr/include/string.h   */
#ifdef WIN32
#ifdef WIN32_XNT
#include "xnt.h"
#else
#include "X11/X.h"           /* /usr/include/X11/X.h   */
#include "X11/keysym.h"      /* /usr/include/X11/keysym.h */
#endif
#else
#include <X11/X.h>           /* /usr/include/X11/X.h   */
#include <X11/keysym.h>      /* /usr/include/X11/keysym.h */
#endif

/*****************************************************************************
*
*   translate_keysym     Translate an X11 keysym value into its text string
*
*   PARAMETERS:
*
*      key    -   KeySym  (an int)  (INPUT)
*
*   FUNCTIONS:
*
*   1.  switch on the key value and assign the approriate text string.
*
*   RETURNED VALUE:
*
*   name    -   pointer to char
*               This is the text string name of the keysym
*
*   NOTES:
*
*   1.   This program was generated from X11/Keysymdef.h
*
*   2.   The strings returned are static, they may be used indefinitely,
*        but should NOT be modified
*
*********************************************************************************/

#ifdef DebuG


typedef struct {
   KeySym      sym;
   char        *val;
} KEYDEF;

static KEYDEF   key_table[] = {
#ifdef XK_VoidSymbol
         XK_VoidSymbol,  "XK_VoidSymbol",
#endif
#ifdef XK_BackSpace
         XK_BackSpace,  "XK_BackSpace",
#endif
#ifdef XK_Tab
         XK_Tab,  "XK_Tab",
#endif
#ifdef XK_Linefeed
         XK_Linefeed,  "XK_Linefeed",
#endif
#ifdef XK_Clear
         XK_Clear,  "XK_Clear",
#endif
#ifdef XK_Return
         XK_Return,  "XK_Return",
#endif
#ifdef XK_Pause
         XK_Pause,  "XK_Pause",
#endif
#ifdef XK_Scroll_Lock
         XK_Scroll_Lock,  "XK_Scroll_Lock",
#endif
#ifdef XK_Escape
         XK_Escape,  "XK_Escape",
#endif
#ifdef XK_Delete
         XK_Delete,  "XK_Delete",
#endif
#ifdef XK_Multi_key
         XK_Multi_key,  "XK_Multi_key",
#endif
#ifdef XK_Kanji
         XK_Kanji,  "XK_Kanji",
#endif
#ifdef XK_Muhenkan
         XK_Muhenkan,  "XK_Muhenkan",
#endif
#ifdef XK_Henkan_Mode
         XK_Henkan_Mode,  "XK_Henkan_Mode",
#endif
#ifdef XK_Romaji
         XK_Romaji,  "XK_Romaji",
#endif
#ifdef XK_Hiragana
         XK_Hiragana,  "XK_Hiragana",
#endif
#ifdef XK_Katakana
         XK_Katakana,  "XK_Katakana",
#endif
#ifdef XK_Hiragana_Katakana
         XK_Hiragana_Katakana,  "XK_Hiragana_Katakana",
#endif
#ifdef XK_Zenkaku
         XK_Zenkaku,  "XK_Zenkaku",
#endif
#ifdef XK_Hankaku
         XK_Hankaku,  "XK_Hankaku",
#endif
#ifdef XK_Zenkaku_Hankaku
         XK_Zenkaku_Hankaku,  "XK_Zenkaku_Hankaku",
#endif
#ifdef XK_Touroku
         XK_Touroku,  "XK_Touroku",
#endif
#ifdef XK_Massyo
         XK_Massyo,  "XK_Massyo",
#endif
#ifdef XK_Kana_Lock
         XK_Kana_Lock,  "XK_Kana_Lock",
#endif
#ifdef XK_Kana_Shift
         XK_Kana_Shift,  "XK_Kana_Shift",
#endif
#ifdef XK_Eisu_Shift
         XK_Eisu_Shift,  "XK_Eisu_Shift",
#endif
#ifdef XK_Eisu_toggle
         XK_Eisu_toggle,  "XK_Eisu_toggle",
#endif
#ifdef XK_Home
         XK_Home,  "XK_Home",
#endif
#ifdef XK_Left
         XK_Left,  "XK_Left",
#endif
#ifdef XK_Up
         XK_Up,  "XK_Up",
#endif
#ifdef XK_Right
         XK_Right,  "XK_Right",
#endif
#ifdef XK_Down
         XK_Down,  "XK_Down",
#endif
#ifdef XK_Prior
         XK_Prior,  "XK_Prior",
#endif
#ifdef XK_Next
         XK_Next,  "XK_Next",
#endif
#ifdef XK_End
         XK_End,  "XK_End",
#endif
#ifdef XK_Begin
         XK_Begin,  "XK_Begin",
#endif
#ifdef XK_Select
         XK_Select,  "XK_Select",
#endif
#ifdef XK_Print
         XK_Print,  "XK_Print",
#endif
#ifdef XK_Execute
         XK_Execute,  "XK_Execute",
#endif
#ifdef XK_Insert
         XK_Insert,  "XK_Insert",
#endif
#ifdef XK_Undo
         XK_Undo,  "XK_Undo",
#endif
#ifdef XK_Redo
         XK_Redo,  "XK_Redo",
#endif
#ifdef XK_Menu
         XK_Menu,  "XK_Menu",
#endif
#ifdef XK_Find
         XK_Find,  "XK_Find",
#endif
#ifdef XK_Cancel
         XK_Cancel,  "XK_Cancel",
#endif
#ifdef XK_Help
         XK_Help,  "XK_Help",
#endif
#ifdef XK_Break
         XK_Break,  "XK_Break",
#endif
#ifdef XK_Mode_switch
         XK_Mode_switch,  "XK_Mode_switch",
#endif
#ifdef XK_Num_Lock
         XK_Num_Lock,  "XK_Num_Lock",
#endif
#ifdef XK_KP_Space
         XK_KP_Space,  "XK_KP_Space",
#endif
#ifdef XK_KP_Tab
         XK_KP_Tab,  "XK_KP_Tab",
#endif
#ifdef XK_KP_Enter
         XK_KP_Enter,  "XK_KP_Enter",
#endif
#ifdef XK_KP_F1
         XK_KP_F1,  "XK_KP_F1",
#endif
#ifdef XK_KP_F2
         XK_KP_F2,  "XK_KP_F2",
#endif
#ifdef XK_KP_F3
         XK_KP_F3,  "XK_KP_F3",
#endif
#ifdef XK_KP_F4
         XK_KP_F4,  "XK_KP_F4",
#endif
#ifdef XK_KP_Equal
         XK_KP_Equal,  "XK_KP_Equal",
#endif
#ifdef XK_KP_Multiply
         XK_KP_Multiply,  "XK_KP_Multiply",
#endif
#ifdef XK_KP_Add
         XK_KP_Add,  "XK_KP_Add",
#endif
#ifdef XK_KP_Separator
         XK_KP_Separator,  "XK_KP_Separator",
#endif
#ifdef XK_KP_Subtract
         XK_KP_Subtract,  "XK_KP_Subtract",
#endif
#ifdef XK_KP_Decimal
         XK_KP_Decimal,  "XK_KP_Decimal",
#endif
#ifdef XK_KP_Divide
         XK_KP_Divide,  "XK_KP_Divide",
#endif
#ifdef XK_KP_0
         XK_KP_0,  "XK_KP_0",
#endif
#ifdef XK_KP_1
         XK_KP_1,  "XK_KP_1",
#endif
#ifdef XK_KP_2
         XK_KP_2,  "XK_KP_2",
#endif
#ifdef XK_KP_3
         XK_KP_3,  "XK_KP_3",
#endif
#ifdef XK_KP_4
         XK_KP_4,  "XK_KP_4",
#endif
#ifdef XK_KP_5
         XK_KP_5,  "XK_KP_5",
#endif
#ifdef XK_KP_6
         XK_KP_6,  "XK_KP_6",
#endif
#ifdef XK_KP_7
         XK_KP_7,  "XK_KP_7",
#endif
#ifdef XK_KP_8
         XK_KP_8,  "XK_KP_8",
#endif
#ifdef XK_KP_9
         XK_KP_9,  "XK_KP_9",
#endif
#ifdef XK_F1
         XK_F1,  "XK_F1",
#endif
#ifdef XK_F2
         XK_F2,  "XK_F2",
#endif
#ifdef XK_F3
         XK_F3,  "XK_F3",
#endif
#ifdef XK_F4
         XK_F4,  "XK_F4",
#endif
#ifdef XK_F5
         XK_F5,  "XK_F5",
#endif
#ifdef XK_F6
         XK_F6,  "XK_F6",
#endif
#ifdef XK_F7
         XK_F7,  "XK_F7",
#endif
#ifdef XK_F8
         XK_F8,  "XK_F8",
#endif
#ifdef XK_F9
         XK_F9,  "XK_F9",
#endif
#ifdef XK_F10
         XK_F10,  "XK_F10",
#endif
#ifdef XK_F11
         XK_F11,  "XK_F11",
#endif
#ifdef XK_F12
         XK_F12,  "XK_F12",
#endif
#ifdef XK_F13
         XK_F13,  "XK_F13",
#endif
#ifdef XK_F14
         XK_F14,  "XK_F14",
#endif
#ifdef XK_F15
         XK_F15,  "XK_F15",
#endif
#ifdef XK_F16
         XK_F16,  "XK_F16",
#endif
#ifdef XK_F17
         XK_F17,  "XK_F17",
#endif
#ifdef XK_F18
         XK_F18,  "XK_F18",
#endif
#ifdef XK_F19
         XK_F19,  "XK_F19",
#endif
#ifdef XK_F20
         XK_F20,  "XK_F20",
#endif
#ifdef XK_F21
         XK_F21,  "XK_F21",
#endif
#ifdef XK_F22
         XK_F22,  "XK_F22",
#endif
#ifdef XK_F23
         XK_F23,  "XK_F23",
#endif
#ifdef XK_F24
         XK_F24,  "XK_F24",
#endif
#ifdef XK_F25
         XK_F25,  "XK_F25",
#endif
#ifdef XK_F26
         XK_F26,  "XK_F26",
#endif
#ifdef XK_F27
         XK_F27,  "XK_F27",
#endif
#ifdef XK_F28
         XK_F28,  "XK_F28",
#endif
#ifdef XK_F29
         XK_F29,  "XK_F29",
#endif
#ifdef XK_F30
         XK_F30,  "XK_F30",
#endif
#ifdef XK_F31
         XK_F31,  "XK_F31",
#endif
#ifdef XK_F32
         XK_F32,  "XK_F32",
#endif
#ifdef XK_R13
         XK_R13,  "XK_R13",
#endif
#ifdef XK_F34
         XK_F34,  "XK_F34",
#endif
#ifdef XK_F35
         XK_F35,  "XK_F35",
#endif
#ifdef XK_Shift_L
         XK_Shift_L,  "XK_Shift_L",
#endif
#ifdef XK_Shift_R
         XK_Shift_R,  "XK_Shift_R",
#endif
#ifdef XK_Control_L
         XK_Control_L,  "XK_Control_L",
#endif
#ifdef XK_Control_R
         XK_Control_R,  "XK_Control_R",
#endif
#ifdef XK_Caps_Lock
         XK_Caps_Lock,  "XK_Caps_Lock",
#endif
#ifdef XK_Shift_Lock
         XK_Shift_Lock,  "XK_Shift_Lock",
#endif
#ifdef XK_Meta_L
         XK_Meta_L,  "XK_Meta_L",
#endif
#ifdef XK_Meta_R
         XK_Meta_R,  "XK_Meta_R",
#endif
#ifdef XK_Alt_L
         XK_Alt_L,  "XK_Alt_L",
#endif
#ifdef XK_Alt_R
         XK_Alt_R,  "XK_Alt_R",
#endif
#ifdef XK_Super_L
         XK_Super_L,  "XK_Super_L",
#endif
#ifdef XK_Super_R
         XK_Super_R,  "XK_Super_R",
#endif
#ifdef XK_Hyper_L
         XK_Hyper_L,  "XK_Hyper_L",
#endif
#ifdef XK_Hyper_R
         XK_Hyper_R,  "XK_Hyper_R",
#endif
#ifdef XK_space
         XK_space,  "XK_space",
#endif
#ifdef XK_exclam
         XK_exclam,  "XK_exclam",
#endif
#ifdef XK_quotedbl
         XK_quotedbl,  "XK_quotedbl",
#endif
#ifdef XK_numbersign
         XK_numbersign,  "XK_numbersign",
#endif
#ifdef XK_dollar
         XK_dollar,  "XK_dollar",
#endif
#ifdef XK_percent
         XK_percent,  "XK_percent",
#endif
#ifdef XK_ampersand
         XK_ampersand,  "XK_ampersand",
#endif
#ifdef XK_apostrophe
         XK_apostrophe,  "XK_apostrophe",
#endif
#ifdef XK_parenleft
         XK_parenleft,  "XK_parenleft",
#endif
#ifdef XK_parenright
         XK_parenright,  "XK_parenright",
#endif
#ifdef XK_asterisk
         XK_asterisk,  "XK_asterisk",
#endif
#ifdef XK_plus
         XK_plus,  "XK_plus",
#endif
#ifdef XK_comma
         XK_comma,  "XK_comma",
#endif
#ifdef XK_minus
         XK_minus,  "XK_minus",
#endif
#ifdef XK_period
         XK_period,  "XK_period",
#endif
#ifdef XK_slash
         XK_slash,  "XK_slash",
#endif
#ifdef XK_0
         XK_0,  "XK_0",
#endif
#ifdef XK_1
         XK_1,  "XK_1",
#endif
#ifdef XK_2
         XK_2,  "XK_2",
#endif
#ifdef XK_3
         XK_3,  "XK_3",
#endif
#ifdef XK_4
         XK_4,  "XK_4",
#endif
#ifdef XK_5
         XK_5,  "XK_5",
#endif
#ifdef XK_6
         XK_6,  "XK_6",
#endif
#ifdef XK_7
         XK_7,  "XK_7",
#endif
#ifdef XK_8
         XK_8,  "XK_8",
#endif
#ifdef XK_9
         XK_9,  "XK_9",
#endif
#ifdef XK_colon
         XK_colon,  "XK_colon",
#endif
#ifdef XK_semicolon
         XK_semicolon,  "XK_semicolon",
#endif
#ifdef XK_less
         XK_less,  "XK_less",
#endif
#ifdef XK_equal
         XK_equal,  "XK_equal",
#endif
#ifdef XK_greater
         XK_greater,  "XK_greater",
#endif
#ifdef XK_question
         XK_question,  "XK_question",
#endif
#ifdef XK_at
         XK_at,  "XK_at",
#endif
#ifdef XK_A
         XK_A,  "XK_A",
#endif
#ifdef XK_B
         XK_B,  "XK_B",
#endif
#ifdef XK_C
         XK_C,  "XK_C",
#endif
#ifdef XK_D
         XK_D,  "XK_D",
#endif
#ifdef XK_E
         XK_E,  "XK_E",
#endif
#ifdef XK_F
         XK_F,  "XK_F",
#endif
#ifdef XK_G
         XK_G,  "XK_G",
#endif
#ifdef XK_H
         XK_H,  "XK_H",
#endif
#ifdef XK_I
         XK_I,  "XK_I",
#endif
#ifdef XK_J
         XK_J,  "XK_J",
#endif
#ifdef XK_K
         XK_K,  "XK_K",
#endif
#ifdef XK_L
         XK_L,  "XK_L",
#endif
#ifdef XK_M
         XK_M,  "XK_M",
#endif
#ifdef XK_N
         XK_N,  "XK_N",
#endif
#ifdef XK_O
         XK_O,  "XK_O",
#endif
#ifdef XK_P
         XK_P,  "XK_P",
#endif
#ifdef XK_Q
         XK_Q,  "XK_Q",
#endif
#ifdef XK_R
         XK_R,  "XK_R",
#endif
#ifdef XK_S
         XK_S,  "XK_S",
#endif
#ifdef XK_T
         XK_T,  "XK_T",
#endif
#ifdef XK_U
         XK_U,  "XK_U",
#endif
#ifdef XK_V
         XK_V,  "XK_V",
#endif
#ifdef XK_W
         XK_W,  "XK_W",
#endif
#ifdef XK_X
         XK_X,  "XK_X",
#endif
#ifdef XK_Y
         XK_Y,  "XK_Y",
#endif
#ifdef XK_Z
         XK_Z,  "XK_Z",
#endif
#ifdef XK_bracketleft
         XK_bracketleft,  "XK_bracketleft",
#endif
#ifdef XK_backslash
         XK_backslash,  "XK_backslash",
#endif
#ifdef XK_bracketright
         XK_bracketright,  "XK_bracketright",
#endif
#ifdef XK_asciicircum
         XK_asciicircum,  "XK_asciicircum",
#endif
#ifdef XK_underscore
         XK_underscore,  "XK_underscore",
#endif
#ifdef XK_grave
         XK_grave,  "XK_grave",
#endif
#ifdef XK_a
         XK_a,  "XK_a",
#endif
#ifdef XK_b
         XK_b,  "XK_b",
#endif
#ifdef XK_c
         XK_c,  "XK_c",
#endif
#ifdef XK_d
         XK_d,  "XK_d",
#endif
#ifdef XK_e
         XK_e,  "XK_e",
#endif
#ifdef XK_f
         XK_f,  "XK_f",
#endif
#ifdef XK_g
         XK_g,  "XK_g",
#endif
#ifdef XK_h
         XK_h,  "XK_h",
#endif
#ifdef XK_i
         XK_i,  "XK_i",
#endif
#ifdef XK_j
         XK_j,  "XK_j",
#endif
#ifdef XK_k
         XK_k,  "XK_k",
#endif
#ifdef XK_l
         XK_l,  "XK_l",
#endif
#ifdef XK_m
         XK_m,  "XK_m",
#endif
#ifdef XK_n
         XK_n,  "XK_n",
#endif
#ifdef XK_o
         XK_o,  "XK_o",
#endif
#ifdef XK_p
         XK_p,  "XK_p",
#endif
#ifdef XK_q
         XK_q,  "XK_q",
#endif
#ifdef XK_r
         XK_r,  "XK_r",
#endif
#ifdef XK_s
         XK_s,  "XK_s",
#endif
#ifdef XK_t
         XK_t,  "XK_t",
#endif
#ifdef XK_u
         XK_u,  "XK_u",
#endif
#ifdef XK_v
         XK_v,  "XK_v",
#endif
#ifdef XK_w
         XK_w,  "XK_w",
#endif
#ifdef XK_x
         XK_x,  "XK_x",
#endif
#ifdef XK_y
         XK_y,  "XK_y",
#endif
#ifdef XK_z
         XK_z,  "XK_z",
#endif
#ifdef XK_braceleft
         XK_braceleft,  "XK_braceleft",
#endif
#ifdef XK_bar
         XK_bar,  "XK_bar",
#endif
#ifdef XK_braceright
         XK_braceright,  "XK_braceright",
#endif
#ifdef XK_asciitilde
         XK_asciitilde,  "XK_asciitilde",
#endif
#ifdef XK_nobreakspace
         XK_nobreakspace,  "XK_nobreakspace",
#endif
#ifdef XK_exclamdown
         XK_exclamdown,  "XK_exclamdown",
#endif
#ifdef XK_cent
         XK_cent,  "XK_cent",
#endif
#ifdef XK_sterling
         XK_sterling,  "XK_sterling",
#endif
#ifdef XK_currency
         XK_currency,  "XK_currency",
#endif
#ifdef XK_yen
         XK_yen,  "XK_yen",
#endif
#ifdef XK_brokenbar
         XK_brokenbar,  "XK_brokenbar",
#endif
#ifdef XK_section
         XK_section,  "XK_section",
#endif
#ifdef XK_diaeresis
         XK_diaeresis,  "XK_diaeresis",
#endif
#ifdef XK_copyright
         XK_copyright,  "XK_copyright",
#endif
#ifdef XK_ordfeminine
         XK_ordfeminine,  "XK_ordfeminine",
#endif
#ifdef XK_guillemotleft
         XK_guillemotleft,  "XK_guillemotleft",
#endif
#ifdef XK_notsign
         XK_notsign,  "XK_notsign",
#endif
#ifdef XK_hyphen
         XK_hyphen,  "XK_hyphen",
#endif
#ifdef XK_registered
         XK_registered,  "XK_registered",
#endif
#ifdef XK_macron
         XK_macron,  "XK_macron",
#endif
#ifdef XK_degree
         XK_degree,  "XK_degree",
#endif
#ifdef XK_plusminus
         XK_plusminus,  "XK_plusminus",
#endif
#ifdef XK_twosuperior
         XK_twosuperior,  "XK_twosuperior",
#endif
#ifdef XK_threesuperior
         XK_threesuperior,  "XK_threesuperior",
#endif
#ifdef XK_acute
         XK_acute,  "XK_acute",
#endif
#ifdef XK_mu
         XK_mu,  "XK_mu",
#endif
#ifdef XK_paragraph
         XK_paragraph,  "XK_paragraph",
#endif
#ifdef XK_periodcentered
         XK_periodcentered,  "XK_periodcentered",
#endif
#ifdef XK_cedilla
         XK_cedilla,  "XK_cedilla",
#endif
#ifdef XK_onesuperior
         XK_onesuperior,  "XK_onesuperior",
#endif
#ifdef XK_masculine
         XK_masculine,  "XK_masculine",
#endif
#ifdef XK_guillemotright
         XK_guillemotright,  "XK_guillemotright",
#endif
#ifdef XK_onequarter
         XK_onequarter,  "XK_onequarter",
#endif
#ifdef XK_onehalf
         XK_onehalf,  "XK_onehalf",
#endif
#ifdef XK_threequarters
         XK_threequarters,  "XK_threequarters",
#endif
#ifdef XK_questiondown
         XK_questiondown,  "XK_questiondown",
#endif
#ifdef XK_Agrave
         XK_Agrave,  "XK_Agrave",
#endif
#ifdef XK_Aacute
         XK_Aacute,  "XK_Aacute",
#endif
#ifdef XK_Acircumflex
         XK_Acircumflex,  "XK_Acircumflex",
#endif
#ifdef XK_Atilde
         XK_Atilde,  "XK_Atilde",
#endif
#ifdef XK_Adiaeresis
         XK_Adiaeresis,  "XK_Adiaeresis",
#endif
#ifdef XK_Aring
         XK_Aring,  "XK_Aring",
#endif
#ifdef XK_AE
         XK_AE,  "XK_AE",
#endif
#ifdef XK_Ccedilla
         XK_Ccedilla,  "XK_Ccedilla",
#endif
#ifdef XK_Egrave
         XK_Egrave,  "XK_Egrave",
#endif
#ifdef XK_Eacute
         XK_Eacute,  "XK_Eacute",
#endif
#ifdef XK_Ecircumflex
         XK_Ecircumflex,  "XK_Ecircumflex",
#endif
#ifdef XK_Ediaeresis
         XK_Ediaeresis,  "XK_Ediaeresis",
#endif
#ifdef XK_Igrave
         XK_Igrave,  "XK_Igrave",
#endif
#ifdef XK_Iacute
         XK_Iacute,  "XK_Iacute",
#endif
#ifdef XK_Icircumflex
         XK_Icircumflex,  "XK_Icircumflex",
#endif
#ifdef XK_Idiaeresis
         XK_Idiaeresis,  "XK_Idiaeresis",
#endif
#ifdef XK_ETH
         XK_ETH,  "XK_ETH",
#endif
#ifdef XK_Ntilde
         XK_Ntilde,  "XK_Ntilde",
#endif
#ifdef XK_Ograve
         XK_Ograve,  "XK_Ograve",
#endif
#ifdef XK_Oacute
         XK_Oacute,  "XK_Oacute",
#endif
#ifdef XK_Ocircumflex
         XK_Ocircumflex,  "XK_Ocircumflex",
#endif
#ifdef XK_Otilde
         XK_Otilde,  "XK_Otilde",
#endif
#ifdef XK_Odiaeresis
         XK_Odiaeresis,  "XK_Odiaeresis",
#endif
#ifdef XK_multiply
         XK_multiply,  "XK_multiply",
#endif
#ifdef XK_Ooblique
         XK_Ooblique,  "XK_Ooblique",
#endif
#ifdef XK_Ugrave
         XK_Ugrave,  "XK_Ugrave",
#endif
#ifdef XK_Uacute
         XK_Uacute,  "XK_Uacute",
#endif
#ifdef XK_Ucircumflex
         XK_Ucircumflex,  "XK_Ucircumflex",
#endif
#ifdef XK_Udiaeresis
         XK_Udiaeresis,  "XK_Udiaeresis",
#endif
#ifdef XK_Yacute
         XK_Yacute,  "XK_Yacute",
#endif
#ifdef XK_THORN
         XK_THORN,  "XK_THORN",
#endif
#ifdef XK_ssharp
         XK_ssharp,  "XK_ssharp",
#endif
#ifdef XK_agrave
         XK_agrave,  "XK_agrave",
#endif
#ifdef XK_aacute
         XK_aacute,  "XK_aacute",
#endif
#ifdef XK_acircumflex
         XK_acircumflex,  "XK_acircumflex",
#endif
#ifdef XK_atilde
         XK_atilde,  "XK_atilde",
#endif
#ifdef XK_adiaeresis
         XK_adiaeresis,  "XK_adiaeresis",
#endif
#ifdef XK_aring
         XK_aring,  "XK_aring",
#endif
#ifdef XK_ae
         XK_ae,  "XK_ae",
#endif
#ifdef XK_ccedilla
         XK_ccedilla,  "XK_ccedilla",
#endif
#ifdef XK_egrave
         XK_egrave,  "XK_egrave",
#endif
#ifdef XK_eacute
         XK_eacute,  "XK_eacute",
#endif
#ifdef XK_ecircumflex
         XK_ecircumflex,  "XK_ecircumflex",
#endif
#ifdef XK_ediaeresis
         XK_ediaeresis,  "XK_ediaeresis",
#endif
#ifdef XK_igrave
         XK_igrave,  "XK_igrave",
#endif
#ifdef XK_iacute
         XK_iacute,  "XK_iacute",
#endif
#ifdef XK_icircumflex
         XK_icircumflex,  "XK_icircumflex",
#endif
#ifdef XK_idiaeresis
         XK_idiaeresis,  "XK_idiaeresis",
#endif
#ifdef XK_eth
         XK_eth,  "XK_eth",
#endif
#ifdef XK_ntilde
         XK_ntilde,  "XK_ntilde",
#endif
#ifdef XK_ograve
         XK_ograve,  "XK_ograve",
#endif
#ifdef XK_oacute
         XK_oacute,  "XK_oacute",
#endif
#ifdef XK_ocircumflex
         XK_ocircumflex,  "XK_ocircumflex",
#endif
#ifdef XK_otilde
         XK_otilde,  "XK_otilde",
#endif
#ifdef XK_odiaeresis
         XK_odiaeresis,  "XK_odiaeresis",
#endif
#ifdef XK_division
         XK_division,  "XK_division",
#endif
#ifdef XK_oslash
         XK_oslash,  "XK_oslash",
#endif
#ifdef XK_ugrave
         XK_ugrave,  "XK_ugrave",
#endif
#ifdef XK_uacute
         XK_uacute,  "XK_uacute",
#endif
#ifdef XK_ucircumflex
         XK_ucircumflex,  "XK_ucircumflex",
#endif
#ifdef XK_udiaeresis
         XK_udiaeresis,  "XK_udiaeresis",
#endif
#ifdef XK_yacute
         XK_yacute,  "XK_yacute",
#endif
#ifdef XK_thorn
         XK_thorn,  "XK_thorn",
#endif
#ifdef XK_ydiaeresis
         XK_ydiaeresis,  "XK_ydiaeresis",
#endif
#ifdef XK_Aogonek
         XK_Aogonek,  "XK_Aogonek",
#endif
#ifdef XK_breve
         XK_breve,  "XK_breve",
#endif
#ifdef XK_Lstroke
         XK_Lstroke,  "XK_Lstroke",
#endif
#ifdef XK_Lcaron
         XK_Lcaron,  "XK_Lcaron",
#endif
#ifdef XK_Sacute
         XK_Sacute,  "XK_Sacute",
#endif
#ifdef XK_Scaron
         XK_Scaron,  "XK_Scaron",
#endif
#ifdef XK_Scedilla
         XK_Scedilla,  "XK_Scedilla",
#endif
#ifdef XK_Tcaron
         XK_Tcaron,  "XK_Tcaron",
#endif
#ifdef XK_Zacute
         XK_Zacute,  "XK_Zacute",
#endif
#ifdef XK_Zcaron
         XK_Zcaron,  "XK_Zcaron",
#endif
#ifdef XK_Zabovedot
         XK_Zabovedot,  "XK_Zabovedot",
#endif
#ifdef XK_aogonek
         XK_aogonek,  "XK_aogonek",
#endif
#ifdef XK_ogonek
         XK_ogonek,  "XK_ogonek",
#endif
#ifdef XK_lstroke
         XK_lstroke,  "XK_lstroke",
#endif
#ifdef XK_lcaron
         XK_lcaron,  "XK_lcaron",
#endif
#ifdef XK_sacute
         XK_sacute,  "XK_sacute",
#endif
#ifdef XK_caron
         XK_caron,  "XK_caron",
#endif
#ifdef XK_scaron
         XK_scaron,  "XK_scaron",
#endif
#ifdef XK_scedilla
         XK_scedilla,  "XK_scedilla",
#endif
#ifdef XK_tcaron
         XK_tcaron,  "XK_tcaron",
#endif
#ifdef XK_zacute
         XK_zacute,  "XK_zacute",
#endif
#ifdef XK_doubleacute
         XK_doubleacute,  "XK_doubleacute",
#endif
#ifdef XK_zcaron
         XK_zcaron,  "XK_zcaron",
#endif
#ifdef XK_zabovedot
         XK_zabovedot,  "XK_zabovedot",
#endif
#ifdef XK_Racute
         XK_Racute,  "XK_Racute",
#endif
#ifdef XK_Abreve
         XK_Abreve,  "XK_Abreve",
#endif
#ifdef XK_Lacute
         XK_Lacute,  "XK_Lacute",
#endif
#ifdef XK_Cacute
         XK_Cacute,  "XK_Cacute",
#endif
#ifdef XK_Ccaron
         XK_Ccaron,  "XK_Ccaron",
#endif
#ifdef XK_Eogonek
         XK_Eogonek,  "XK_Eogonek",
#endif
#ifdef XK_Ecaron
         XK_Ecaron,  "XK_Ecaron",
#endif
#ifdef XK_Dcaron
         XK_Dcaron,  "XK_Dcaron",
#endif
#ifdef XK_Dstroke
         XK_Dstroke,  "XK_Dstroke",
#endif
#ifdef XK_Nacute
         XK_Nacute,  "XK_Nacute",
#endif
#ifdef XK_Ncaron
         XK_Ncaron,  "XK_Ncaron",
#endif
#ifdef XK_Odoubleacute
         XK_Odoubleacute,  "XK_Odoubleacute",
#endif
#ifdef XK_Rcaron
         XK_Rcaron,  "XK_Rcaron",
#endif
#ifdef XK_Uring
         XK_Uring,  "XK_Uring",
#endif
#ifdef XK_Udoubleacute
         XK_Udoubleacute,  "XK_Udoubleacute",
#endif
#ifdef XK_Tcedilla
         XK_Tcedilla,  "XK_Tcedilla",
#endif
#ifdef XK_racute
         XK_racute,  "XK_racute",
#endif
#ifdef XK_abreve
         XK_abreve,  "XK_abreve",
#endif
#ifdef XK_lacute
         XK_lacute,  "XK_lacute",
#endif
#ifdef XK_cacute
         XK_cacute,  "XK_cacute",
#endif
#ifdef XK_ccaron
         XK_ccaron,  "XK_ccaron",
#endif
#ifdef XK_eogonek
         XK_eogonek,  "XK_eogonek",
#endif
#ifdef XK_ecaron
         XK_ecaron,  "XK_ecaron",
#endif
#ifdef XK_dcaron
         XK_dcaron,  "XK_dcaron",
#endif
#ifdef XK_dstroke
         XK_dstroke,  "XK_dstroke",
#endif
#ifdef XK_nacute
         XK_nacute,  "XK_nacute",
#endif
#ifdef XK_ncaron
         XK_ncaron,  "XK_ncaron",
#endif
#ifdef XK_odoubleacute
         XK_odoubleacute,  "XK_odoubleacute",
#endif
#ifdef XK_udoubleacute
         XK_udoubleacute,  "XK_udoubleacute",
#endif
#ifdef XK_rcaron
         XK_rcaron,  "XK_rcaron",
#endif
#ifdef XK_uring
         XK_uring,  "XK_uring",
#endif
#ifdef XK_tcedilla
         XK_tcedilla,  "XK_tcedilla",
#endif
#ifdef XK_abovedot
         XK_abovedot,  "XK_abovedot",
#endif
#ifdef XK_Hstroke
         XK_Hstroke,  "XK_Hstroke",
#endif
#ifdef XK_Hcircumflex
         XK_Hcircumflex,  "XK_Hcircumflex",
#endif
#ifdef XK_Iabovedot
         XK_Iabovedot,  "XK_Iabovedot",
#endif
#ifdef XK_Gbreve
         XK_Gbreve,  "XK_Gbreve",
#endif
#ifdef XK_Jcircumflex
         XK_Jcircumflex,  "XK_Jcircumflex",
#endif
#ifdef XK_hstroke
         XK_hstroke,  "XK_hstroke",
#endif
#ifdef XK_hcircumflex
         XK_hcircumflex,  "XK_hcircumflex",
#endif
#ifdef XK_idotless
         XK_idotless,  "XK_idotless",
#endif
#ifdef XK_gbreve
         XK_gbreve,  "XK_gbreve",
#endif
#ifdef XK_jcircumflex
         XK_jcircumflex,  "XK_jcircumflex",
#endif
#ifdef XK_Cabovedot
         XK_Cabovedot,  "XK_Cabovedot",
#endif
#ifdef XK_Ccircumflex
         XK_Ccircumflex,  "XK_Ccircumflex",
#endif
#ifdef XK_Gabovedot
         XK_Gabovedot,  "XK_Gabovedot",
#endif
#ifdef XK_Gcircumflex
         XK_Gcircumflex,  "XK_Gcircumflex",
#endif
#ifdef XK_Ubreve
         XK_Ubreve,  "XK_Ubreve",
#endif
#ifdef XK_Scircumflex
         XK_Scircumflex,  "XK_Scircumflex",
#endif
#ifdef XK_cabovedot
         XK_cabovedot,  "XK_cabovedot",
#endif
#ifdef XK_ccircumflex
         XK_ccircumflex,  "XK_ccircumflex",
#endif
#ifdef XK_gabovedot
         XK_gabovedot,  "XK_gabovedot",
#endif
#ifdef XK_gcircumflex
         XK_gcircumflex,  "XK_gcircumflex",
#endif
#ifdef XK_ubreve
         XK_ubreve,  "XK_ubreve",
#endif
#ifdef XK_scircumflex
         XK_scircumflex,  "XK_scircumflex",
#endif
#ifdef XK_kra
         XK_kra,  "XK_kra",
#endif
#ifdef XK_Rcedilla
         XK_Rcedilla,  "XK_Rcedilla",
#endif
#ifdef XK_Itilde
         XK_Itilde,  "XK_Itilde",
#endif
#ifdef XK_Lcedilla
         XK_Lcedilla,  "XK_Lcedilla",
#endif
#ifdef XK_Emacron
         XK_Emacron,  "XK_Emacron",
#endif
#ifdef XK_Gcedilla
         XK_Gcedilla,  "XK_Gcedilla",
#endif
#ifdef XK_Tslash
         XK_Tslash,  "XK_Tslash",
#endif
#ifdef XK_rcedilla
         XK_rcedilla,  "XK_rcedilla",
#endif
#ifdef XK_itilde
         XK_itilde,  "XK_itilde",
#endif
#ifdef XK_lcedilla
         XK_lcedilla,  "XK_lcedilla",
#endif
#ifdef XK_emacron
         XK_emacron,  "XK_emacron",
#endif
#ifdef XK_gcedilla
         XK_gcedilla,  "XK_gcedilla",
#endif
#ifdef XK_tslash
         XK_tslash,  "XK_tslash",
#endif
#ifdef XK_ENG
         XK_ENG,  "XK_ENG",
#endif
#ifdef XK_eng
         XK_eng,  "XK_eng",
#endif
#ifdef XK_Amacron
         XK_Amacron,  "XK_Amacron",
#endif
#ifdef XK_Iogonek
         XK_Iogonek,  "XK_Iogonek",
#endif
#ifdef XK_Eabovedot
         XK_Eabovedot,  "XK_Eabovedot",
#endif
#ifdef XK_Imacron
         XK_Imacron,  "XK_Imacron",
#endif
#ifdef XK_Ncedilla
         XK_Ncedilla,  "XK_Ncedilla",
#endif
#ifdef XK_Omacron
         XK_Omacron,  "XK_Omacron",
#endif
#ifdef XK_Kcedilla
         XK_Kcedilla,  "XK_Kcedilla",
#endif
#ifdef XK_Uogonek
         XK_Uogonek,  "XK_Uogonek",
#endif
#ifdef XK_Utilde
         XK_Utilde,  "XK_Utilde",
#endif
#ifdef XK_Umacron
         XK_Umacron,  "XK_Umacron",
#endif
#ifdef XK_amacron
         XK_amacron,  "XK_amacron",
#endif
#ifdef XK_iogonek
         XK_iogonek,  "XK_iogonek",
#endif
#ifdef XK_eabovedot
         XK_eabovedot,  "XK_eabovedot",
#endif
#ifdef XK_imacron
         XK_imacron,  "XK_imacron",
#endif
#ifdef XK_ncedilla
         XK_ncedilla,  "XK_ncedilla",
#endif
#ifdef XK_omacron
         XK_omacron,  "XK_omacron",
#endif
#ifdef XK_kcedilla
         XK_kcedilla,  "XK_kcedilla",
#endif
#ifdef XK_uogonek
         XK_uogonek,  "XK_uogonek",
#endif
#ifdef XK_utilde
         XK_utilde,  "XK_utilde",
#endif
#ifdef XK_umacron
         XK_umacron,  "XK_umacron",
#endif
#ifdef XK_overline
         XK_overline,  "XK_overline",
#endif
#ifdef XK_kana_fullstop
         XK_kana_fullstop,  "XK_kana_fullstop",
#endif
#ifdef XK_kana_openingbracket
         XK_kana_openingbracket,  "XK_kana_openingbracket",
#endif
#ifdef XK_kana_closingbracket
         XK_kana_closingbracket,  "XK_kana_closingbracket",
#endif
#ifdef XK_kana_comma
         XK_kana_comma,  "XK_kana_comma",
#endif
#ifdef XK_kana_conjunctive
         XK_kana_conjunctive,  "XK_kana_conjunctive",
#endif
#ifdef XK_kana_WO
         XK_kana_WO,  "XK_kana_WO",
#endif
#ifdef XK_kana_a
         XK_kana_a,  "XK_kana_a",
#endif
#ifdef XK_kana_i
         XK_kana_i,  "XK_kana_i",
#endif
#ifdef XK_kana_u
         XK_kana_u,  "XK_kana_u",
#endif
#ifdef XK_kana_e
         XK_kana_e,  "XK_kana_e",
#endif
#ifdef XK_kana_o
         XK_kana_o,  "XK_kana_o",
#endif
#ifdef XK_kana_ya
         XK_kana_ya,  "XK_kana_ya",
#endif
#ifdef XK_kana_yu
         XK_kana_yu,  "XK_kana_yu",
#endif
#ifdef XK_kana_yo
         XK_kana_yo,  "XK_kana_yo",
#endif
#ifdef XK_kana_tsu
         XK_kana_tsu,  "XK_kana_tsu",
#endif
#ifdef XK_prolongedsound
         XK_prolongedsound,  "XK_prolongedsound",
#endif
#ifdef XK_kana_A
         XK_kana_A,  "XK_kana_A",
#endif
#ifdef XK_kana_I
         XK_kana_I,  "XK_kana_I",
#endif
#ifdef XK_kana_U
         XK_kana_U,  "XK_kana_U",
#endif
#ifdef XK_kana_E
         XK_kana_E,  "XK_kana_E",
#endif
#ifdef XK_kana_O
         XK_kana_O,  "XK_kana_O",
#endif
#ifdef XK_kana_KA
         XK_kana_KA,  "XK_kana_KA",
#endif
#ifdef XK_kana_KI
         XK_kana_KI,  "XK_kana_KI",
#endif
#ifdef XK_kana_KU
         XK_kana_KU,  "XK_kana_KU",
#endif
#ifdef XK_kana_KE
         XK_kana_KE,  "XK_kana_KE",
#endif
#ifdef XK_kana_KO
         XK_kana_KO,  "XK_kana_KO",
#endif
#ifdef XK_kana_SA
         XK_kana_SA,  "XK_kana_SA",
#endif
#ifdef XK_kana_SHI
         XK_kana_SHI,  "XK_kana_SHI",
#endif
#ifdef XK_kana_SU
         XK_kana_SU,  "XK_kana_SU",
#endif
#ifdef XK_kana_SE
         XK_kana_SE,  "XK_kana_SE",
#endif
#ifdef XK_kana_SO
         XK_kana_SO,  "XK_kana_SO",
#endif
#ifdef XK_kana_TA
         XK_kana_TA,  "XK_kana_TA",
#endif
#ifdef XK_kana_CHI
         XK_kana_CHI,  "XK_kana_CHI",
#endif
#ifdef XK_kana_TSU
         XK_kana_TSU,  "XK_kana_TSU",
#endif
#ifdef XK_kana_TE
         XK_kana_TE,  "XK_kana_TE",
#endif
#ifdef XK_kana_TO
         XK_kana_TO,  "XK_kana_TO",
#endif
#ifdef XK_kana_NA
         XK_kana_NA,  "XK_kana_NA",
#endif
#ifdef XK_kana_NI
         XK_kana_NI,  "XK_kana_NI",
#endif
#ifdef XK_kana_NU
         XK_kana_NU,  "XK_kana_NU",
#endif
#ifdef XK_kana_NE
         XK_kana_NE,  "XK_kana_NE",
#endif
#ifdef XK_kana_NO
         XK_kana_NO,  "XK_kana_NO",
#endif
#ifdef XK_kana_HA
         XK_kana_HA,  "XK_kana_HA",
#endif
#ifdef XK_kana_HI
         XK_kana_HI,  "XK_kana_HI",
#endif
#ifdef XK_kana_FU
         XK_kana_FU,  "XK_kana_FU",
#endif
#ifdef XK_kana_HE
         XK_kana_HE,  "XK_kana_HE",
#endif
#ifdef XK_kana_HO
         XK_kana_HO,  "XK_kana_HO",
#endif
#ifdef XK_kana_MA
         XK_kana_MA,  "XK_kana_MA",
#endif
#ifdef XK_kana_MI
         XK_kana_MI,  "XK_kana_MI",
#endif
#ifdef XK_kana_MU
         XK_kana_MU,  "XK_kana_MU",
#endif
#ifdef XK_kana_ME
         XK_kana_ME,  "XK_kana_ME",
#endif
#ifdef XK_kana_MO
         XK_kana_MO,  "XK_kana_MO",
#endif
#ifdef XK_kana_YA
         XK_kana_YA,  "XK_kana_YA",
#endif
#ifdef XK_kana_YU
         XK_kana_YU,  "XK_kana_YU",
#endif
#ifdef XK_kana_YO
         XK_kana_YO,  "XK_kana_YO",
#endif
#ifdef XK_kana_RA
         XK_kana_RA,  "XK_kana_RA",
#endif
#ifdef XK_kana_RI
         XK_kana_RI,  "XK_kana_RI",
#endif
#ifdef XK_kana_RU
         XK_kana_RU,  "XK_kana_RU",
#endif
#ifdef XK_kana_RE
         XK_kana_RE,  "XK_kana_RE",
#endif
#ifdef XK_kana_RO
         XK_kana_RO,  "XK_kana_RO",
#endif
#ifdef XK_kana_WA
         XK_kana_WA,  "XK_kana_WA",
#endif
#ifdef XK_kana_N
         XK_kana_N,  "XK_kana_N",
#endif
#ifdef XK_voicedsound
         XK_voicedsound,  "XK_voicedsound",
#endif
#ifdef XK_semivoicedsound
         XK_semivoicedsound,  "XK_semivoicedsound",
#endif
#ifdef XK_Arabic_comma
         XK_Arabic_comma,  "XK_Arabic_comma",
#endif
#ifdef XK_Arabic_semicolon
         XK_Arabic_semicolon,  "XK_Arabic_semicolon",
#endif
#ifdef XK_Arabic_question_mark
         XK_Arabic_question_mark,  "XK_Arabic_question_mark",
#endif
#ifdef XK_Arabic_hamza
         XK_Arabic_hamza,  "XK_Arabic_hamza",
#endif
#ifdef XK_Arabic_maddaonalef
         XK_Arabic_maddaonalef,  "XK_Arabic_maddaonalef",
#endif
#ifdef XK_Arabic_hamzaonalef
         XK_Arabic_hamzaonalef,  "XK_Arabic_hamzaonalef",
#endif
#ifdef XK_Arabic_hamzaonwaw
         XK_Arabic_hamzaonwaw,  "XK_Arabic_hamzaonwaw",
#endif
#ifdef XK_Arabic_hamzaunderalef
         XK_Arabic_hamzaunderalef,  "XK_Arabic_hamzaunderalef",
#endif
#ifdef XK_Arabic_hamzaonyeh
         XK_Arabic_hamzaonyeh,  "XK_Arabic_hamzaonyeh",
#endif
#ifdef XK_Arabic_alef
         XK_Arabic_alef,  "XK_Arabic_alef",
#endif
#ifdef XK_Arabic_beh
         XK_Arabic_beh,  "XK_Arabic_beh",
#endif
#ifdef XK_Arabic_tehmarbuta
         XK_Arabic_tehmarbuta,  "XK_Arabic_tehmarbuta",
#endif
#ifdef XK_Arabic_teh
         XK_Arabic_teh,  "XK_Arabic_teh",
#endif
#ifdef XK_Arabic_theh
         XK_Arabic_theh,  "XK_Arabic_theh",
#endif
#ifdef XK_Arabic_jeem
         XK_Arabic_jeem,  "XK_Arabic_jeem",
#endif
#ifdef XK_Arabic_hah
         XK_Arabic_hah,  "XK_Arabic_hah",
#endif
#ifdef XK_Arabic_khah
         XK_Arabic_khah,  "XK_Arabic_khah",
#endif
#ifdef XK_Arabic_dal
         XK_Arabic_dal,  "XK_Arabic_dal",
#endif
#ifdef XK_Arabic_thal
         XK_Arabic_thal,  "XK_Arabic_thal",
#endif
#ifdef XK_Arabic_ra
         XK_Arabic_ra,  "XK_Arabic_ra",
#endif
#ifdef XK_Arabic_zain
         XK_Arabic_zain,  "XK_Arabic_zain",
#endif
#ifdef XK_Arabic_seen
         XK_Arabic_seen,  "XK_Arabic_seen",
#endif
#ifdef XK_Arabic_sheen
         XK_Arabic_sheen,  "XK_Arabic_sheen",
#endif
#ifdef XK_Arabic_sad
         XK_Arabic_sad,  "XK_Arabic_sad",
#endif
#ifdef XK_Arabic_dad
         XK_Arabic_dad,  "XK_Arabic_dad",
#endif
#ifdef XK_Arabic_tah
         XK_Arabic_tah,  "XK_Arabic_tah",
#endif
#ifdef XK_Arabic_zah
         XK_Arabic_zah,  "XK_Arabic_zah",
#endif
#ifdef XK_Arabic_ain
         XK_Arabic_ain,  "XK_Arabic_ain",
#endif
#ifdef XK_Arabic_ghain
         XK_Arabic_ghain,  "XK_Arabic_ghain",
#endif
#ifdef XK_Arabic_tatweel
         XK_Arabic_tatweel,  "XK_Arabic_tatweel",
#endif
#ifdef XK_Arabic_feh
         XK_Arabic_feh,  "XK_Arabic_feh",
#endif
#ifdef XK_Arabic_qaf
         XK_Arabic_qaf,  "XK_Arabic_qaf",
#endif
#ifdef XK_Arabic_kaf
         XK_Arabic_kaf,  "XK_Arabic_kaf",
#endif
#ifdef XK_Arabic_lam
         XK_Arabic_lam,  "XK_Arabic_lam",
#endif
#ifdef XK_Arabic_meem
         XK_Arabic_meem,  "XK_Arabic_meem",
#endif
#ifdef XK_Arabic_noon
         XK_Arabic_noon,  "XK_Arabic_noon",
#endif
#ifdef XK_Arabic_ha
         XK_Arabic_ha,  "XK_Arabic_ha",
#endif
#ifdef XK_Arabic_waw
         XK_Arabic_waw,  "XK_Arabic_waw",
#endif
#ifdef XK_Arabic_alefmaksura
         XK_Arabic_alefmaksura,  "XK_Arabic_alefmaksura",
#endif
#ifdef XK_Arabic_yeh
         XK_Arabic_yeh,  "XK_Arabic_yeh",
#endif
#ifdef XK_Arabic_fathatan
         XK_Arabic_fathatan,  "XK_Arabic_fathatan",
#endif
#ifdef XK_Arabic_dammatan
         XK_Arabic_dammatan,  "XK_Arabic_dammatan",
#endif
#ifdef XK_Arabic_kasratan
         XK_Arabic_kasratan,  "XK_Arabic_kasratan",
#endif
#ifdef XK_Arabic_fatha
         XK_Arabic_fatha,  "XK_Arabic_fatha",
#endif
#ifdef XK_Arabic_damma
         XK_Arabic_damma,  "XK_Arabic_damma",
#endif
#ifdef XK_Arabic_kasra
         XK_Arabic_kasra,  "XK_Arabic_kasra",
#endif
#ifdef XK_Arabic_shadda
         XK_Arabic_shadda,  "XK_Arabic_shadda",
#endif
#ifdef XK_Arabic_sukun
         XK_Arabic_sukun,  "XK_Arabic_sukun",
#endif
#ifdef XK_Serbian_dje
         XK_Serbian_dje,  "XK_Serbian_dje",
#endif
#ifdef XK_Macedonia_gje
         XK_Macedonia_gje,  "XK_Macedonia_gje",
#endif
#ifdef XK_Cyrillic_io
         XK_Cyrillic_io,  "XK_Cyrillic_io",
#endif
#ifdef XK_Ukrainian_ie
         XK_Ukrainian_ie,  "XK_Ukrainian_ie",
#endif
#ifdef XK_Macedonia_dse
         XK_Macedonia_dse,  "XK_Macedonia_dse",
#endif
#ifdef XK_Ukrainian_i
         XK_Ukrainian_i,  "XK_Ukrainian_i",
#endif
#ifdef XK_Ukrainian_yi
         XK_Ukrainian_yi,  "XK_Ukrainian_yi",
#endif
#ifdef XK_Cyrillic_je
         XK_Cyrillic_je,  "XK_Cyrillic_je",
#endif
#ifdef XK_Cyrillic_lje
         XK_Cyrillic_lje,  "XK_Cyrillic_lje",
#endif
#ifdef XK_Cyrillic_nje
         XK_Cyrillic_nje,  "XK_Cyrillic_nje",
#endif
#ifdef XK_Serbian_tshe
         XK_Serbian_tshe,  "XK_Serbian_tshe",
#endif
#ifdef XK_Macedonia_kje
         XK_Macedonia_kje,  "XK_Macedonia_kje",
#endif
#ifdef XK_Byelorussian_shortu
         XK_Byelorussian_shortu,  "XK_Byelorussian_shortu",
#endif
#ifdef XK_Cyrillic_dzhe
         XK_Cyrillic_dzhe,  "XK_Cyrillic_dzhe",
#endif
#ifdef XK_numerosign
         XK_numerosign,  "XK_numerosign",
#endif
#ifdef XK_Serbian_DJE
         XK_Serbian_DJE,  "XK_Serbian_DJE",
#endif
#ifdef XK_Macedonia_GJE
         XK_Macedonia_GJE,  "XK_Macedonia_GJE",
#endif
#ifdef XK_Cyrillic_IO
         XK_Cyrillic_IO,  "XK_Cyrillic_IO",
#endif
#ifdef XK_Ukrainian_IE
         XK_Ukrainian_IE,  "XK_Ukrainian_IE",
#endif
#ifdef XK_Macedonia_DSE
         XK_Macedonia_DSE,  "XK_Macedonia_DSE",
#endif
#ifdef XK_Ukrainian_I
         XK_Ukrainian_I,  "XK_Ukrainian_I",
#endif
#ifdef XK_Ukrainian_YI
         XK_Ukrainian_YI,  "XK_Ukrainian_YI",
#endif
#ifdef XK_Cyrillic_JE
         XK_Cyrillic_JE,  "XK_Cyrillic_JE",
#endif
#ifdef XK_Cyrillic_LJE
         XK_Cyrillic_LJE,  "XK_Cyrillic_LJE",
#endif
#ifdef XK_Cyrillic_NJE
         XK_Cyrillic_NJE,  "XK_Cyrillic_NJE",
#endif
#ifdef XK_Serbian_TSHE
         XK_Serbian_TSHE,  "XK_Serbian_TSHE",
#endif
#ifdef XK_Macedonia_KJE
         XK_Macedonia_KJE,  "XK_Macedonia_KJE",
#endif
#ifdef XK_Byelorussian_SHORTU
         XK_Byelorussian_SHORTU,  "XK_Byelorussian_SHORTU",
#endif
#ifdef XK_Cyrillic_DZHE
         XK_Cyrillic_DZHE,  "XK_Cyrillic_DZHE",
#endif
#ifdef XK_Cyrillic_yu
         XK_Cyrillic_yu,  "XK_Cyrillic_yu",
#endif
#ifdef XK_Cyrillic_a
         XK_Cyrillic_a,  "XK_Cyrillic_a",
#endif
#ifdef XK_Cyrillic_be
         XK_Cyrillic_be,  "XK_Cyrillic_be",
#endif
#ifdef XK_Cyrillic_tse
         XK_Cyrillic_tse,  "XK_Cyrillic_tse",
#endif
#ifdef XK_Cyrillic_de
         XK_Cyrillic_de,  "XK_Cyrillic_de",
#endif
#ifdef XK_Cyrillic_ie
         XK_Cyrillic_ie,  "XK_Cyrillic_ie",
#endif
#ifdef XK_Cyrillic_ef
         XK_Cyrillic_ef,  "XK_Cyrillic_ef",
#endif
#ifdef XK_Cyrillic_ghe
         XK_Cyrillic_ghe,  "XK_Cyrillic_ghe",
#endif
#ifdef XK_Cyrillic_ha
         XK_Cyrillic_ha,  "XK_Cyrillic_ha",
#endif
#ifdef XK_Cyrillic_i
         XK_Cyrillic_i,  "XK_Cyrillic_i",
#endif
#ifdef XK_Cyrillic_shorti
         XK_Cyrillic_shorti,  "XK_Cyrillic_shorti",
#endif
#ifdef XK_Cyrillic_ka
         XK_Cyrillic_ka,  "XK_Cyrillic_ka",
#endif
#ifdef XK_Cyrillic_el
         XK_Cyrillic_el,  "XK_Cyrillic_el",
#endif
#ifdef XK_Cyrillic_em
         XK_Cyrillic_em,  "XK_Cyrillic_em",
#endif
#ifdef XK_Cyrillic_en
         XK_Cyrillic_en,  "XK_Cyrillic_en",
#endif
#ifdef XK_Cyrillic_o
         XK_Cyrillic_o,  "XK_Cyrillic_o",
#endif
#ifdef XK_Cyrillic_pe
         XK_Cyrillic_pe,  "XK_Cyrillic_pe",
#endif
#ifdef XK_Cyrillic_ya
         XK_Cyrillic_ya,  "XK_Cyrillic_ya",
#endif
#ifdef XK_Cyrillic_er
         XK_Cyrillic_er,  "XK_Cyrillic_er",
#endif
#ifdef XK_Cyrillic_es
         XK_Cyrillic_es,  "XK_Cyrillic_es",
#endif
#ifdef XK_Cyrillic_te
         XK_Cyrillic_te,  "XK_Cyrillic_te",
#endif
#ifdef XK_Cyrillic_u
         XK_Cyrillic_u,  "XK_Cyrillic_u",
#endif
#ifdef XK_Cyrillic_zhe
         XK_Cyrillic_zhe,  "XK_Cyrillic_zhe",
#endif
#ifdef XK_Cyrillic_ve
         XK_Cyrillic_ve,  "XK_Cyrillic_ve",
#endif
#ifdef XK_Cyrillic_softsign
         XK_Cyrillic_softsign,  "XK_Cyrillic_softsign",
#endif
#ifdef XK_Cyrillic_yeru
         XK_Cyrillic_yeru,  "XK_Cyrillic_yeru",
#endif
#ifdef XK_Cyrillic_ze
         XK_Cyrillic_ze,  "XK_Cyrillic_ze",
#endif
#ifdef XK_Cyrillic_sha
         XK_Cyrillic_sha,  "XK_Cyrillic_sha",
#endif
#ifdef XK_Cyrillic_e
         XK_Cyrillic_e,  "XK_Cyrillic_e",
#endif
#ifdef XK_Cyrillic_shcha
         XK_Cyrillic_shcha,  "XK_Cyrillic_shcha",
#endif
#ifdef XK_Cyrillic_che
         XK_Cyrillic_che,  "XK_Cyrillic_che",
#endif
#ifdef XK_Cyrillic_hardsign
         XK_Cyrillic_hardsign,  "XK_Cyrillic_hardsign",
#endif
#ifdef XK_Cyrillic_YU
         XK_Cyrillic_YU,  "XK_Cyrillic_YU",
#endif
#ifdef XK_Cyrillic_A
         XK_Cyrillic_A,  "XK_Cyrillic_A",
#endif
#ifdef XK_Cyrillic_BE
         XK_Cyrillic_BE,  "XK_Cyrillic_BE",
#endif
#ifdef XK_Cyrillic_TSE
         XK_Cyrillic_TSE,  "XK_Cyrillic_TSE",
#endif
#ifdef XK_Cyrillic_DE
         XK_Cyrillic_DE,  "XK_Cyrillic_DE",
#endif
#ifdef XK_Cyrillic_IE
         XK_Cyrillic_IE,  "XK_Cyrillic_IE",
#endif
#ifdef XK_Cyrillic_EF
         XK_Cyrillic_EF,  "XK_Cyrillic_EF",
#endif
#ifdef XK_Cyrillic_GHE
         XK_Cyrillic_GHE,  "XK_Cyrillic_GHE",
#endif
#ifdef XK_Cyrillic_HA
         XK_Cyrillic_HA,  "XK_Cyrillic_HA",
#endif
#ifdef XK_Cyrillic_I
         XK_Cyrillic_I,  "XK_Cyrillic_I",
#endif
#ifdef XK_Cyrillic_SHORTI
         XK_Cyrillic_SHORTI,  "XK_Cyrillic_SHORTI",
#endif
#ifdef XK_Cyrillic_KA
         XK_Cyrillic_KA,  "XK_Cyrillic_KA",
#endif
#ifdef XK_Cyrillic_EL
         XK_Cyrillic_EL,  "XK_Cyrillic_EL",
#endif
#ifdef XK_Cyrillic_EM
         XK_Cyrillic_EM,  "XK_Cyrillic_EM",
#endif
#ifdef XK_Cyrillic_EN
         XK_Cyrillic_EN,  "XK_Cyrillic_EN",
#endif
#ifdef XK_Cyrillic_O
         XK_Cyrillic_O,  "XK_Cyrillic_O",
#endif
#ifdef XK_Cyrillic_PE
         XK_Cyrillic_PE,  "XK_Cyrillic_PE",
#endif
#ifdef XK_Cyrillic_YA
         XK_Cyrillic_YA,  "XK_Cyrillic_YA",
#endif
#ifdef XK_Cyrillic_ER
         XK_Cyrillic_ER,  "XK_Cyrillic_ER",
#endif
#ifdef XK_Cyrillic_ES
         XK_Cyrillic_ES,  "XK_Cyrillic_ES",
#endif
#ifdef XK_Cyrillic_TE
         XK_Cyrillic_TE,  "XK_Cyrillic_TE",
#endif
#ifdef XK_Cyrillic_U
         XK_Cyrillic_U,  "XK_Cyrillic_U",
#endif
#ifdef XK_Cyrillic_ZHE
         XK_Cyrillic_ZHE,  "XK_Cyrillic_ZHE",
#endif
#ifdef XK_Cyrillic_VE
         XK_Cyrillic_VE,  "XK_Cyrillic_VE",
#endif
#ifdef XK_Cyrillic_SOFTSIGN
         XK_Cyrillic_SOFTSIGN,  "XK_Cyrillic_SOFTSIGN",
#endif
#ifdef XK_Cyrillic_YERU
         XK_Cyrillic_YERU,  "XK_Cyrillic_YERU",
#endif
#ifdef XK_Cyrillic_ZE
         XK_Cyrillic_ZE,  "XK_Cyrillic_ZE",
#endif
#ifdef XK_Cyrillic_SHA
         XK_Cyrillic_SHA,  "XK_Cyrillic_SHA",
#endif
#ifdef XK_Cyrillic_E
         XK_Cyrillic_E,  "XK_Cyrillic_E",
#endif
#ifdef XK_Cyrillic_SHCHA
         XK_Cyrillic_SHCHA,  "XK_Cyrillic_SHCHA",
#endif
#ifdef XK_Cyrillic_CHE
         XK_Cyrillic_CHE,  "XK_Cyrillic_CHE",
#endif
#ifdef XK_Cyrillic_HARDSIGN
         XK_Cyrillic_HARDSIGN,  "XK_Cyrillic_HARDSIGN",
#endif
#ifdef XK_Greek_ALPHAaccent
         XK_Greek_ALPHAaccent,  "XK_Greek_ALPHAaccent",
#endif
#ifdef XK_Greek_EPSILONaccent
         XK_Greek_EPSILONaccent,  "XK_Greek_EPSILONaccent",
#endif
#ifdef XK_Greek_ETAaccent
         XK_Greek_ETAaccent,  "XK_Greek_ETAaccent",
#endif
#ifdef XK_Greek_IOTAaccent
         XK_Greek_IOTAaccent,  "XK_Greek_IOTAaccent",
#endif
#ifdef XK_Greek_IOTAdiaeresis
         XK_Greek_IOTAdiaeresis,  "XK_Greek_IOTAdiaeresis",
#endif
#ifdef XK_Greek_OMICRONaccent
         XK_Greek_OMICRONaccent,  "XK_Greek_OMICRONaccent",
#endif
#ifdef XK_Greek_UPSILONaccent
         XK_Greek_UPSILONaccent,  "XK_Greek_UPSILONaccent",
#endif
#ifdef XK_Greek_UPSILONdieresis
         XK_Greek_UPSILONdieresis,  "XK_Greek_UPSILONdieresis",
#endif
#ifdef XK_Greek_OMEGAaccent
         XK_Greek_OMEGAaccent,  "XK_Greek_OMEGAaccent",
#endif
#ifdef XK_Greek_accentdieresis
         XK_Greek_accentdieresis,  "XK_Greek_accentdieresis",
#endif
#ifdef XK_Greek_horizbar
         XK_Greek_horizbar,  "XK_Greek_horizbar",
#endif
#ifdef XK_Greek_alphaaccent
         XK_Greek_alphaaccent,  "XK_Greek_alphaaccent",
#endif
#ifdef XK_Greek_epsilonaccent
         XK_Greek_epsilonaccent,  "XK_Greek_epsilonaccent",
#endif
#ifdef XK_Greek_etaaccent
         XK_Greek_etaaccent,  "XK_Greek_etaaccent",
#endif
#ifdef XK_Greek_iotaaccent
         XK_Greek_iotaaccent,  "XK_Greek_iotaaccent",
#endif
#ifdef XK_Greek_iotadieresis
         XK_Greek_iotadieresis,  "XK_Greek_iotadieresis",
#endif
#ifdef XK_Greek_iotaaccentdieresis
         XK_Greek_iotaaccentdieresis,  "XK_Greek_iotaaccentdieresis",
#endif
#ifdef XK_Greek_omicronaccent
         XK_Greek_omicronaccent,  "XK_Greek_omicronaccent",
#endif
#ifdef XK_Greek_upsilonaccent
         XK_Greek_upsilonaccent,  "XK_Greek_upsilonaccent",
#endif
#ifdef XK_Greek_upsilondieresis
         XK_Greek_upsilondieresis,  "XK_Greek_upsilondieresis",
#endif
#ifdef XK_Greek_upsilonaccentdieresis
         XK_Greek_upsilonaccentdieresis,  "XK_Greek_upsilonaccentdieresis",
#endif
#ifdef XK_Greek_omegaaccent
         XK_Greek_omegaaccent,  "XK_Greek_omegaaccent",
#endif
#ifdef XK_Greek_ALPHA
         XK_Greek_ALPHA,  "XK_Greek_ALPHA",
#endif
#ifdef XK_Greek_BETA
         XK_Greek_BETA,  "XK_Greek_BETA",
#endif
#ifdef XK_Greek_GAMMA
         XK_Greek_GAMMA,  "XK_Greek_GAMMA",
#endif
#ifdef XK_Greek_DELTA
         XK_Greek_DELTA,  "XK_Greek_DELTA",
#endif
#ifdef XK_Greek_EPSILON
         XK_Greek_EPSILON,  "XK_Greek_EPSILON",
#endif
#ifdef XK_Greek_ZETA
         XK_Greek_ZETA,  "XK_Greek_ZETA",
#endif
#ifdef XK_Greek_ETA
         XK_Greek_ETA,  "XK_Greek_ETA",
#endif
#ifdef XK_Greek_THETA
         XK_Greek_THETA,  "XK_Greek_THETA",
#endif
#ifdef XK_Greek_IOTA
         XK_Greek_IOTA,  "XK_Greek_IOTA",
#endif
#ifdef XK_Greek_KAPPA
         XK_Greek_KAPPA,  "XK_Greek_KAPPA",
#endif
#ifdef XK_Greek_LAMDA
         XK_Greek_LAMDA,  "XK_Greek_LAMDA",
#endif
#ifdef XK_Greek_MU
         XK_Greek_MU,  "XK_Greek_MU",
#endif
#ifdef XK_Greek_NU
         XK_Greek_NU,  "XK_Greek_NU",
#endif
#ifdef XK_Greek_XI
         XK_Greek_XI,  "XK_Greek_XI",
#endif
#ifdef XK_Greek_OMICRON
         XK_Greek_OMICRON,  "XK_Greek_OMICRON",
#endif
#ifdef XK_Greek_PI
         XK_Greek_PI,  "XK_Greek_PI",
#endif
#ifdef XK_Greek_RHO
         XK_Greek_RHO,  "XK_Greek_RHO",
#endif
#ifdef XK_Greek_SIGMA
         XK_Greek_SIGMA,  "XK_Greek_SIGMA",
#endif
#ifdef XK_Greek_TAU
         XK_Greek_TAU,  "XK_Greek_TAU",
#endif
#ifdef XK_Greek_UPSILON
         XK_Greek_UPSILON,  "XK_Greek_UPSILON",
#endif
#ifdef XK_Greek_PHI
         XK_Greek_PHI,  "XK_Greek_PHI",
#endif
#ifdef XK_Greek_CHI
         XK_Greek_CHI,  "XK_Greek_CHI",
#endif
#ifdef XK_Greek_PSI
         XK_Greek_PSI,  "XK_Greek_PSI",
#endif
#ifdef XK_Greek_OMEGA
         XK_Greek_OMEGA,  "XK_Greek_OMEGA",
#endif
#ifdef XK_Greek_alpha
         XK_Greek_alpha,  "XK_Greek_alpha",
#endif
#ifdef XK_Greek_beta
         XK_Greek_beta,  "XK_Greek_beta",
#endif
#ifdef XK_Greek_gamma
         XK_Greek_gamma,  "XK_Greek_gamma",
#endif
#ifdef XK_Greek_delta
         XK_Greek_delta,  "XK_Greek_delta",
#endif
#ifdef XK_Greek_epsilon
         XK_Greek_epsilon,  "XK_Greek_epsilon",
#endif
#ifdef XK_Greek_zeta
         XK_Greek_zeta,  "XK_Greek_zeta",
#endif
#ifdef XK_Greek_eta
         XK_Greek_eta,  "XK_Greek_eta",
#endif
#ifdef XK_Greek_theta
         XK_Greek_theta,  "XK_Greek_theta",
#endif
#ifdef XK_Greek_iota
         XK_Greek_iota,  "XK_Greek_iota",
#endif
#ifdef XK_Greek_kappa
         XK_Greek_kappa,  "XK_Greek_kappa",
#endif
#ifdef XK_Greek_lamda
         XK_Greek_lamda,  "XK_Greek_lamda",
#endif
#ifdef XK_Greek_mu
         XK_Greek_mu,  "XK_Greek_mu",
#endif
#ifdef XK_Greek_nu
         XK_Greek_nu,  "XK_Greek_nu",
#endif
#ifdef XK_Greek_xi
         XK_Greek_xi,  "XK_Greek_xi",
#endif
#ifdef XK_Greek_omicron
         XK_Greek_omicron,  "XK_Greek_omicron",
#endif
#ifdef XK_Greek_pi
         XK_Greek_pi,  "XK_Greek_pi",
#endif
#ifdef XK_Greek_rho
         XK_Greek_rho,  "XK_Greek_rho",
#endif
#ifdef XK_Greek_sigma
         XK_Greek_sigma,  "XK_Greek_sigma",
#endif
#ifdef XK_Greek_finalsmallsigma
         XK_Greek_finalsmallsigma,  "XK_Greek_finalsmallsigma",
#endif
#ifdef XK_Greek_tau
         XK_Greek_tau,  "XK_Greek_tau",
#endif
#ifdef XK_Greek_upsilon
         XK_Greek_upsilon,  "XK_Greek_upsilon",
#endif
#ifdef XK_Greek_phi
         XK_Greek_phi,  "XK_Greek_phi",
#endif
#ifdef XK_Greek_chi
         XK_Greek_chi,  "XK_Greek_chi",
#endif
#ifdef XK_Greek_psi
         XK_Greek_psi,  "XK_Greek_psi",
#endif
#ifdef XK_Greek_omega
         XK_Greek_omega,  "XK_Greek_omega",
#endif
#ifdef XK_leftradical
         XK_leftradical,  "XK_leftradical",
#endif
#ifdef XK_topleftradical
         XK_topleftradical,  "XK_topleftradical",
#endif
#ifdef XK_horizconnector
         XK_horizconnector,  "XK_horizconnector",
#endif
#ifdef XK_topintegral
         XK_topintegral,  "XK_topintegral",
#endif
#ifdef XK_botintegral
         XK_botintegral,  "XK_botintegral",
#endif
#ifdef XK_vertconnector
         XK_vertconnector,  "XK_vertconnector",
#endif
#ifdef XK_topleftsqbracket
         XK_topleftsqbracket,  "XK_topleftsqbracket",
#endif
#ifdef XK_botleftsqbracket
         XK_botleftsqbracket,  "XK_botleftsqbracket",
#endif
#ifdef XK_toprightsqbracket
         XK_toprightsqbracket,  "XK_toprightsqbracket",
#endif
#ifdef XK_botrightsqbracket
         XK_botrightsqbracket,  "XK_botrightsqbracket",
#endif
#ifdef XK_topleftparens
         XK_topleftparens,  "XK_topleftparens",
#endif
#ifdef XK_botleftparens
         XK_botleftparens,  "XK_botleftparens",
#endif
#ifdef XK_toprightparens
         XK_toprightparens,  "XK_toprightparens",
#endif
#ifdef XK_botrightparens
         XK_botrightparens,  "XK_botrightparens",
#endif
#ifdef XK_leftmiddlecurlybrace
         XK_leftmiddlecurlybrace,  "XK_leftmiddlecurlybrace",
#endif
#ifdef XK_rightmiddlecurlybrace
         XK_rightmiddlecurlybrace,  "XK_rightmiddlecurlybrace",
#endif
#ifdef XK_topleftsummation
         XK_topleftsummation,  "XK_topleftsummation",
#endif
#ifdef XK_botleftsummation
         XK_botleftsummation,  "XK_botleftsummation",
#endif
#ifdef XK_topvertsummationconnector
         XK_topvertsummationconnector,  "XK_topvertsummationconnector",
#endif
#ifdef XK_botvertsummationconnector
         XK_botvertsummationconnector,  "XK_botvertsummationconnector",
#endif
#ifdef XK_toprightsummation
         XK_toprightsummation,  "XK_toprightsummation",
#endif
#ifdef XK_botrightsummation
         XK_botrightsummation,  "XK_botrightsummation",
#endif
#ifdef XK_rightmiddlesummation
         XK_rightmiddlesummation,  "XK_rightmiddlesummation",
#endif
#ifdef XK_lessthanequal
         XK_lessthanequal,  "XK_lessthanequal",
#endif
#ifdef XK_notequal
         XK_notequal,  "XK_notequal",
#endif
#ifdef XK_greaterthanequal
         XK_greaterthanequal,  "XK_greaterthanequal",
#endif
#ifdef XK_integral
         XK_integral,  "XK_integral",
#endif
#ifdef XK_therefore
         XK_therefore,  "XK_therefore",
#endif
#ifdef XK_variation
         XK_variation,  "XK_variation",
#endif
#ifdef XK_infinity
         XK_infinity,  "XK_infinity",
#endif
#ifdef XK_nabla
         XK_nabla,  "XK_nabla",
#endif
#ifdef XK_approximate
         XK_approximate,  "XK_approximate",
#endif
#ifdef XK_similarequal
         XK_similarequal,  "XK_similarequal",
#endif
#ifdef XK_ifonlyif
         XK_ifonlyif,  "XK_ifonlyif",
#endif
#ifdef XK_implies
         XK_implies,  "XK_implies",
#endif
#ifdef XK_identical
         XK_identical,  "XK_identical",
#endif
#ifdef XK_radical
         XK_radical,  "XK_radical",
#endif
#ifdef XK_includedin
         XK_includedin,  "XK_includedin",
#endif
#ifdef XK_includes
         XK_includes,  "XK_includes",
#endif
#ifdef XK_intersection
         XK_intersection,  "XK_intersection",
#endif
#ifdef XK_union
         XK_union,  "XK_union",
#endif
#ifdef XK_logicaland
         XK_logicaland,  "XK_logicaland",
#endif
#ifdef XK_logicalor
         XK_logicalor,  "XK_logicalor",
#endif
#ifdef XK_partialderivative
         XK_partialderivative,  "XK_partialderivative",
#endif
#ifdef XK_function
         XK_function,  "XK_function",
#endif
#ifdef XK_leftarrow
         XK_leftarrow,  "XK_leftarrow",
#endif
#ifdef XK_uparrow
         XK_uparrow,  "XK_uparrow",
#endif
#ifdef XK_rightarrow
         XK_rightarrow,  "XK_rightarrow",
#endif
#ifdef XK_downarrow
         XK_downarrow,  "XK_downarrow",
#endif
#ifdef XK_blank
         XK_blank,  "XK_blank",
#endif
#ifdef XK_soliddiamond
         XK_soliddiamond,  "XK_soliddiamond",
#endif
#ifdef XK_checkerboard
         XK_checkerboard,  "XK_checkerboard",
#endif
#ifdef XK_ht
         XK_ht,  "XK_ht",
#endif
#ifdef XK_ff
         XK_ff,  "XK_ff",
#endif
#ifdef XK_cr
         XK_cr,  "XK_cr",
#endif
#ifdef XK_lf
         XK_lf,  "XK_lf",
#endif
#ifdef XK_nl
         XK_nl,  "XK_nl",
#endif
#ifdef XK_vt
         XK_vt,  "XK_vt",
#endif
#ifdef XK_lowrightcorner
         XK_lowrightcorner,  "XK_lowrightcorner",
#endif
#ifdef XK_uprightcorner
         XK_uprightcorner,  "XK_uprightcorner",
#endif
#ifdef XK_upleftcorner
         XK_upleftcorner,  "XK_upleftcorner",
#endif
#ifdef XK_lowleftcorner
         XK_lowleftcorner,  "XK_lowleftcorner",
#endif
#ifdef XK_crossinglines
         XK_crossinglines,  "XK_crossinglines",
#endif
#ifdef XK_horizlinescan1
         XK_horizlinescan1,  "XK_horizlinescan1",
#endif
#ifdef XK_horizlinescan3
         XK_horizlinescan3,  "XK_horizlinescan3",
#endif
#ifdef XK_horizlinescan5
         XK_horizlinescan5,  "XK_horizlinescan5",
#endif
#ifdef XK_horizlinescan7
         XK_horizlinescan7,  "XK_horizlinescan7",
#endif
#ifdef XK_horizlinescan9
         XK_horizlinescan9,  "XK_horizlinescan9",
#endif
#ifdef XK_leftt
         XK_leftt,  "XK_leftt",
#endif
#ifdef XK_rightt
         XK_rightt,  "XK_rightt",
#endif
#ifdef XK_bott
         XK_bott,  "XK_bott",
#endif
#ifdef XK_topt
         XK_topt,  "XK_topt",
#endif
#ifdef XK_vertbar
         XK_vertbar,  "XK_vertbar",
#endif
#ifdef XK_emspace
         XK_emspace,  "XK_emspace",
#endif
#ifdef XK_enspace
         XK_enspace,  "XK_enspace",
#endif
#ifdef XK_em3space
         XK_em3space,  "XK_em3space",
#endif
#ifdef XK_em4space
         XK_em4space,  "XK_em4space",
#endif
#ifdef XK_digitspace
         XK_digitspace,  "XK_digitspace",
#endif
#ifdef XK_punctspace
         XK_punctspace,  "XK_punctspace",
#endif
#ifdef XK_thinspace
         XK_thinspace,  "XK_thinspace",
#endif
#ifdef XK_hairspace
         XK_hairspace,  "XK_hairspace",
#endif
#ifdef XK_emdash
         XK_emdash,  "XK_emdash",
#endif
#ifdef XK_endash
         XK_endash,  "XK_endash",
#endif
#ifdef XK_signifblank
         XK_signifblank,  "XK_signifblank",
#endif
#ifdef XK_ellipsis
         XK_ellipsis,  "XK_ellipsis",
#endif
#ifdef XK_doubbaselinedot
         XK_doubbaselinedot,  "XK_doubbaselinedot",
#endif
#ifdef XK_onethird
         XK_onethird,  "XK_onethird",
#endif
#ifdef XK_twothirds
         XK_twothirds,  "XK_twothirds",
#endif
#ifdef XK_onefifth
         XK_onefifth,  "XK_onefifth",
#endif
#ifdef XK_twofifths
         XK_twofifths,  "XK_twofifths",
#endif
#ifdef XK_threefifths
         XK_threefifths,  "XK_threefifths",
#endif
#ifdef XK_fourfifths
         XK_fourfifths,  "XK_fourfifths",
#endif
#ifdef XK_onesixth
         XK_onesixth,  "XK_onesixth",
#endif
#ifdef XK_fivesixths
         XK_fivesixths,  "XK_fivesixths",
#endif
#ifdef XK_careof
         XK_careof,  "XK_careof",
#endif
#ifdef XK_figdash
         XK_figdash,  "XK_figdash",
#endif
#ifdef XK_leftanglebracket
         XK_leftanglebracket,  "XK_leftanglebracket",
#endif
#ifdef XK_decimalpoint
         XK_decimalpoint,  "XK_decimalpoint",
#endif
#ifdef XK_rightanglebracket
         XK_rightanglebracket,  "XK_rightanglebracket",
#endif
#ifdef XK_marker
         XK_marker,  "XK_marker",
#endif
#ifdef XK_oneeighth
         XK_oneeighth,  "XK_oneeighth",
#endif
#ifdef XK_threeeighths
         XK_threeeighths,  "XK_threeeighths",
#endif
#ifdef XK_fiveeighths
         XK_fiveeighths,  "XK_fiveeighths",
#endif
#ifdef XK_seveneighths
         XK_seveneighths,  "XK_seveneighths",
#endif
#ifdef XK_trademark
         XK_trademark,  "XK_trademark",
#endif
#ifdef XK_signaturemark
         XK_signaturemark,  "XK_signaturemark",
#endif
#ifdef XK_trademarkincircle
         XK_trademarkincircle,  "XK_trademarkincircle",
#endif
#ifdef XK_leftopentriangle
         XK_leftopentriangle,  "XK_leftopentriangle",
#endif
#ifdef XK_rightopentriangle
         XK_rightopentriangle,  "XK_rightopentriangle",
#endif
#ifdef XK_emopencircle
         XK_emopencircle,  "XK_emopencircle",
#endif
#ifdef XK_emopenrectangle
         XK_emopenrectangle,  "XK_emopenrectangle",
#endif
#ifdef XK_leftsinglequotemark
         XK_leftsinglequotemark,  "XK_leftsinglequotemark",
#endif
#ifdef XK_rightsinglequotemark
         XK_rightsinglequotemark,  "XK_rightsinglequotemark",
#endif
#ifdef XK_leftdoublequotemark
         XK_leftdoublequotemark,  "XK_leftdoublequotemark",
#endif
#ifdef XK_rightdoublequotemark
         XK_rightdoublequotemark,  "XK_rightdoublequotemark",
#endif
#ifdef XK_prescription
         XK_prescription,  "XK_prescription",
#endif
#ifdef XK_minutes
         XK_minutes,  "XK_minutes",
#endif
#ifdef XK_seconds
         XK_seconds,  "XK_seconds",
#endif
#ifdef XK_latincross
         XK_latincross,  "XK_latincross",
#endif
#ifdef XK_hexagram
         XK_hexagram,  "XK_hexagram",
#endif
#ifdef XK_filledrectbullet
         XK_filledrectbullet,  "XK_filledrectbullet",
#endif
#ifdef XK_filledlefttribullet
         XK_filledlefttribullet,  "XK_filledlefttribullet",
#endif
#ifdef XK_filledrighttribullet
         XK_filledrighttribullet,  "XK_filledrighttribullet",
#endif
#ifdef XK_emfilledcircle
         XK_emfilledcircle,  "XK_emfilledcircle",
#endif
#ifdef XK_emfilledrect
         XK_emfilledrect,  "XK_emfilledrect",
#endif
#ifdef XK_enopencircbullet
         XK_enopencircbullet,  "XK_enopencircbullet",
#endif
#ifdef XK_enopensquarebullet
         XK_enopensquarebullet,  "XK_enopensquarebullet",
#endif
#ifdef XK_openrectbullet
         XK_openrectbullet,  "XK_openrectbullet",
#endif
#ifdef XK_opentribulletup
         XK_opentribulletup,  "XK_opentribulletup",
#endif
#ifdef XK_opentribulletdown
         XK_opentribulletdown,  "XK_opentribulletdown",
#endif
#ifdef XK_openstar
         XK_openstar,  "XK_openstar",
#endif
#ifdef XK_enfilledcircbullet
         XK_enfilledcircbullet,  "XK_enfilledcircbullet",
#endif
#ifdef XK_enfilledsqbullet
         XK_enfilledsqbullet,  "XK_enfilledsqbullet",
#endif
#ifdef XK_filledtribulletup
         XK_filledtribulletup,  "XK_filledtribulletup",
#endif
#ifdef XK_filledtribulletdown
         XK_filledtribulletdown,  "XK_filledtribulletdown",
#endif
#ifdef XK_leftpointer
         XK_leftpointer,  "XK_leftpointer",
#endif
#ifdef XK_rightpointer
         XK_rightpointer,  "XK_rightpointer",
#endif
#ifdef XK_club
         XK_club,  "XK_club",
#endif
#ifdef XK_diamond
         XK_diamond,  "XK_diamond",
#endif
#ifdef XK_heart
         XK_heart,  "XK_heart",
#endif
#ifdef XK_maltesecross
         XK_maltesecross,  "XK_maltesecross",
#endif
#ifdef XK_dagger
         XK_dagger,  "XK_dagger",
#endif
#ifdef XK_doubledagger
         XK_doubledagger,  "XK_doubledagger",
#endif
#ifdef XK_checkmark
         XK_checkmark,  "XK_checkmark",
#endif
#ifdef XK_ballotcross
         XK_ballotcross,  "XK_ballotcross",
#endif
#ifdef XK_musicalsharp
         XK_musicalsharp,  "XK_musicalsharp",
#endif
#ifdef XK_musicalflat
         XK_musicalflat,  "XK_musicalflat",
#endif
#ifdef XK_malesymbol
         XK_malesymbol,  "XK_malesymbol",
#endif
#ifdef XK_femalesymbol
         XK_femalesymbol,  "XK_femalesymbol",
#endif
#ifdef XK_telephone
         XK_telephone,  "XK_telephone",
#endif
#ifdef XK_telephonerecorder
         XK_telephonerecorder,  "XK_telephonerecorder",
#endif
#ifdef XK_phonographcopyright
         XK_phonographcopyright,  "XK_phonographcopyright",
#endif
#ifdef XK_caret
         XK_caret,  "XK_caret",
#endif
#ifdef XK_singlelowquotemark
         XK_singlelowquotemark,  "XK_singlelowquotemark",
#endif
#ifdef XK_doublelowquotemark
         XK_doublelowquotemark,  "XK_doublelowquotemark",
#endif
#ifdef XK_cursor
         XK_cursor,  "XK_cursor",
#endif
#ifdef XK_leftcaret
         XK_leftcaret,  "XK_leftcaret",
#endif
#ifdef XK_rightcaret
         XK_rightcaret,  "XK_rightcaret",
#endif
#ifdef XK_downcaret
         XK_downcaret,  "XK_downcaret",
#endif
#ifdef XK_upcaret
         XK_upcaret,  "XK_upcaret",
#endif
#ifdef XK_overbar
         XK_overbar,  "XK_overbar",
#endif
#ifdef XK_downtack
         XK_downtack,  "XK_downtack",
#endif
#ifdef XK_upshoe
         XK_upshoe,  "XK_upshoe",
#endif
#ifdef XK_downstile
         XK_downstile,  "XK_downstile",
#endif
#ifdef XK_underbar
         XK_underbar,  "XK_underbar",
#endif
#ifdef XK_jot
         XK_jot,  "XK_jot",
#endif
#ifdef XK_quad
         XK_quad,  "XK_quad",
#endif
#ifdef XK_uptack
         XK_uptack,  "XK_uptack",
#endif
#ifdef XK_circle
         XK_circle,  "XK_circle",
#endif
#ifdef XK_upstile
         XK_upstile,  "XK_upstile",
#endif
#ifdef XK_downshoe
         XK_downshoe,  "XK_downshoe",
#endif
#ifdef XK_rightshoe
         XK_rightshoe,  "XK_rightshoe",
#endif
#ifdef XK_leftshoe
         XK_leftshoe,  "XK_leftshoe",
#endif
#ifdef XK_lefttack
         XK_lefttack,  "XK_lefttack",
#endif
#ifdef XK_righttack
         XK_righttack,  "XK_righttack",
#endif
#ifdef XK_hebrew_doublelowline
         XK_hebrew_doublelowline,  "XK_hebrew_doublelowline",
#endif
#ifdef XK_hebrew_aleph
         XK_hebrew_aleph,  "XK_hebrew_aleph",
#endif
#ifdef XK_hebrew_bet
         XK_hebrew_bet,  "XK_hebrew_bet",
#endif
#ifdef XK_hebrew_gimel
         XK_hebrew_gimel,  "XK_hebrew_gimel",
#endif
#ifdef XK_hebrew_dalet
         XK_hebrew_dalet,  "XK_hebrew_dalet",
#endif
#ifdef XK_hebrew_he
         XK_hebrew_he,  "XK_hebrew_he",
#endif
#ifdef XK_hebrew_waw
         XK_hebrew_waw,  "XK_hebrew_waw",
#endif
#ifdef XK_hebrew_zain
         XK_hebrew_zain,  "XK_hebrew_zain",
#endif
#ifdef XK_hebrew_chet
         XK_hebrew_chet,  "XK_hebrew_chet",
#endif
#ifdef XK_hebrew_tet
         XK_hebrew_tet,  "XK_hebrew_tet",
#endif
#ifdef XK_hebrew_yod
         XK_hebrew_yod,  "XK_hebrew_yod",
#endif
#ifdef XK_hebrew_finalkaph
         XK_hebrew_finalkaph,  "XK_hebrew_finalkaph",
#endif
#ifdef XK_hebrew_kaph
         XK_hebrew_kaph,  "XK_hebrew_kaph",
#endif
#ifdef XK_hebrew_lamed
         XK_hebrew_lamed,  "XK_hebrew_lamed",
#endif
#ifdef XK_hebrew_finalmem
         XK_hebrew_finalmem,  "XK_hebrew_finalmem",
#endif
#ifdef XK_hebrew_mem
         XK_hebrew_mem,  "XK_hebrew_mem",
#endif
#ifdef XK_hebrew_finalnun
         XK_hebrew_finalnun,  "XK_hebrew_finalnun",
#endif
#ifdef XK_hebrew_nun
         XK_hebrew_nun,  "XK_hebrew_nun",
#endif
#ifdef XK_hebrew_samech
         XK_hebrew_samech,  "XK_hebrew_samech",
#endif
#ifdef XK_hebrew_ayin
         XK_hebrew_ayin,  "XK_hebrew_ayin",
#endif
#ifdef XK_hebrew_finalpe
         XK_hebrew_finalpe,  "XK_hebrew_finalpe",
#endif
#ifdef XK_hebrew_pe
         XK_hebrew_pe,  "XK_hebrew_pe",
#endif
#ifdef XK_hebrew_finalzade
         XK_hebrew_finalzade,  "XK_hebrew_finalzade",
#endif
#ifdef XK_hebrew_zade
         XK_hebrew_zade,  "XK_hebrew_zade",
#endif
#ifdef XK_hebrew_qoph
         XK_hebrew_qoph,  "XK_hebrew_qoph",
#endif
#ifdef XK_hebrew_resh
         XK_hebrew_resh,  "XK_hebrew_resh",
#endif
#ifdef XK_hebrew_shin
         XK_hebrew_shin,  "XK_hebrew_shin",
#endif
#ifdef XK_hebrew_taw
         XK_hebrew_taw,  "XK_hebrew_taw",
#endif
#ifdef XK_Reset
         XK_Reset,  "XK_Reset",
#endif
#ifdef XK_System
         XK_System,  "XK_System",
#endif
#ifdef XK_User
         XK_User,  "XK_User",
#endif
#ifdef XK_ClearLine
         XK_ClearLine,  "XK_ClearLine",
#endif
#ifdef XK_InsertLine
         XK_InsertLine,  "XK_InsertLine",
#endif
#ifdef XK_DeleteLine
         XK_DeleteLine,  "XK_DeleteLine",
#endif
#ifdef XK_InsertChar
         XK_InsertChar,  "XK_InsertChar",
#endif
#ifdef XK_DeleteChar
         XK_DeleteChar,  "XK_DeleteChar",
#endif
#ifdef XK_BackTab
         XK_BackTab,  "XK_BackTab",
#endif
#ifdef XK_KP_BackTab
         XK_KP_BackTab,  "XK_KP_BackTab",
#endif
#ifdef XK_Ext16bit_L
         XK_Ext16bit_L,  "XK_Ext16bit_L",
#endif
#ifdef XK_Ext16bit_R
         XK_Ext16bit_R,  "XK_Ext16bit_R",
#endif
#ifdef hpXK_Modelock1
         hpXK_Modelock1,  "hpXK_Modelock1",
#endif
#ifdef hpXK_Modelock2
         hpXK_Modelock2,  "hpXK_Modelock2",
#endif
#ifdef XK_mute_acute
         XK_mute_acute,  "XK_mute_acute",
#endif
#ifdef XK_mute_grave
         XK_mute_grave,  "XK_mute_grave",
#endif
#ifdef XK_mute_asciicircum
         XK_mute_asciicircum,  "XK_mute_asciicircum",
#endif
#ifdef XK_mute_diaeresis
         XK_mute_diaeresis,  "XK_mute_diaeresis",
#endif
#ifdef XK_mute_asciitilde
         XK_mute_asciitilde,  "XK_mute_asciitilde",
#endif
#ifdef XK_lira
         XK_lira,  "XK_lira",
#endif
#ifdef XK_guilder
         XK_guilder,  "XK_guilder",
#endif
#ifdef XK_Ydiaeresis
         XK_Ydiaeresis,  "XK_Ydiaeresis",
#endif
#ifdef XK_longminus
         XK_longminus,  "XK_longminus",
#endif
#ifdef XK_block
         XK_block,  "XK_block",
#endif
#ifdef osfXK_BackSpace
         osfXK_BackSpace,  "osfXK_BackSpace",
#endif
#ifdef osfXK_Insert
         osfXK_Insert,  "osfXK_Insert",
#endif
#ifdef osfXK_Delete
         osfXK_Delete,  "osfXK_Delete",
#endif
#ifdef osfXK_Copy
         osfXK_Copy,  "osfXK_Copy",
#endif
#ifdef osfXK_Cut
         osfXK_Cut,  "osfXK_Cut",
#endif
#ifdef osfXK_Paste
         osfXK_Paste,  "osfXK_Paste",
#endif
#ifdef osfXK_AddMode
         osfXK_AddMode,  "osfXK_AddMode",
#endif
#ifdef osfXK_PrimaryPaste
         osfXK_PrimaryPaste,  "osfXK_PrimaryPaste",
#endif
#ifdef osfXK_QuickPaste
         osfXK_QuickPaste,  "osfXK_QuickPaste",
#endif
#ifdef osfXK_PageUp
         osfXK_PageUp,  "osfXK_PageUp",
#endif
#ifdef osfXK_PageDown
         osfXK_PageDown,  "osfXK_PageDown",
#endif
#ifdef osfXK_EndLine
         osfXK_EndLine,  "osfXK_EndLine",
#endif
#ifdef osfXK_BeginLine
         osfXK_BeginLine,  "osfXK_BeginLine",
#endif
#ifdef osfXK_Activate
         osfXK_Activate,  "osfXK_Activate",
#endif
#ifdef osfXK_MenuBar
         osfXK_MenuBar,  "osfXK_MenuBar",
#endif
#ifdef osfXK_Clear
         osfXK_Clear,  "osfXK_Clear",
#endif
#ifdef osfXK_Cancel
         osfXK_Cancel,  "osfXK_Cancel",
#endif
#ifdef osfXK_Help
         osfXK_Help,  "osfXK_Help",
#endif
#ifdef osfXK_Menu
         osfXK_Menu,  "osfXK_Menu",
#endif
#ifdef osfXK_Select
         osfXK_Select,  "osfXK_Select",
#endif
#ifdef osfXK_Undo
         osfXK_Undo,  "osfXK_Undo",
#endif
#ifdef osfXK_Left
         osfXK_Left,  "osfXK_Left",
#endif
#ifdef osfXK_Up
         osfXK_Up,  "osfXK_Up",
#endif
#ifdef osfXK_Right
         osfXK_Right,  "osfXK_Right",
#endif
#ifdef osfXK_Down
         osfXK_Down,  "osfXK_Down",
#endif
#ifdef apXK_LineDel
         apXK_LineDel,  "apXK_LineDel",
#endif
#ifdef apXK_CharDel
         apXK_CharDel,  "apXK_CharDel",
#endif
#ifdef apXK_Copy
         apXK_Copy,  "apXK_Copy",
#endif
#ifdef apXK_Cut
         apXK_Cut,  "apXK_Cut",
#endif
#ifdef apXK_Paste
         apXK_Paste,  "apXK_Paste",
#endif
#ifdef apXK_Move
         apXK_Move,  "apXK_Move",
#endif
#ifdef apXK_Grow
         apXK_Grow,  "apXK_Grow",
#endif
#ifdef apXK_Cmd
         apXK_Cmd,  "apXK_Cmd",
#endif
#ifdef apXK_Shell
         apXK_Shell,  "apXK_Shell",
#endif
#ifdef apXK_LeftBar
         apXK_LeftBar,  "apXK_LeftBar",
#endif
#ifdef apXK_RightBar
         apXK_RightBar,  "apXK_RightBar",
#endif
#ifdef apXK_LeftBox
         apXK_LeftBox,  "apXK_LeftBox",
#endif
#ifdef apXK_RightBox
         apXK_RightBox,  "apXK_RightBox",
#endif
#ifdef apXK_UpBox
         apXK_UpBox,  "apXK_UpBox",
#endif
#ifdef apXK_DownBox
         apXK_DownBox,  "apXK_DownBox",
#endif
#ifdef apXK_Pop
         apXK_Pop,  "apXK_Pop",
#endif
#ifdef apXK_Read
         apXK_Read,  "apXK_Read",
#endif
#ifdef apXK_Edit
         apXK_Edit,  "apXK_Edit",
#endif
#ifdef apXK_Save
         apXK_Save,  "apXK_Save",
#endif
#ifdef apXK_Exit
         apXK_Exit,  "apXK_Exit",
#endif
#ifdef apXK_Repeat
         apXK_Repeat,  "apXK_Repeat",
#endif
#ifdef apXK_KP_parenleft
         apXK_KP_parenleft,  "apXK_KP_parenleft",
#endif
#ifdef apXK_KP_parenright
         apXK_KP_parenright,  "apXK_KP_parenright",
#endif
         0, "<NO_TRANSLATION>"};

#define MAX_KEY 978

char  *translate_keysym(KeySym  key)
{

/*char      *name;*/
int        i;

for (i = 0; i < MAX_KEY; i++)
   if (key == key_table[i].sym)
      return(key_table[i].val);

return("<NO_TRANSLATION>");

} /* end of translate_keysym */
KeySym  keyname_to_sym(char    *name)
{

/*KeySym    *keysym;*/
int        i;

for (i = 0; i < MAX_KEY; i++)
   if (strcmp(name, key_table[i].val) == 0)
      return(key_table[i].sym);

return(NoSymbol);

} /* end of keyname_to_sym */
#endif

