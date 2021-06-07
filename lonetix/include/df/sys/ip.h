// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/ip.h
 *
 * Internet Protocol (IP) definitions and types.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Right Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_IP_H_
#define DF_SYS_IP_H_

#include "xpt.h"

/// Known Internet Protocol address family types enumeration.
typedef enum {
	IP4,  ///< Internet Protocol version 4
	IP6   ///< Internet Protocol version 6
} IpType;

/// Internet Protocol version 4 (IPv4) address, big endian order.
typedef union {
	Uint8  bytes[4];  ///< Address bytes representation
	Uint16 words[2];  ///< Address as a pair of 16-bits words
	Uint32 dword;     ///< Address as a single 32-bits dword (big endian)
} Ipv4adr;

/// Size of an IPv4 address in bytes.
#define IPV4_SIZE 4
/// Bits inside an IPv4 address.
#define IPV4_WIDTH (IPV4_SIZE * 8)

STATIC_ASSERT(sizeof(Ipv4adr) == IPV4_SIZE, "Bad Ipv4adr size");

/// Size of a string to make an IPv4 address, **excluding** trailing `\0`.
#define IPV4_STRLEN 16

/// IPv4 wildcard address static initializer: `0.0.0.0`.
#define IPV4_ANY_INIT       { { 0x00, 0x00, 0x00, 0x00 } }
/// IPv4 loopback address static initializer: `127.0.0.1`.
#define IPV4_LOOPBACK_INIT  { { 0x7f, 0x00, 0x00, 0x01 } }
/// IPv4 broadcast address static initializer: `255.255.255.255`.
#define IPV4_BROADCAST_INIT { { 0xff, 0xff, 0xff, 0xff } }

/**
 * \brief Convert `Ipv4adr` to its string representation.
 *
 * The destination buffer **is assumed to be large enough to store
 * the resulting string**, a buffer of `IPV4_STRLEN + 1`
 * chars is safe to use as the `dest` argument.
 *
 * \param [in]  adr  Address to be converted, must not be `NULL`
 * \param [out] dest Destination storage for string representation, must not be `NULL`
 *
 * \return Pointer to the trailing `\0` `char` inside `dest`, useful
 *         for further string concatenation.
 */
char *Ipv4_AdrToString(const Ipv4adr *adr, char *dest);

/**
 * \brief Convert an address from its string representation to `Ipv4adr`.
 *
 * \return `OK` if conversion was successful, `NG` if address string
 *         does not represent a valid IP.
 */
Judgement Ipv4_StringToAdr(const char *address, Ipv4adr *dest);

/// Compare IPv4 addresses for equality.
FORCE_INLINE Boolean Ipv4_Equal(const Ipv4adr *a, const Ipv4adr *b)
{
	return a->dword == b->dword;
}

/// Internet Protocol version 6 address.
typedef union {
	Uint8  bytes[16];  ///< Address as raw bytes
	Uint16 words[8];   ///< Address as short words sequence
	Uint32 dwords[4];  ///< Address as dwords sequence
} Ipv6adr;

/// Size of an IPv6 address in bytes.
#define IPV6_SIZE   16
/// Bits inside an IPv6 address.
#define IPV6_WIDTH (IPV6_SIZE * 8)

STATIC_ASSERT(sizeof(Ipv6adr) == IPV6_SIZE, "Bad Ipv6adr size");

/// Size of a string to make an IPv6 address, **excluding** trailing `\0`.
#define IPV6_STRLEN 46

/// Static initializer for an UNSPECIFIED Ipv6 address.
#define IPV6_UNSPEC_INIT { { \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
} }

/// Static initializer for a LOOPBACK Ipv6 address.
#define IPV6_LOOPBACK_INIT { { \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01  \
} }

/// Test whether an IPv6 address represents a mapped IPv4 address.
#define IS_IPV6_V4MAPPED(v6) \
	((v6)->dwords[0] == 0 && \
	 (v6)->dwords[1] == 0 && \
	 (v6)->dwords[2] == BE32(0xffff))

/// Test whether an IPv6 address is multicast.
#define IS_IPV6_MULTICAST(v6) ((v6)->bytes[0] == 0xff)

/**
 * \brief Convert an IPv6 address to its string representation, destination
 *        storage should be `[IPV6_STRLEN + 1]`.
 *
 * \return Pointer to the trailing `\0` inside `dest`.
 */
char *Ipv6_AdrToString(const Ipv6adr *adr, char *dest);

/**
 * \brief Convert IPv6 address string to `Ipv6adr`.
 *
 * \return `OK` on success, `NG` if `address` does not represent a valid IPv6 address. 
 */
Judgement Ipv6_StringToAdr(const char *address, Ipv6adr *dest);

/// Compare two IPv6 addresses for equality.
FORCE_INLINE Boolean Ipv6_Equal(const Ipv6adr *a, const Ipv6adr *b)
{
	return a->dwords[0] == b->dwords[0] && a->dwords[1] == b->dwords[1] &&
	       a->dwords[2] == b->dwords[2] && a->dwords[3] == b->dwords[3];
}

/// Generic IP address.
typedef struct {
	IpType family;  ///< Currently stored address family
	union {
		Uint8   bytes[IPV6_SIZE];  ///< Address as raw bytes, for convenient initialization
		Ipv4adr v4;                ///< IPv4 address, if `family == IP4`
		Ipv6adr v6;                ///< IPv6 address, if `family == IP6`

		Uint32  dword;             ///< As IPv4 single dword, for macro compat
		Uint16  words[8];          ///< As IPv4/IPv6 word sequence, for macro compat
		Uint32  dwords[4];         ///< As IPv6 dwords sequence, for macro compat
	};
} Ipadr;

/**
 * \brief Convert a generic IP address to its string representation.
 *
 * \note A destination string of length `IPV6_STRLEN + 1`
 *       is capable of storing an address string for any address family.
 */
char *Ip_AdrToString(const Ipadr *adr, char *dest);

/**
 * \brief Convert IP string representation to `Ipadr`.
 *
 * \return `OK` on success, `NG` if `address` is not a valid IP address.
 */
Judgement Ip_StringToAdr(const char *address, Ipadr *dest);

/// Compare generic addresses for equality.
FORCE_INLINE Boolean Ip_Equal(const Ipadr *a, const Ipadr *b)
{
	if (a->family != b->family)
		return FALSE;

	switch (a->family) {
	case IP4: return Ipv4_Equal(&a->v4, &b->v4);
	case IP6: return Ipv6_Equal(&a->v6, &b->v6);
	default:  UNREACHABLE; return FALSE;
	}
}

#endif
