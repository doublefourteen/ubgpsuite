// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/mrt.h
 *
 * Multithreaded Routing Toolkit (MRT) types and functions.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_MRT_H_
#define DF_BGP_MRT_H_

#include "bgp/bgp.h"
#include "stm.h"

/**
 * \name MRT record types
 *
 * \note Values are in network order (big endian)
 *
 * @{
 */

#define MRT_NULL         BE16(0)   // Deprecated
#define MRT_START        BE16(1)   // Deprecated
#define MRT_DIE          BE16(2)   // Deprecated
#define MRT_I_AM_DEAD    BE16(3)   // Deprecated
#define MRT_PEER_DOWN    BE16(4)   // Deprecated
#define MRT_BGP          BE16(5)   // Deprecated, also known as ZEBRA_BGP
#define MRT_RIP          BE16(6)   // Deprecated
#define MRT_IDRP         BE16(7)   // Deprecated
#define MRT_RIPNG        BE16(8)   // Deprecated
#define MRT_BGP4PLUS     BE16(9)   // Deprecated
#define MRT_BGP4PLUS_01  BE16(10)  // Deprecated
#define MRT_OSPFV2       BE16(11)
#define MRT_TABLE_DUMP   BE16(12)
#define MRT_TABLE_DUMPV2 BE16(13)
#define MRT_BGP4MP       BE16(16)
#define MRT_BGP4MP_ET    BE16(17)
#define MRT_ISIS         BE16(32)
#define MRT_ISIS_ET      BE16(33)
#define MRT_OSPFV3       BE16(48)
#define MRT_OSPFV3_ET    BE16(49)

/// 2-octets type for MRT record header type field, network byte order (big endian).
typedef Uint16 MrtType;

/** @} */


/**
 * \name MRT subtypes enumeration for records of type BGP/ZEBRA
 *
 * \warning ZEBRA BGP has been deprecated in favor of BGP4MP.
 *
 * \see `MrtSubType`
 *
 * @{
 */

#define ZEBRA_NULL         BE16(0)
#define ZEBRA_UPDATE       BE16(1)
#define ZEBRA_PREF_UPDATE  BE16(2)
#define ZEBRA_STATE_CHANGE BE16(3)
#define ZEBRA_SYNC         BE16(4)
#define ZEBRA_OPEN         BE16(5)
#define ZEBRA_NOTIFY       BE16(6)
#define ZEBRA_KEEPALIVE    BE16(7)

/** @} */

/**
 * \name MRT subtypes for records of type BGP4MP
 *
 * BGP4MP is defined in [RFC 6396](https://tools.ietf.org/html/rfc6396#section-4.2),
 * and extended by [RFC 8050](https://tools.ietf.org/html/rfc8050#page-2).
 *
 * \see [IANA BGP4MP Subtype Codes](https://www.iana.org/assignments/mrt/mrt.xhtml#BGP4MP-codes)
 * \see `MrtSubType`
 *
 * @{
 */

#define BGP4MP_STATE_CHANGE              BE16(0)   ///< RFC 6396
#define BGP4MP_MESSAGE                   BE16(1)   ///< RFC 6396
#define BGP4MP_ENTRY                     BE16(2)   ///< Deprecated
#define BGP4MP_SNAPSHOT                  BE16(3)   ///< Deprecated
#define BGP4MP_MESSAGE_AS4               BE16(4)   ///< RFC 6396
#define BGP4MP_STATE_CHANGE_AS4          BE16(5)   ///< RFC 6396
#define BGP4MP_MESSAGE_LOCAL             BE16(6)   ///< RFC 6396
#define BGP4MP_MESSAGE_AS4_LOCAL         BE16(7)   ///< RFC 6396
#define BGP4MP_MESSAGE_ADDPATH           BE16(8)   ///< RFC 8050
#define BGP4MP_MESSAGE_AS4_ADDPATH       BE16(9)   ///< RFC 8050
#define BGP4MP_MESSAGE_LOCAL_ADDPATH     BE16(10)  ///< RFC 8050
#define BGP4MP_MESSAGE_AS4_LOCAL_ADDPATH BE16(11)  ///< RFC 8050

/** @} */

/**
 * \name MRT subtypes for records of type TABLE_DUMPV2
 *
 * Table Dump version 2 is defined in [RFC 6396](https://tools.ietf.org/html/rfc6396#section-4.3).
 *
 * \see [IANA Table Dump version 2 Subtype Codes](https://www.iana.org/assignments/mrt/mrt.xhtml#table-dump-v2-subtype-codes)
 * \see `MrtSubType`
 *
 * @{
 */

#define TABLE_DUMPV2_PEER_INDEX_TABLE           BE16(1)   ///< RFC 6396
#define TABLE_DUMPV2_RIB_IPV4_UNICAST           BE16(2)   ///< RFC 6396
#define TABLE_DUMPV2_RIB_IPV4_MULTICAST         BE16(3)   ///< RFC 6396
#define TABLE_DUMPV2_RIB_IPV6_UNICAST           BE16(4)   ///< RFC 6396
#define TABLE_DUMPV2_RIB_IPV6_MULTICAST         BE16(5)   ///< RFC 6396
#define TABLE_DUMPV2_RIB_GENERIC                BE16(6)   ///< RFC 6396
#define TABLE_DUMPV2_GEO_PEER_TABLE             BE16(7)   ///< RFC 6397
#define TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH   BE16(8)   ///< RFC 8050
#define TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH BE16(9)   ///< RFC 8050
#define TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH   BE16(10)  ///< RFC 8050
#define TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH BE16(11)  ///< RFC 8050
#define TABLE_DUMPV2_RIB_GENERIC_ADDPATH        BE16(12)  ///< RFC 8050

/** @} */

/// Type for MRT header subtype field, 2-octets type, network byte order (big endian).
typedef Uint16 MrtSubType;

/// Lookup table for TABLE_DUMPV2 PEER_INDEX.
typedef struct {
	// NOTE: Data in this table is accessed atomically,
	//       despite these fields being signed they are effectively treated as
	//       unsigned by the library.

	Sint16 validCount;           ///< Count of valid entries inside `offsets`
	Sint16 peerCount;            ///< Cached PEER_INDEX_TABLE peer count value
	Sint32 offsets[FLEX_ARRAY];  ///< Offset table, `peerCount` entries
} Mrtpeertabv2;

/**
 * \brief MRT record type.
 *
 * Behaves in a similar way to `Bgpmsg`.
 * Performs allocations via possibly custom allocator defined in fields
 * `allocp` and `memOps`.
 * If those fields are left `NULL', it uses `Mem_StdOps`.
 */
typedef struct {
	Uint8 *buf;            ///< Raw MRT record contents

	void         *allocp;  ///< Optional custom memory allocator
	const MemOps *memOps;  ///< Optional custom memory operations

	/**
	 * Fast peer offset table, only meaningful for
	 * `TABLE_DUMPV2` `PEER_INDEX_TABLE`.
	 *
	 * \note This is an atomic pointer to a [Mrtpeertabv2](\ref Mrtpeertabv2).
	 *
	 * \warning Honest, this field **MUST NOT** be accessed directly.
	 */
	void *peerOffTab;
} Mrtrecord;

/// Move `Mrtrecord` contents from `src` to `dest`, leaving `src` empty.
FORCE_INLINE void MRT_MOVEREC(Mrtrecord *dest, Mrtrecord *src)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	memcpy(dest, src, sizeof(*dest));
	src->buf        = NULL;
	src->peerOffTab = NULL;
}

// Retrieve `MemOps` associated with record `rec`.
FORCE_INLINE const MemOps *MRT_MEMOPS(const Mrtrecord *rec)
{
	return rec->memOps ? rec->memOps : Mem_StdOps;
}

#define _MRT_EXHDRMASK ( \
	BIT(BE16(MRT_BGP4MP_ET)) | \
	BIT(BE16(MRT_ISIS_ET))   | \
	BIT(BE16(MRT_OSPFV3_ET))   \
)

#define _TABLE_DUMPV2_RIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_GENERIC))                | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH)) | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH)) | \
	BIT(BE16(TABLE_DUMPV2_RIB_GENERIC_ADDPATH))          \
)
#define _TABLE_DUMPV2_V4RIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH))   \
)
#define _TABLE_DUMPV2_V6RIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH))   \
)
#define _TABLE_DUMPV2_UNICASTRIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST))           | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH))     \
)
#define _TABLE_DUMPV2_MULTICASTRIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST))         | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH)) | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH))   \
)
#define _TABLE_DUMPV2_ADDPATHRIBMASK ( \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH)) | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH))   | \
	BIT(BE16(TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH)) | \
	BIT(BE16(TABLE_DUMPV2_RIB_GENERIC_ADDPATH))          \
)

#define _BGP4MP_AS4MASK ( \
	BIT(BE16(BGP4MP_MESSAGE_AS4))             | \
	BIT(BE16(BGP4MP_STATE_CHANGE_AS4))        | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_LOCAL))       | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_ADDPATH))     | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_LOCAL_ADDPATH)) \
)
#define _BGP4MP_ADDPATHMASK ( \
	BIT(BE16(BGP4MP_MESSAGE_ADDPATH))         | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_ADDPATH))     | \
	BIT(BE16(BGP4MP_MESSAGE_LOCAL_ADDPATH))   | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_LOCAL_ADDPATH)) \
)
#define _BGP4MP_MESSAGEMASK ( \
	BIT(BE16(BGP4MP_MESSAGE))                 | \
	BIT(BE16(BGP4MP_MESSAGE_AS4))             | \
	BIT(BE16(BGP4MP_MESSAGE_LOCAL))           | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_LOCAL))       | \
	BIT(BE16(BGP4MP_MESSAGE_ADDPATH))         | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_ADDPATH))     | \
	BIT(BE16(BGP4MP_MESSAGE_LOCAL_ADDPATH))   | \
	BIT(BE16(BGP4MP_MESSAGE_AS4_LOCAL_ADDPATH)) \
)

#define _ZEBRA_MESSAGEMASK ( \
	BIT(BE16(ZEBRA_UPDATE))  | \
	BIT(BE16(ZEBRA_OPEN))    | \
	BIT(BE16(ZEBRA_NOTIFY))  | \
	BIT(BE16(ZEBRA_KEEPALIVE)) \
)

/// Test whether a MRT record type provides extended precision timestamp header.
FORCE_INLINE Boolean MRT_ISEXHDRTYPE(MrtType type)
{
	return BE16(type) <= 49 && (_MRT_EXHDRMASK & BIT(BE16(type))) != 0;
}

/// Test whether a MRT record type is BGP4MP.
FORCE_INLINE Boolean MRT_ISBGP4MP(MrtType type)
{
	return type == MRT_BGP4MP || type == MRT_BGP4MP_ET;
}

/// Test whether a MRT record type is ISIS.
FORCE_INLINE Boolean MRT_ISISIS(MrtType type)
{
	return type == MRT_ISIS || type == MRT_ISIS_ET;
}

/// Test whether a MRT record type is OSPFV3.
FORCE_INLINE Boolean MRT_ISOSPFV3(MrtType type)
{
	return type == MRT_OSPFV3 || type == MRT_OSPFV3_ET;
}

/// Test whether a BGP4MP MRT record subtype contains 32-bits ASN.
FORCE_INLINE Boolean BGP4MP_ISASN32BIT(MrtSubType subtype)
{
	return BE16(subtype) <= 11 && (_BGP4MP_AS4MASK & BIT(BE16(subtype))) != 0;
}

/// Test whether a BGP4MP MRT subtype belongs has ADD_PATH information.
FORCE_INLINE Boolean BGP4MP_ISADDPATH(MrtSubType subtype)
{
	return BE16(subtype) <= 11 &&
	      (_BGP4MP_ADDPATHMASK & BIT(BE16(subtype))) != 0;
}

/**
 * \brief Test whether a BGP4MP MRT subtype wraps a BGP message.
 *
 * \see `Bgp_UnwrapBgp4mp()`
 */
FORCE_INLINE Boolean BGP4MP_ISMESSAGE(MrtSubType subtype)
{
	return BE16(subtype) <= 11 &&
	      (_BGP4MP_MESSAGEMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean BGP4MP_ISSTATECHANGE(MrtSubType subtype)
{
	return subtype == BGP4MP_STATE_CHANGE ||
	       subtype == BGP4MP_STATE_CHANGE_AS4;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISRIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_RIBMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISIPV4RIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_V4RIBMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISIPV6RIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_V6RIBMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISGENERICRIB(MrtSubType subtype)
{
	return subtype == TABLE_DUMPV2_RIB_GENERIC ||
	       subtype == TABLE_DUMPV2_RIB_GENERIC_ADDPATH;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISUNICASTRIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_UNICASTRIBMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISMULTICASTRIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_MULTICASTRIBMASK & BIT(BE16(subtype))) != 0;
}

FORCE_INLINE Boolean TABLE_DUMPV2_ISADDPATHRIB(MrtSubType subtype)
{
	return BE16(subtype) <= 12 &&
	      (_TABLE_DUMPV2_ADDPATHRIBMASK & BIT(BE16(subtype))) != 0;
}

/**
 * \brief Test whether a ZEBRA BGP message wraps BGP message.
 *
 * \see `Bgp_UnwrapZebra()`
 */
FORCE_INLINE Boolean ZEBRA_ISMESSAGE(MrtSubType subtype)
{
	return BE16(subtype) <= 7 &&
	      (_ZEBRA_MESSAGEMASK & BIT(BE16(subtype))) != 0;
}

#pragma pack(push, 1)

/**
 * \brief MRT header type.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint32     timestamp;  ///< Unix timestamp
	MrtType    type;       ///< MRT record type
	MrtSubType subtype;    ///< MRT record subtype
	Uint32     len;        ///< Record length in bytes **not including** header itself
} Mrthdr;

/**
 * \brief Extended MRT header, includes microseconds information.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 *
 * \see `Mrthdr`
 */
typedef ALIGNED(1, struct) {
	Uint32     timestamp;
	MrtType    type;
	MrtSubType subtype;
	Uint32     len;
	Uint32     microsecs;
} Mrthdrex;

typedef ALIGNED(1, struct) {
	Uint32 peerBgpId;             ///< Peer BGP identifier
	Uint16 viewNameLen;           ///< `viewName` field length in chars
	char   viewName[FLEX_ARRAY];  ///< Optional view name, **not** `\0` terminated
	// Uint16 peerCount;
	// Mrtpeerent peers[FLEX_ARRAY];
} Mrtpeeridx;

#define MRT_PEER_IP6      BIT(0)
#define MRT_PEER_ASN32BIT BIT(1)

typedef ALIGNED(1, struct) {
	Uint8  type;
	Uint32 bgpId;
	union {
		struct {
			Ipv4adr addr;
			Asn16   asn;
		} a16v4;
		struct {
			Ipv4adr addr;
			Asn32   asn;
		} a32v4;
		struct {
			Ipv6adr addr;
			Asn16   asn;
		} a16v6;
		struct {
			Ipv6adr addr;
			Asn32   asn;
		} a32v6;
	};
} Mrtpeerentv2;

FORCE_INLINE Boolean MRT_ISPEERIPV6(Uint8 type)
{
	return (type & MRT_PEER_IP6) != 0;
}

FORCE_INLINE Boolean MRT_ISPEERASN32BIT(Uint8 type)
{
	return (type & MRT_PEER_ASN32BIT) != 0;
}

FORCE_INLINE Asn MRT_GETPEERADDR(Ipadr *dest, const Mrtpeerentv2 *peer)
{
	switch (peer->type & (MRT_PEER_IP6|MRT_PEER_ASN32BIT)) {
	case MRT_PEER_IP6|MRT_PEER_ASN32BIT:
		dest->family = IP6;
		dest->v6     = peer->a32v6.addr;
		return ASN32BIT(peer->a32v6.asn);
	case MRT_PEER_IP6:
		dest->family = IP6;
		dest->v6     = peer->a16v6.addr;
		return ASN16BIT(peer->a16v6.asn);
	case MRT_PEER_ASN32BIT:
		dest->family = IP4;
		dest->v4     = peer->a32v4.addr;
		return ASN32BIT(peer->a32v4.asn);
	default:
		dest->family = IP4;
		dest->v4     = peer->a16v4.addr;
		return ASN16BIT(peer->a16v4.asn);
	}
}

#define _MRT_RIBENT_FIELDS(IpT) \
	IpT    prefix;          \
	Uint8  prefixLen;       \
	Uint8  status;          \
	Uint32 originatedTime;  \
	IpT    peerAddr;        \
	Uint16 peerAs;          \
	Uint16 attrLen

/**
 * \brief Legacy TABLE_DUMP RIB entry.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint16 viewno;
	Uint16 seqno;
	union {
		struct {
			_MRT_RIBENT_FIELDS(Ipv4adr);
			// Uint8 attrs[FLEX_ARRAY];
		} v4;
		struct {
			_MRT_RIBENT_FIELDS(Ipv6adr);
			// Uint8 attrs[FLEX_ARRAY];
		} v6;
	};
} Mrtribent;

FORCE_INLINE Boolean RIB_GETPFX(Afi              afi,
                                RawPrefix       *dest,
                                const Mrtribent *rib)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	switch (afi) {
	case AFI_IP:
		if (rib->v4.prefixLen > IPV4_WIDTH)
			return FALSE;

		dest->width = rib->v4.prefixLen;
		memcpy(dest->bytes, &rib->v4.prefix, PFXLEN(rib->v4.prefixLen));
		return TRUE;
	case AFI_IP6:
		if (rib->v6.prefixLen > IPV6_WIDTH)
			return FALSE;

		dest->width = rib->v6.prefixLen;
		memcpy(dest->bytes, &rib->v6.prefix, PFXLEN(rib->v6.prefixLen));
		return TRUE;
	default:
		UNREACHABLE;
		return FALSE;
	}
}

FORCE_INLINE Uint32 RIB_GETORIGINATED(Afi afi, const Mrtribent *rib)
{
	switch (afi) {
	case AFI_IP:  return rib->v4.originatedTime;
	case AFI_IP6: return rib->v6.originatedTime;
	default:
		UNREACHABLE;
		return 0;
	}
}

FORCE_INLINE Asn RIB_GETPEERADDR(Afi afi, Ipadr *dest, const Mrtribent *rib)
{
	switch (afi) {
	case AFI_IP:
		dest->family = afi;
		dest->v4     = rib->v4.peerAddr;
		return ASN16BIT(rib->v4.peerAs);
	case AFI_IP6:
		dest->family = afi;
		dest->v6     = rib->v6.peerAddr;
		return ASN16BIT(rib->v6.peerAs);
	default:
		UNREACHABLE;
		return -1;
	}
}

// NOTE: RIB AFI is stored inside `Mrthdr subtype` field.
FORCE_INLINE Bgpattrseg *RIB_GETATTRIBS(Afi afi, const Mrtribent *rib)
{
	switch (afi) {
	case AFI_IP:  return (Bgpattrseg *) &rib->v4.attrLen;
	case AFI_IP6: return (Bgpattrseg *) &rib->v6.attrLen;
	default:      UNREACHABLE; return NULL;
	}
}

typedef ALIGNED(1, struct) {
	Uint32 seqno;
	union {
		RawPrefix nlri;
		struct {
			Afi       afi;
			Safi      safi;
			RawPrefix nlri;
		} gen;
	};
} Mrtribhdrv2;

FORCE_INLINE Mrtribhdrv2 *RIBV2_HDR(const Mrthdr *hdr)
{
	return (Mrtribhdrv2 *) (hdr + 1);
}

typedef ALIGNED(1, struct) {
	Uint16 entryCount;
	Uint8  entries[FLEX_ARRAY];
} Mrtribentriesv2;

/**
 * \brief TABLE_DUMPV2 RIB entry.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint16 peerIndex;          ///< `Mrtpeerentv2` index inside PEER_INDEX_TABLE
	Uint32 originatedTime;
	Uint8  data[FLEX_ARRAY];
} Mrtribentv2;

FORCE_INLINE Bgpattrseg *RIBV2_GETATTRIBS(MrtSubType         subtype,
                                          const Mrtribentv2 *rib)
{
	return (Bgpattrseg *) (TABLE_DUMPV2_ISADDPATHRIB(subtype) ?
	                       rib->data + 4 :
	                       rib->data);
}

FORCE_INLINE Uint32 RIBV2_GETPATHID(const Mrtribentv2 *ent)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	Uint32 pathid;

	memcpy(&pathid, ent->data, sizeof(pathid));
	return pathid;
}

FORCE_INLINE void RIBV2_SETPATHID(Mrtribentv2 *ent, Uint32 pathid)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	memcpy(ent->data, &pathid, sizeof(pathid));
}

FORCE_INLINE void RIBV2_GETNLRI(MrtSubType         subtype,
                                Prefix            *dest,
                                const Mrtribhdrv2 *hdr,
                                const Mrtribentv2 *ent)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	if (TABLE_DUMPV2_ISGENERICRIB(subtype)) {
		dest->afi  = hdr->gen.afi;
		dest->safi = hdr->gen.safi;

		memcpy(PLAINPFX(dest), &hdr->gen.nlri, 1 + PFXLEN(hdr->gen.nlri.width));
	} else {
		dest->afi  = TABLE_DUMPV2_ISIPV6RIB(subtype) ?
		             AFI_IP6 :
		             AFI_IP;
		dest->safi = TABLE_DUMPV2_ISMULTICASTRIB(subtype) ?
		             SAFI_MULTICAST :
		             SAFI_UNICAST;

		memcpy(PLAINPFX(dest), &hdr->nlri, 1 + PFXLEN(hdr->nlri.width));
	}

	dest->isAddPath = TABLE_DUMPV2_ISADDPATHRIB(subtype);
	dest->pathId    = (dest->isAddPath) ? RIBV2_GETPATHID(ent) : 0;
}

#define _MRT_BGP4MP_COMFIELDS(AsnT) \
	AsnT   peerAs, localAs;  \
	Uint16 iface;            \
	Afi    afi

#define _MRT_BGP4MP_HDRFIELDS(AsnT, IpT) \
	_MRT_BGP4MP_COMFIELDS(AsnT);  \
	IpT peerAddr, localAddr

/// Common header fields to all BGP4MP MRT records.
typedef ALIGNED(1, union) {
	struct {
		_MRT_BGP4MP_COMFIELDS(Asn32);
	} a32;
	struct {
		_MRT_BGP4MP_COMFIELDS(Asn16);
	} a16;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn16, Ipv4adr);
	} a16v4;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn16, Ipv6adr);
	} a16v6;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn32, Ipv4adr);
	} a32v4;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn32, Ipv6adr);
	} a32v6;
} Bgp4mphdr;

FORCE_INLINE Bgp4mphdr *BGP4MP_HDR(const Mrthdr *hdr)
{
	return (hdr->type == MRT_BGP4MP_ET) ?
	       (Bgp4mphdr *) ((const Mrthdrex *) hdr + 1) :
	       (Bgp4mphdr *) (hdr + 1);
}

FORCE_INLINE Asn BGP4MP_GETPEERADDR(MrtSubType       subtype,
                                    Ipadr           *dest,
                                    const Bgp4mphdr *bgp4mp)
{
	if (BGP4MP_ISASN32BIT(subtype)) {
		switch (bgp4mp->a32.afi) {
		case AFI_IP:
			dest->family = IP4;
			dest->v4     = bgp4mp->a32v4.peerAddr;
			break;
		case AFI_IP6:
			dest->family = IP6;
			dest->v6     = bgp4mp->a32v6.peerAddr;
			break;
		default:
			UNREACHABLE;
			break;
		}
		return ASN32BIT(bgp4mp->a32.peerAs);
	} else {
		switch (bgp4mp->a16.afi) {
		case AFI_IP:
			dest->family = IP4;
			dest->v4     = bgp4mp->a16v4.peerAddr;
			break;
		case AFI_IP6:
			dest->family = IP6;
			dest->v6     = bgp4mp->a16v6.peerAddr;
			break;
		default:
			UNREACHABLE;
			break;
		}
		return ASN16BIT(bgp4mp->a16.peerAs);
	}
}

FORCE_INLINE Asn BGP4MP_GETLOCALADDR(MrtSubType       subtype,
                                     Ipadr           *dest,
                                     const Bgp4mphdr *bgp4mp)
{
	if (BGP4MP_ISASN32BIT(subtype)) {
		switch (bgp4mp->a32.afi) {
		case AFI_IP:
			dest->family = IP4;
			dest->v4     = bgp4mp->a32v4.localAddr;
			break;
		case AFI_IP6:
			dest->family = IP6;
			dest->v6     = bgp4mp->a32v6.localAddr;
			break;
		default:
			UNREACHABLE;
			return -1;
		}
		return ASN32BIT(bgp4mp->a32.localAs);
	} else {
		switch (bgp4mp->a16.afi) {
		case AFI_IP:
			dest->family = IP4;
			dest->v4     = bgp4mp->a16v4.localAddr;
			break;
		case AFI_IP6:
			dest->family = IP6;
			dest->v6     = bgp4mp->a16v6.localAddr;
			break;
		default:
			UNREACHABLE;
			return -1;
		}
		return ASN16BIT(bgp4mp->a16.localAs);
	}
}

/// BGP4MP message providing BGP Finite State Machine (FSM) state change information.
typedef ALIGNED(1, union) {
	Bgp4mphdr hdr;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn16, Ipv4adr);
		BgpFsmState oldState, newState;
	} a16v4;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn16, Ipv6adr);
		BgpFsmState oldState, newState;
	} a16v6;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn32, Ipv4adr);
		BgpFsmState oldState, newState;
	} a32v4;
	struct {
		_MRT_BGP4MP_HDRFIELDS(Asn32, Ipv6adr);
		BgpFsmState oldState, newState;
	} a32v6;
} Bgp4mpstatchng;

typedef ALIGNED(1, struct) {
	Uint16  peerAs;
	Ipv4adr peerAddr;
} Zebrahdr;

FORCE_INLINE Zebrahdr *ZEBRA_HDR(const Mrthdr *hdr)
{
	return (Zebrahdr *) (hdr + 1);
}

typedef ALIGNED(1, struct) {
	Zebrahdr hdr;
	Uint16   localAs;
	Ipv4adr  localAddr;
	Uint8    msg[FLEX_ARRAY];
} Zebramsghdr;

typedef ALIGNED(1, struct) {
	Zebrahdr    hdr;
	BgpFsmState oldState, newState;
} Zebrastatchng;

#pragma pack(pop)

/// MRT record header size in bytes.
#define MRT_HDRSIZ   (4uLL + 2uLL + 2uLL + 4uLL)
/// Extended timestamp MRT record header size in bytes.
#define MRT_EXHDRSIZ (MRT_HDRSIZ + 4uLL)

STATIC_ASSERT(MRT_HDRSIZ == sizeof(Mrthdr), "MRT_HDRSIZ vs Mrthdr size mismatch");
STATIC_ASSERT(MRT_EXHDRSIZ == sizeof(Mrthdrex), "MRT_EXHDRSIZ vs Mrthdrex size mismatch");

/// Retrieve a pointer to a MRT record header.
FORCE_INLINE Mrthdr *MRT_HDR(const Mrtrecord *rec)
{
	return (Mrthdr *) rec->buf;
}

typedef struct {
	Uint8 *base, *lim;
	Uint8 *ptr;

	Uint16 peerCount;
	Uint16 nextIdx;
} Mrtpeeriterv2;

typedef struct {
	Uint8 *base, *lim;
	Uint8 *ptr;

	Boolean8 isAddPath;
	Uint16   entryCount;
	Uint16   nextIdx;
} Mrtribiterv2;

Judgement Bgp_MrtFromBuf(Mrtrecord *rec, const void *data, size_t nbytes);
Judgement Bgp_ReadMrt(Mrtrecord *rec, void *streamp, const StmOps *ops);
void Bgp_ClearMrt(Mrtrecord *rec);

// =========================================
// TABLE_DUMPV2 - PEER_INDEX_TABLE

Mrtpeeridx *Bgp_GetMrtPeerIndex(Mrtrecord *rec);
void *Bgp_GetMrtPeerIndexPeers(Mrtrecord *rec, size_t *peersCount, size_t *nbytes);
Judgement Bgp_StartMrtPeersv2(Mrtpeeriterv2 *it, Mrtrecord *rec);
Mrtpeerentv2 *Bgp_NextMrtPeerv2(Mrtpeeriterv2 *it);
Mrtpeerentv2 *Bgp_GetMrtPeerByIndex(Mrtrecord *rec, Uint16 idx);

// =========================================
// TABLE_DUMPV2 - RIB SubTypes

Mrtribhdrv2 *Bgp_GetMrtRibHdrv2(Mrtrecord *rec, size_t *nbytes);
Mrtribentriesv2 *Bgp_GetMrtRibEntriesv2(Mrtrecord *rec, size_t *nbytes);
Judgement Bgp_StartMrtRibEntriesv2(Mrtribiterv2 *it, Mrtrecord *rec);
Mrtribentv2 *Bgp_NextRibEntryv2(Mrtribiterv2 *rec);

// =========================================
// BGP4MP

Bgp4mphdr *Bgp_GetBgp4mpHdr(Mrtrecord *rec, size_t *nbytes);
Judgement Bgp_UnwrapBgp4mp(Mrtrecord *rec, Bgpmsg *msg, unsigned flags);

// =========================================
// DEPRECATED - TABLE_DUMP

Mrtribent *Bgp_GetMrtRibHdr(Mrtrecord *rec);
void *Bgp_GetMrtRibEntry(Mrtrecord *rec, size_t *nbytes);

// =========================================
// DEPRECATED - ZEBRA

Zebrahdr *Bgp_GetZebraHdr(Mrtrecord *rec, size_t *nbytes);
Judgement Bgp_UnwrapZebra(Mrtrecord *rec, Bgpmsg *msg, unsigned flags);

// ==========================================
// RIB attribute segment encoding formats

Judgement Bgp_StartAllRibv2NextHops(Nexthopiter      *it,
                                    const Mrthdr     *hdr,
                                    const Bgpattrseg *tpa,
                                    Bgpattrtab        tab);

Judgement Bgp_RebuildMsgFromRib(const Prefix     *nlri,
                                const Bgpattrseg *tpa,
                                Bgpmsg           *msg,
                                unsigned          flags);

#endif
