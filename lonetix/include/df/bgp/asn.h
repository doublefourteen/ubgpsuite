// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/asn.h
 *
 * Types, constant and utilities to work with ASN of various width.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_ASN_H_
#define DF_BGP_ASN_H_

#include "xpt.h"

/// 2 octet `AS_TRANS` constant, in network order (big-endian).
#define AS_TRANS  BE16(23456)
/// 4 octet `AS_TRANS` constant, in network order (big-endian).
#define AS4_TRANS BE32(23456)

/// 2 bytes wide AS number (ASN16), in network order (big-endian).
typedef Uint16 Asn16;
/// 4 bytes wide AS number (ASN32), in network order (big-endian).
typedef Uint32 Asn32;

/**
 * \brief Fat AS type capable of holding either a 4 octet (ASN32) or a 2 octet
 *        (ASN16) AS number, with additional flags indicating ASN properties.
 *
 * \note This type doesn't reflect any actual BGP ASN encoding, it is merely
 *       a convenience abstraction, useful when ASN properties should be bundled
 *       with the actual ASN.
 */
typedef Sint64 Asn;  // NOTE: signed so negative values may be used for
                     // special purposes if necessary

/// `Asn` flag indicating that the currently stored ASN is 4 octet wide (ASN32).
#define ASN32BITFLAG BIT(62)

/**
 * \brief Return an `Asn` holding an `Asn16` value.
 *
 * \note `asn` must be in network order (big endian).
 */
#define ASN16BIT(asn) ((Asn) ((Asn16) (asn)))

/**
 * \brief Return an `Asn` holding a `Asn32`.
 *
 * \note `asn` should be in network order (big endian).
 */
#define ASN32BIT(asn) ((Asn) (ASN32BITFLAG | ((Asn) ((Asn32) asn))))

/// Test whether an `Asn` is holding a 4 octet wide ASN.
FORCE_INLINE Boolean ISASN32BIT(Asn asn)
{
	return (asn & ASN32BITFLAG) != 0;
}

/**
 * \brief Extract AS number stored inside an `Asn`.
 *
 * Returned ASN is extended to `Asn32` even if it was originally an `Asn16`,
 * this allows code involving `Asn` to treat any ASN uniformly.
 *
 * \note Resulting ASN is in network byte order (big endian).
 */
FORCE_INLINE Asn32 ASN(Asn asn)
{
	Asn32 res = asn & 0xffffffffuLL;

#if EDN_NATIVE != EDN_BE
	// Shift _asn of 16 bits if this was originally ASN16,
	// on LE machines this mimics a BE16 word to BE32 dword extension
	res <<= (((asn & ASN32BITFLAG) == 0) << 4);
#endif
	return res;
}

/// Test whether `asn` represents either `AS_TRANS` or `AS4_TRANS`.
FORCE_INLINE Boolean ISASTRANS(Asn asn)
{
	return ASN(asn) == AS4_TRANS;
}

#endif
