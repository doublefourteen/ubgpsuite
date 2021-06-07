// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/prefix.h
 *
 * Network prefixes types and utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_PREFIX_H_
#define DF_BGP_PREFIX_H_

#include "sys/ip.h"

/**
 * \brief `Afi` values.
 *
 * \see [Address family numbers](https://www.iana.org/assignments/address-family-numbers/address-family-numbers.xhtml)
 *
 * \note Address family numbers are in network order (big endian).
 *
 * @{
 */

#define AFI_IP  BE16(1)
#define AFI_IP6 BE16(2)

/**
 * \brief Address Family Identifier, as defined by the BGP protocol.
 *
 * \note Address family numbers are in network order (big endian).
 */
typedef Uint16 Afi;

/** @} */

/**
 * \brief `Safi` values.
 *
 * \see [SAFI namespace](https://www.iana.org/assignments/safi-namespace/safi-namespace.xhtml)
 * \see [RFC 4760](http://www.iana.org/go/rfc4760)
 *
 * @{
 */

#define SAFI_UNICAST   U8_C(1)
#define SAFI_MULTICAST U8_C(2)

/// Subsequent Address Family Identifier, as defined by the BGP protocol.
typedef Uint8 Safi;

/** @} */

#pragma pack(push, 1)

/**
 * \brief BGP prefix with PATH ID information
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint32 pathId;  ///< Path identifier
	Uint8  width;   ///< Prefix length in bits

	/**
	 * \brief Prefix content `union`.
	 *
	 * \warning Only `PFXLEN(width)` bytes are relevant.
	 */
	union {
		Uint8   bytes[16];  ///< Prefix raw bytes
		Uint16  words[8];   ///< Prefix contents as a sequence of 16-bits values
		Uint32  dwords[4];  ///< Prefix contents as a sequence of 32-bits values
		Ipv4adr v4;         ///< Prefix as a **full** IPv4 address
		Ipv6adr v6;         ///< Prefix as a **full** IPv6 address
	};
} ApRawPrefix;

/**
 * \brief BGP prefix with no PATH ID information.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint8 width;  ///< Prefix length in bits

	/**
	 * \brief Prefix content `union`.
	 *
	 * \warning Only `PFXLEN(width)` bytes are relevant.
	 */
	union {
		Uint8   bytes[16];  ///< Prefix raw bytes
		Uint16  words[8];   ///< Prefix contents as a sequence of 16-bits values
		Uint32  dwords[4];  ///< Prefix contents as a sequence of 32-bits values
		Ipv4adr v4;         ///< Prefix as a **full** IPv4 address
		Ipv6adr v6;         ///< Prefix as a **full** IPv6 address
	};
} RawPrefix;

/**
 * \brief "Fat" prefix structure, contains both the actual data and metadata about the BGP prefix itself.
 *
 * The structure doesn't reflect the actual BGP protocol format,
 * it is used whenever a raw BGP data pointer isn't sufficient
 * to convey enough context for a prefix (e.g. iterating every prefix available
 * inside a BGP message).
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 *
 * \note Given the significant amount of metadata, it
 *       should be used sparingly, raw prefixes should be preferred
 *       whenever possible to save the overhead.
 */
 typedef ALIGNED(1, struct) {
	Boolean8 isAddPath;  ///< Whether the path identifier is meaningful or not
	Afi      afi;        ///< Prefix address family
	Safi     safi;       ///< Prefix subsequent AFI
	Uint32   pathId;     ///< Path identifier (only meaningful if `isAddPath` is `TRUE`)
	Uint8    width;      ///< Prefix width, in bits (maximum legal value depends on AFI)

	/**
	 * \brief Prefix content `union`.
	 *
	 * \warning Only `PFXLEN(width)` bytes are relevant.
	 */
	union {
		Uint8   bytes[16];  ///< Prefix raw bytes
		Uint16  words[8];   ///< Prefix contents as a sequence of 16-bits values
		Uint32  dwords[4];  ///< Prefix contents as a sequence of 32-bits values
		Ipv4adr v4;         ///< Prefix as a **full** IPv4 address
		Ipv6adr v6;         ///< Prefix as a **full** IPv6 address
	};
} Prefix;

#pragma pack(pop)

/// Calculate prefix length in bytes from bit width.
#define PFXLEN(width) ((size_t) (((width) >> 3) + (((width) & 7) != 0)))
/**
 * \brief Return pointer to `RawPrefix` portion out of `ApRawPrefix`, `Prefix` or `RawPrefix` itself.
 *
 * Cast operation is useful to discard PATH ID information where irrelevant.
 */
#define PLAINPFX(prefix) ((RawPrefix *) (&(prefix)->width))
/// Return pointer to `ApRawPrefix` out of `Prefix` or `ApRawPrefix` itself.
#define APPFX(prefix) ((ApRawPrefix *) (&(prefix)->pathId))

/// Maximum length of an IPv6 prefix encoded as a string.
#define PFX6_STRLEN   (IPV6_STRLEN + 1 + 3)
/// Maximum length of an IPv4 prefix encoded as a string.
#define PFX4_STRLEN   (IPV4_STRLEN + 1 + 2)
/// Maximum length of an IPv6 prefix with PATH ID, encoded as a string.
#define APPFX6_STRLEN (10 + 1 + PFX6_STRLEN)
/// Maximum length of an IPv4 prefix with PATH ID, encoded as a string.
#define APPFX4_STRLEN (10 + 1 + PFX4_STRLEN)
/// Maximum length of a prefix encoded as a string.
#define PFX_STRLEN    PFX6_STRLEN
/// Maximum length of a prefix with PATH ID, encoded as a string.
#define APPFX_STRLEN  APPFX6_STRLEN

/**
 * Convert a `RawPrefix` of the specified `Afi` to its string representation.
 *
 * \return Pointer to the trailing `\0` inside `dest`.
 *
 * \note Assumes `dest` is large enough to hold result (use `[PFX_STRLEN + 1]`)
 */
char *Bgp_PrefixToString(Afi afi, const RawPrefix *pfx, char *dest);
/**
 * Convert an `ApRawPrefix` of the specified `Afi` to its string representation.
 *
 * \return Pointer to the trailing `\0` inside `dest`.
 *
 * \note Assumes `dest` is large enough to hold result (use `[APPFX_STRLEN + 1]`)
 */
char *Bgp_ApPrefixToString(Afi afi, const ApRawPrefix *pfx, char *dest);

/**
 * \brief Convert string with format `address/width` to `Prefix`.
 *
 * \return `OK` on success, `NG` on invalid prefix string.
 *
 * \note Does not handle PATH ID, may only return plain prefixes.
 */
Judgement Bgp_StringToPrefix(const char *s, Prefix *dest);

/**
 * \brief Direct iterator over raw prefix data.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Afi      afi;
	Safi     safi;
	Boolean8 isAddPath;

	Uint8 *base, *lim;
	Uint8 *ptr;
} Prefixiter;

/**
 * \brief Start iterating `nbytes` bytes from `data` for prefixes of the specified `afi` and `safi`.
 *
 * \return `OK` on success, `NG` on error. Sets BGP error, see `Bgp_GetErrStat()`.
 */
Judgement Bgp_StartPrefixes(Prefixiter *it, Afi afi, Safi safi, const void *data, size_t nbytes, Boolean isAddPath);
/**
 * \brief Get current prefix and advance iterator.
 *
 * \return Current prefix on success, depending on `isAddPath` prefix type
 *         may be either `RawPrefix` or `ApRawPrefix`. `NULL` on iteration end or
 *         error. Sets BGP error, see `Bgp_GetErrStat()`.
 */
void *Bgp_NextPrefix(Prefixiter *it);

#endif
