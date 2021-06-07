// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/utfdef.h
 *
 * UTF-8 types and macro constants definitions.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_UTFDEF_H_
#define DF_UTFDEF_H_

#include "xpt.h"

/// 32-bits unsigned type capable of holding any UTF-8 character.
typedef Uint32 Rune;

#define MAXUTF    4u         ///< Maximum bytes per `Rune`
#define RUNE_SYNC 0x80       ///< Cannot represent part of a UTF sequence (<)
#define RUNE_SELF 0x80       ///< `Rune` and UTF sequences are the same (<)
#define RUNE_ERR  0xfffdu    ///< Decoding error in UTF
#define MAXRUNE   0x10ffffu  ///< Maximum `Rune` value
#define BOM       0xefbbbfu  ///< UTF-8 BOM marker

#endif
