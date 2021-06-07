// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/vt100.h
 *
 * VT100 compliant control code sequences.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_VT100_H_
#define DF_SYS_VT100_H_

#ifndef STR
#define STR(x) #x
#endif
#ifndef XSTR
#define XSTR(x) STR(x)
#endif

/// VT100 escape character constant, prefixed to any command
#define VTESC "\x1b"

/// Operating System Command marker, immediately following ESC
#define VTOSC "]"
/// CSI marker, immediately following ESC
#define VTCSI "["

/// Enabling command suffix
#define VTENB "h"
/// Disabling command suffix
#define VTDIS "l"

/// Designate Character Set - DEC Line mode drawing
#define VTDCSLIN VTESC "(0"
/// Designate Character Set - US ASCII (default) mode
#define VTDCSCHR VTESC "(B"

/// Soft Terminal Reset (reset terminal to default state)
#define VTSRST VTESC VTCSI "!p"

/// Cursor blink command mnemonic
#define ATT160  "?12"
/// Text Cursor Enable Mode command mnemonic
#define DECTCEM "?25"

/// Start cursor blinking
#define VTBLKENB VTESC VTCSI ATT160 VTENB
/// Stop cursor blinking
#define VTBLKDIS VTESC VTCSI ATT160 VTDIS
/// Show cursor inside console
#define VTCURSHW VTESC VTCSI DECTCEM VTENB
/// Hide console cursor
#define VTCURHID VTESC VTCSI DECTCEM VTDIS

/// Console Screen Buffer command mnemonic
#define DECSCRB "?1049"

/// Switch to Alternate Screen buffer
#define VTALTSCR  VTESC VTCSI DECSCRB VTENB
/// Switch to Main Screen buffer
#define VTMAINSCR VTESC VTCSI DECSCRB VTDIS

/// Set Graphic Rendition command mnemonic
#define SGR "m"

/// Set Graphics Rendition to `N` (N is any of the SGR constants)
#define VTSGR(n) VTESC VTCSI XSTR(n) SGR

/// Obtain the corresponding background color code from a foreground color
#define VT_TOBG(c)  ((c) + 10)
/// Obtain the corresponding bold/emphasized variant from a foreground or background color
#define VT_TOBLD(c) ((c) + 60)

/// Reset graphics rendition to its default mode (both foreground and background)
#define VTDFLT   0
/// Enable bold text/emphasis
#define VTBLD    1
/// Disable bold text/emphasis
#define VTNOBLD  22
/// Enable text underline
#define VTUND    4
/// Disable text underline
#define VTNOUND  24
/// Invert foreground and background color
#define VTINV    7
/// Restore foreground and background to their normal value
#define VTNOINV  27

/// Black color code (foreground)
#define VTBLK    30
/// Red color code (foreground)
#define VTRED    31
/// Green color code (foreground)
#define VTGRN    32
/// Yellow color code (foreground)
#define VTYEL    33
/// Blue color code (foreground)
#define VTBLUE   34
/// Magenta color code (foreground)
#define VTMAGN   35
/// Cyan color code (foreground)
#define VTCYAN   36
/// White color code (foreground)
#define VTWHIT   37

/// Code to restore foreground color to its default value
#define VTFGDFLT 39

#define VTBGBLK  40
#define VTBGRED  41
#define VTBGGRN  42
#define VTBGYEL  43
#define VTBGBLUE 44
#define VTBGMAGN 45
#define VTBGCYAN 46
#define VTBGWHIT 47

#define VTBGDFLT 49

#define VTBLK_BLD  90
#define VTRED_BLD  91
#define VTGRN_BLD  92
#define VTYEL_BLD  93
#define VTBLUE_BLD 94
#define VTMAGN_BLD 95
#define VTCYAN_BLD 96
#define VTWHIT_BLD 97

#define VTBGBLK_BLD  100
#define VTBGRED_BLD  101
#define VTBGGRN_BLD  102
#define VTBGYEL_BLD  103
#define VTBGBLUE_BLD 104
#define VTBGMAGN_BLD 105
#define VTBGCYAN_BLD 106
#define VTBGWHIT_BLD 107

/// Erase in Display command mnemonic
#define ED "J"
/// Erase in Line command mnemonic
#define EL "K"

/// Erase in Display with command parameter `n`
#define VTED(n) VTESC VTCSI XSTR(n) ED
/// Erase in Line with command parameter `n`
#define VTEL(n) VTESC VTCSI XSTR(n) EL

/// Erase from cursor (inclusive) to end of display/line
#define VTCUR 0
/// Erase from the beginning of the line/display to end
#define VTSET 1
/// Erase everything in line/display
#define VTALL 2

#endif
