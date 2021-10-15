// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/bgp.h
 *
 * Border Gateway Protocol version 4 (BGP-4) types and functions.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * \see [RFC 4271](https://tools.ietf.org/html/rfc4271)
 */

#ifndef DF_BGP_MSG_H_
#define DF_BGP_MSG_H_

#include "bgp/asn.h"
#include "bgp/prefix.h"
#include "mem.h"
#include "srcloc.h"
#include "stm.h"

/// Result of an operation over a BGP message or data type.
typedef enum {
	// NOTE: Enum member ordering is IMPORTANT

	// BGP message filtering VM error codes (negative values, fit into Sint8)
	BGPEBADVM         = -16,  ///< Attempting operation on VM with a failed setup state flag
	BGPEVMNOPROG      = -15,  ///< VM program is empty
	BGPEVMBADCOMTCH   = -14,  ///< COMMUNITY Match expression is invalid or too complex
	BGPEVMASMTCHESIZE = -13,  ///< AS Match expression is so complex it cannot be evaluated within a reasonable memory limit
	BGPEVMASNGRPLIM   = -12,  ///< `BGP_VMOP_ASMTCH` has too many nested grouping levels
	BGPEVMBADASMTCH   = -11,  ///< `BGP_VMOP_ASMTCH` has inconsistent matching rules
	BGPEVMBADJMP      = -10,  ///< Jump instruction beyond program's `END`.
	BGPEVMILL         = -9,   ///< Illegal instruction
	BGPEVMOOM         = -8,   ///< VM heap exhausted (out of memory)
	BGPEVMBADENDBLK   = -7,   ///< `ENDBLK` instruction with no corresponding `BLK`
	BGPEVMUFLOW       = -6,   ///< BGP VM Stack underflow
	BGPEVMOFLOW       = -5,   ///< BGP VM Stack overflow
	BGPEVMBADFN       = -4,   ///< `CALL` instruction references bad function index
	BGPEVMBADK        = -3,   ///< `LOADK` instruction references bad constant index
	BGPEVMMSGERR      = -2,   ///< Accessing BGP message caused an error `Bgp_GetErrStat()` for details)
	BGPEVMBADOP       = -1,   ///< Bad VM instruction operand (e.g. AFI_IP PATRICIA was expected, but AFI_IP6 is provided)

	BGPENOERR = 0,        ///< No error (success)
	BGPENOMEM,            ///< Memory allocation failure

	// NOTE: following enumeration need not to fit inside `BgpvmRet`

	BGPEIO,               ///< Read error from input stream (see `Bgp_ReadMsg()`)
	BGPEBADTYPE,          ///< Provided wrong message type for the required operation
	BGPENOADDPATH,        ///< Attempt to access ADD_PATH information but message is not ADD_PATH enabled
	BGPEBADATTRTYPE,      ///< Provided wrong attribute type for the required operation
	BGPEBADMARKER,        ///< Invalid or corrupted initial BGP marker detected
	BGPETRUNCMSG,         ///< Truncated BGP message (incomplete header or data)
	BGPEOVRSIZ,           ///< Oversized BGP message (message length exceeding `BGP_MSGSIZ`)
	BGPEBADOPENLEN,       ///< BGP OPEN message has inconsistent length
	BGPEBADAGGR,          ///< Malformed AGGREGATOR attribute
	BGPEBADAGGR4,         ///< Malformed AS4_AGGREGATOR attribute
	BGPEDUPNLRIATTR,      ///< Duplicate multiprotocol/NLRI attribute inside UPDATE message
	BGPEBADPFXWIDTH,      ///< Prefix has illegal width (e.g. IPv4 prefix with `width > 32`)
	BGPETRUNCPFX,         ///< Message contains a truncated prefix
	BGPETRUNCATTR,        ///< Truncated BGP attribute
	BGPEAFIUNSUP,         ///< Unknown address family identifier inside message
	BGPESAFIUNSUP,        ///< Unknown subsequent address family identifier inside message
	BGPEBADMRTTYPE,       ///< Provided wrong MRT record type for the required operation
	BGPETRUNCMRT,         ///< Truncated MRT record (incomplete header or data)
	BGPEBADPEERIDXCNT,    ///< TABLE_DUMPV2 PEER_INDEX_TABLE record contains incoherent peer entry count
	BGPETRUNCPEERV2,      ///< TABLE_DUMPV2 PEER_INDEX_TABLE record contains truncated peer entry
	BGPEBADRIBV2CNT,      ///< TABLE_DUMPV2 RIB record contains incoherent RIB entry count
	BGPETRUNCRIBV2,       ///< TABLE_DUMPV2 RIB record contains a truncated RIB entry
	BGPEBADRIBV2MPREACH,  ///< TABLE_DUMPV2 RIB record contains an illegal MP_REACH attribute
	BGPERIBNOMPREACH,     ///< TABLE_DUMP or TABLE_DUMPV2 IPv6 RIB record lacks MP_REACH_NLRI attribute
	BGPEBADPEERIDX,       ///< Attempt to access an out of bounds peer entry
} BgpRet;

/// Test whether a `BgpRet` belongs to the `Bgpvm` error category.
FORCE_INLINE Boolean BGP_ISVMERR(BgpRet err)
{
	return err < BGPENOERR && err >= BGPEBADVM;
}

/// Test whether a `BgpRet` belongs to the BGP error category.
FORCE_INLINE Boolean BGP_ISMSGERR(BgpRet err)
{
	return err >= BGPEBADTYPE && err <= BGPESAFIUNSUP;
}

/// Test whether a `BgpRet` belongs to the MRT error category.
FORCE_INLINE Boolean BGP_ISMRTERR(BgpRet err)
{
	return err >= BGPEBADMRTTYPE && err <= BGPEBADPEERIDX;
}

/// Translate a `BgpRet` to a descriptive human readable string.
const char *Bgp_ErrorString(BgpRet err);

/// BGP library current result status.
typedef struct {
	/// Latest operation result.
	BgpRet code;
	/**
	 * \brief Current error handler function.
	 */
	void (*func)(BgpRet, Srcloc *, void *);
	/// Additional user data, forwarded to error handler unaltered.
	void  *obj;
} BgpErrStat;

/// Ignoring error handler, no error management is done with this handler.
#define BGP_ERR_IGN  ((void (*)(BgpRet, Srcloc *, void *))  0)
/**
 * \brief Abort error handler, terminates execution abnormally on error,
 *        providing an useful error trace if possible.
 */
#define BGP_ERR_QUIT ((void (*)(BgpRet, Srcloc *, void *)) -1)

/**
 * Install BGP error handler.
 */
void Bgp_SetErrFunc(void (*func)(BgpRet, Srcloc *, void *), void *obj);

/// Retrieve the latest operation outcome and extended error status information.
BgpRet Bgp_GetErrStat(BgpErrStat *stat);

/**
 * \name BGP message types
 *
 * @{
 */
#define BGP_OPEN          U8_C(1)    ///< `OPEN` message
#define BGP_UPDATE        U8_C(2)    ///< `UPDATE` message
#define BGP_NOTIFICATION  U8_C(3)    ///< `NOTIFICATION` message
#define BGP_KEEPALIVE     U8_C(4)    ///< `KEEPALIVE` message
#define BGP_ROUTE_REFRESH U8_C(5)    ///< `ROUTE_REFRESH` message
#define BGP_CLOSE         U8_C(255)  ///< `CLOSE` message

typedef Uint8 BgpType;  ///< Message type octet inside BGP header.

/** @} */

/**
 * \name BGP Finite State Machine values
 *
 * \see [RFC 4271 Section 8.2.2](\ref https://tools.ietf.org/html/rfc4271#section-8.2.2)
 *
 * \note Values are in network byte order (big endian).
 *
 * @{
 */

#define BGP_FSM_IDLE        BE16(1)  ///< `IDLE` state
#define BGP_FSM_CONNECT     BE16(2)  ///< `CONNECT` state
#define BGP_FSM_ACTIVE      BE16(3)  ///< `ACTIVE` state
#define BGP_FSM_OPENSENT    BE16(4)  ///< `OPENSENT` state
#define BGP_FSM_OPENCONFIRM BE16(5)  ///< `OPENCONFIRM` state
#define BGP_FSM_ESTABLISHED BE16(6)  ///< `ESTABLISHED` state

typedef Uint16 BgpFsmState;  ///< BGP Finite State Machine status code value.

/** @} */

/// Size of the BGP marker field inside the BGP message header in bytes.
#define BGP_MARKER_LEN 16u
/// Size of the BGP message header, in bytes.
#define BGP_HDRSIZ     19u

/// Maximum legal size for a regular BGP message, in bytes.
#define BGP_MSGSIZ   0x1000u
/// Maximum legal size for an extended BGP message, in bytes.
#define BGP_EXMSGSIZ 0xffffu

/**
 * \name BGP message flags
 *
 * \see `Bgpmsg` `flags` field
 *
 * @{
 */

/// Flag indicating the `Bgpmsg` structure is using an unowned buffer.
#define BGPF_UNOWNED BIT(0)
/// Flag indicating a BGP message uses 32 bits ASN.
#define BGPF_ASN32BIT BIT(1)
/// Flag indicating a BGP message containing ADD_PATH information.
#define BGPF_ADDPATH BIT(2)
/// Flag to allow BGP messages exceeding 4096 bytes size limit up to 65535 bytes.
#define BGPF_EXMSG BIT(3)
/**
 * \brief Flag used by `Bgp_RebuildMsgFromRib()`, specifies attribute parameter
 *        is in TABLE_DUMPV2 format.
 *
 * \note If this flag is not set, any ADD_PATH information inside
 *       NLRI is discarded, as TABLE_DUMP doesn't support it.
 *
 * \see `Bgp_RebuildMsgFromRib()`
 */
#define BGPF_RIBV2 BIT(4)
/**
 * \brief Require strict [RFC 6396](https://tools.ietf.org/html/rfc6396)
 *        compliance when building a BGP message from a MRT snapshot.
 *
 * Some MRT RIBs don't fully adhere to RFC format specification, by leaving
 * AFI and SAFI information inside MP_REACH as opposed to omitting it as
 * required by the standard. Such behavior is detected and tolerated by
 * default; by specifying this flag an explicit request is made to apply
 * the RFC verbatim.
 *
 * \see `Bgp_RebuildMsgFromRib()`
 */
#define BGPF_STRICT_RFC6396 BIT(5)
/**
 * \brief When rebuilding a `Bgpmsg` from either TABLE_DUMPV2 or TABLE_DUMP,
 *        strip MP_UNREACH_NLRI if encountered.
 *
 * \see `Bgp_RebuildMsgFromRib()`
 */
#define BGPF_STRIPUNREACH BIT(6)
/**
 * \brief When rebuilding a `Bgpmsg` from either TABLE_DUMPV2 or TABLE_DUMP,
 *        erase MP_UNREACH_NLRI attribute contents when encountered,
 *        but leave it in attribute list.
 *
 * \note Compared to `BGPF_STRIPUNREACH`, this preserves the information that
 *       an MP_UNREACH_NLRI attribute was effectively encountered, while
 *       discarding its contents to avoid processing them. It also preserves
 *       the original attribute offset inside the `Bgpattrtab`, allowing
 *       efficient lookup over the original RIB, when original content is
 *       relevant.
 *
 * \see `Bgp_RebuildMsgFromRib()`
 */
#define BGPF_CLEARUNREACH BIT(7)

/** @} */

/**
 * \name BGP parameter codes
 *
 * @{
 */

/// BGP parameter capability code.
#define BGP_PARM_CAPABILITY U8_C(2)

typedef Uint8 BgpParmCode;  ///< Octet inside a `Bgpparm' identifying the parameter

/** @} */

/**
 * \name BGP capability codes
 *
 * \see [IANA Assigned Capability Codes](https://www.iana.org/assignments/capability-codes/capability-codes.xhtml)
 *
 * @{
 */

#define BGP_CAP_RESERVED                             U8_C(0)    ///< RFC 5492
#define BGP_CAP_MULTIPROTOCOL                        U8_C(1)    ///< RFC 2858
#define BGP_CAP_ROUTE_REFRESH                        U8_C(2)    ///< RFC 2918
#define BGP_CAP_COOPERATIVE_ROUTE_FILTERING          U8_C(3)    ///< RFC 5291
#define BGP_CAP_MULTIPLE_ROUTES_DEST                 U8_C(4)    ///< RFC 3107
#define BGP_CAP_EXTENDED_NEXT_HOP                    U8_C(5)    ///< RFC 5549
#define BGP_CAP_EXTENDED_MESSAGE                     U8_C(6)    ///< https://tools.ietf.org/html/draft-ietf-idr-bgp-extended-messages-21
#define BGP_CAP_BGPSEC                               U8_C(7)    ///< https://tools.ietf.org/html/draft-ietf-sidr-bgpsec-protocol-23
#define BGP_CAP_MULTIPLE_LABELS                      U8_C(8)    ///< https://www.iana.org/go/rfc8277
#define BGP_CAP_BGP_ROLE                             U8_C(9)    ///< draft-ietf-idr-bgp-open-policy
#define BGP_CAP_GRACEFUL_RESTART                     U8_C(64)   ///< RFC 4724
#define BGP_CAP_ASN32BIT                             U8_C(65)   ///< RFC 6793
#define BGP_CAP_DYNAMIC_CISCO                        U8_C(66)   ///< Cisco version of Dynamic capability
#define BGP_CAP_DYNAMIC                              U8_C(67)
#define BGP_CAP_MULTISESSION                         U8_C(68)   ///< draft-ietf-idr-bgp-multisession
#define BGP_CAP_ADD_PATH                             U8_C(69)   ///< RFC 7911
#define BGP_CAP_ENHANCED_ROUTE_REFRESH               U8_C(70)   ///< RFC 7313
#define BGP_CAP_LONG_LIVED_GRACEFUL_RESTART          U8_C(71)   ///< https://tools.ietf.org/html/draft-uttaro-idr-bgp-persistence-03
#define BGP_CAP_CP_ORF                               U8_C(72)   ///< RFC 7543
#define BGP_CAP_FQDN                                 U8_C(73)   ///< https://tools.ietf.org/html/draft-walton-bgp-hostname-capability-02
#define BGP_CAP_ROUTE_REFRESH_CISCO                  U8_C(128)  ///< Deprecated https://www.iana.org/go/rfc8810
#define BGP_CAP_ORF_CISCO                            U8_C(130)  ///< Deprecated https://www.iana.org/go/rfc8810
#define BGP_CAP_MULTISESSION_CISCO                   U8_C(131)  ///< Cisco version of multisession_bgp

typedef Uint8 BgpCapCode;

/** @} */

/**
 * \name Known BGP attribute codes
 *
 * @{
 */

#define BGP_ATTR_ORIGIN                                   U8_C(1)
#define BGP_ATTR_AS_PATH                                  U8_C(2)
#define BGP_ATTR_NEXT_HOP                                 U8_C(3)
#define BGP_ATTR_MULTI_EXIT_DISC                          U8_C(4)
#define BGP_ATTR_LOCAL_PREF                               U8_C(5)
#define BGP_ATTR_ATOMIC_AGGREGATE                         U8_C(6)
#define BGP_ATTR_AGGREGATOR                               U8_C(7)
#define BGP_ATTR_COMMUNITY                                U8_C(8)
#define BGP_ATTR_ORIGINATOR_ID                            U8_C(9)
#define BGP_ATTR_CLUSTER_LIST                             U8_C(10)
#define BGP_ATTR_DPA                                      U8_C(11)
#define BGP_ATTR_ADVERTISER                               U8_C(12)
#define BGP_ATTR_RCID_PATH_CLUSTER_ID                     U8_C(13)
#define BGP_ATTR_MP_REACH_NLRI                            U8_C(14)
#define BGP_ATTR_MP_UNREACH_NLRI                          U8_C(15)
#define BGP_ATTR_EXTENDED_COMMUNITY                       U8_C(16)
#define BGP_ATTR_AS4_PATH                                 U8_C(17)
#define BGP_ATTR_AS4_AGGREGATOR                           U8_C(18)
#define BGP_ATTR_SAFI_SSA                                 U8_C(19)
#define BGP_ATTR_CONNECTOR                                U8_C(20)
#define BGP_ATTR_AS_PATHLIMIT                             U8_C(21)
#define BGP_ATTR_PMSI_TUNNEL                              U8_C(22)
#define BGP_ATTR_TUNNEL_ENCAPSULATION                     U8_C(23)
#define BGP_ATTR_TRAFFIC_ENGINEERING                      U8_C(24)
#define BGP_ATTR_IPV6_ADDRESS_SPECIFIC_EXTENDED_COMMUNITY U8_C(25)
#define BGP_ATTR_AIGP                                     U8_C(26)
#define BGP_ATTR_PE_DISTINGUISHER_LABELS                  U8_C(27)
#define BGP_ATTR_ENTROPY_LEVEL_CAPABILITY                 U8_C(28)
#define BGP_ATTR_LS                                       U8_C(29)
#define BGP_ATTR_LARGE_COMMUNITY                          U8_C(32)
#define BGP_ATTR_BGPSEC_PATH                              U8_C(33)
#define BGP_ATTR_COMMUNITY_CONTAINER                      U8_C(34)
#define BGP_ATTR_PREFIX_SID                               U8_C(40)
#define BGP_ATTR_SET                                      U8_C(128)
#define BGP_ATTR_RESERVED                                 U8_C(255)

typedef Uint8 BgpAttrCode;

/** @} */

/// Length of a fast attribute lookup table.
#define BGP_ATTRTAB_LEN 12  // NOTE: Keep in sync with attributes.c

// Low level BGP attributes offset table manipulation

/// Fast attribute lookup table, keep track of most relevant BGP attributes offset inside TPA.
typedef Sint16 Bgpattrtab[BGP_ATTRTAB_LEN];  // NOTE: despite being Sint16, it's still used as if Uint16

// Special offsets inside Bgpattrtab

/// Attribute offset is yet unknown (special value for `Bgpattrtab`).
#define BGP_ATTR_UNKNOWN  -1  // NOTE: 0xffff, DO NOT change! assumed by memset()!
/// Attribute not present inside message TPA (special value for `Bgpattrtab`).
#define BGP_ATTR_NOTFOUND -2

/// `BgpAttrCode` -> index inside `Bgpattrtab` translation table, -1 for "Not available".
extern const Sint8 bgp_attrTabIdx[256];

/// Reset all `Bgpattrtab` entries to `BGP_ATTR_UNKNOWN`.
FORCE_INLINE void BGP_CLRATTRTAB(Bgpattrtab tab)
{
	EXTERNC void *memset(void *, int, size_t);
	STATIC_ASSERT(BGP_ATTR_UNKNOWN == -1, "memset() assumes BGP_ATTR_UNKNOWN == -1");

	memset(tab, 0xff, BGP_ATTRTAB_LEN * sizeof(*tab));
}

/**
 * \brief BGP message.
 *
 * `Bgpmsg` holds memory and metadata required to properly
 * intepret a BGP message of any type. This structure
 * should not be used to build a BGP message from scratch.
 * Its model is biased towards zero-copy and minimal overhead BGP
 * decoding. No actual consistency check over BGP data occurs unless it is
 * explicitly accessed, thus any operation may fail on corrupted data.
 *
 * The underlying message buffer may be allocated upon initialization
 * (`Bgpmsg` operates on an owned message buffer), or be borrowed as is
 * (`Bgpmsg` operates on an unowned message buffer).
 * In the latter case the buffer is understood to remain valid
 * for the entire lifetime of the message structure, and is not freed on
 * `Bgp_ClearMsg()`.
 *
 * An optional `MemOps` interface may be provided to customize
 * memory allocation (e.g. implement memory pools for BGP messages).
 * `allocp` and `memOps` fields are never modified by any function,
 * thus a `Bgpmsg` structure willing to customize its memory
 * allocation policy should initialize in advance these fields for the API
 * to use it.
 * If `memOps` is `NULL`, `Mem_StdOps` is used.
 *
 * \see [RFC 4271](https://tools.ietf.org/html/rfc4271)
 */
typedef struct {
	Uint8 *buf;            ///< Underlying message buffer
	Uint16 flags;          ///< Message flags, any of the `BGPF_*` flag

	void         *allocp;  ///< Optional allocator for custom memory allocation policy
	const MemOps *memOps;  ///< Optional custom memory allocation operations

	Bgpattrtab table;      ///< Fast attribute lookup table
} Bgpmsg;

/// Move `Bgpmsg` contents from `src` to `dest`, leaving `src` empty.
FORCE_INLINE void BGP_MOVEMSG(Bgpmsg *dest, Bgpmsg *src)
{
	EXTERNC void *memcpy(void *, const void *, size_t);

	memcpy(dest, src, sizeof(*dest));
	src->buf = NULL;
}

/// Test `Bgpmsg` flags field for `BGPF_UNOWNED`.
FORCE_INLINE Boolean BGP_ISUNOWNED(Uint16 flags)
{
	return (flags & BGPF_UNOWNED) != 0;
}

/// Test `Bgpmsg` flags field for `BGPF_ASN32BIT`.
FORCE_INLINE Boolean BGP_ISASN32BIT(Uint16 flags)
{
	return (flags & BGPF_ASN32BIT) != 0;
}

/// Test `Bgpmsg` flags field for `BGPF_ADDPATH`.
FORCE_INLINE Boolean BGP_ISADDPATH(Uint16 flags)
{
	return (flags & BGPF_ADDPATH) != 0;
}

/// Test `Bgpmsg` flags field for `BGPF_EXMSG`.
FORCE_INLINE Boolean BGP_ISEXMSG(Uint16 flags)
{
	return (flags & BGPF_EXMSG) != 0;
}

/// Retrieve memory operations associated with `msg`.
FORCE_INLINE const MemOps *BGP_MEMOPS(const Bgpmsg *msg)
{
	return msg->memOps ? msg->memOps : Mem_StdOps;
}

/**
 * \name BGP NOTIFICATION error codes
 *
 * @{
 */

#define BGP_ERRC_UNSPEC  U8_C(0)
#define BGP_ERRC_MSGHDR  U8_C(1)
#define BGP_ERRC_OPEN    U8_C(2)
#define BGP_ERRC_UPDATE  U8_C(3)
#define BGP_ERRC_EXPIRED U8_C(4)
#define BGP_ERRC_FSM     U8_C(5)
#define BGP_ERRC_CEASE   U8_C(6)

/// [BGP NOTIFICATION](\ref Bgpnotification) error code octet.
typedef Uint8 BgpErrCode;

/**
 * \brief [BGP NOTIFICATION](\ref Bgpnotification) error subcode octet.
 *
 * Depending on the NOTIFICATION message error code, its subcode
 * field further elaborates on the BGP error.
 */
typedef Uint8 BgpErrSubCode;

/** @} */

// Specific BGP message types
#pragma pack(push, 1)

/**
 * \brief BGP message header.
 * \see   [RFC 4271](https://tools.ietf.org/html/rfc4271#section-6.1)
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint8   marker[BGP_MARKER_LEN];  ///< Marker, should be 0xff filled
	Uint16  len;                     ///< Message length in bytes, including header itself
	BgpType type;                    ///< Message type
} Bgphdr;

/// Retrieve pointer to BGP message header.
FORCE_INLINE Bgphdr *BGP_HDR(const Bgpmsg *msg)
{
	return (Bgphdr *) msg->buf;
}

/**
 * \brief BGP OPEN message.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Bgphdr hdr;                ///< Common message header
	Uint8  version;            ///< Supported BGP version (typically 4)
	Uint16 holdTime;           ///< BGP session hold time in seconds
	Uint16 myAs;               ///< Sender ASN16
	Uint32 bgpId;              ///< IPv4 address
	Uint8  parmsLen;           ///< Subsequent parameters field length
	Uint8  parms[FLEX_ARRAY];  ///< Open message parameters
} Bgpopen;

/**
 * \brief BGP OPEN message parameters segment.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint8 len;                ///< `parms` field length in bytes
	Uint8 parms[FLEX_ARRAY];  ///< Variable-sized portion of parameters
} Bgpparmseg;

/**
 * \brief Parameter inside `Bgpparmseg`.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	BgpParmCode code;              ///< Parameter type octet
	Uint8       len;               ///< `data` field length in bytes
	Uint8       data[FLEX_ARRAY];  ///< Parameter contents
} Bgpparm;

/**
 * \brief CAPABILITY inside a parameter with code `BGP_PARM_CAPABILITY`.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	BgpCapCode code;              ///< BGP capability code
	Uint8      len;               ///< Capability length in bytes, **excluding** the actual header
	Uint8      data[FLEX_ARRAY];  ///< Capability contents
} Bgpcap;

/// Return a direct pointer over `Bgpopen` parameters segment.
FORCE_INLINE Bgpparmseg *BGP_OPENPARMS(const Bgpopen *open)
{
	return (Bgpparmseg *) &open->parmsLen;
}

/**
 * \brief BGP message of type UPDATE, including its header.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Bgphdr hdr;
	Uint8  data[FLEX_ARRAY];
} Bgpupdate;

typedef ALIGNED(1, struct) {
	Uint16 len;
	Uint8  nlri[FLEX_ARRAY];
} Bgpwithdrawnseg;

typedef ALIGNED(1, struct) {
	Uint16 len;
	Uint8  attrs[FLEX_ARRAY];
} Bgpattrseg;

FORCE_INLINE Bgpwithdrawnseg *BGP_WITHDRAWN(const Bgpupdate *msg)
{
	return (Bgpwithdrawnseg *) msg->data;
}

FORCE_INLINE Bgpattrseg *BGP_ATTRIBUTES(const Bgpupdate *msg)
{
	const Bgpwithdrawnseg *w = BGP_WITHDRAWN(msg);
	return (Bgpattrseg *) (w->nlri + BE16(w->len));
}

FORCE_INLINE void *BGP_ANNOUNCEMENTS(const Bgpupdate *msg)
{
	const Bgpattrseg *tpa = BGP_ATTRIBUTES(msg);
	return (void *) (tpa->attrs + BE16(tpa->len));
}

// NOTE: NLRI segment is handled with no struct

/// Attribute length has an additional byte.
#define BGP_ATTR_EXTENDED   BIT(4)
/// Partial attribute flag.
#define BGP_ATTR_PARTIAL    BIT(5)
/// Transitive attribute flag.
#define BGP_ATTR_TRANSITIVE BIT(6)
/// Optional attribute flag.
#define BGP_ATTR_OPTIONAL   BIT(7)

typedef ALIGNED(1, struct) {
	Uint8       flags;
	BgpAttrCode code;
	Uint8       len;
} Bgpattr;

typedef ALIGNED(1, struct) {
	Uint8       flags;
	BgpAttrCode code;
	Uint16      len;
} Bgpattrex;

FORCE_INLINE Boolean BGP_ISATTREXT(Uint16 flags)
{
	return (flags & BGP_ATTR_EXTENDED) != 0;
}

FORCE_INLINE Boolean BGP_ISATTRPART(Uint16 flags)
{
	return (flags & BGP_ATTR_PARTIAL) != 0;
}

FORCE_INLINE Boolean BGP_ISATTRTRANS(Uint16 flags)
{
	return (flags & BGP_ATTR_TRANSITIVE) != 0;
}

FORCE_INLINE Boolean BGP_ISATTROPT(Uint16 flags)
{
	return (flags & BGP_ATTR_OPTIONAL) != 0;
}

FORCE_INLINE size_t BGP_ATTRHDRSIZ(const Bgpattr *attr)
{
	return 3 + BGP_ISATTREXT(attr->flags);
}

FORCE_INLINE size_t BGP_ATTRLEN(const Bgpattr *attr)
{
	return BGP_ISATTREXT(attr->flags) ?
	       BE16(((const Bgpattrex *) attr)->len) :
	       attr->len;
}

FORCE_INLINE void *BGP_ATTRPTR(const Bgpattr *attr)
{
	return (Uint8 *) attr + BGP_ATTRHDRSIZ(attr);
}

/**
 * \name Possible values of BGP ORIGIN attribute.
 *
 * @{
 */
#define BGP_ORIGIN_IGP        U8_C(0)
#define BGP_ORIGIN_EGP        U8_C(1)
#define BGP_ORIGIN_INCOMPLETE U8_C(2)

/// Octet type for BGP ORIGIN attribute code.
typedef Uint8 BgpOriginCode;

/** @} */

FORCE_INLINE Boolean BGP_CHKORIGINSIZ(const Bgpattr *attr)
{
	return BGP_ATTRLEN(attr) == 1;
}

FORCE_INLINE BgpOriginCode BGP_ORIGIN(const Bgpattr *attr)
{
	return *(const BgpOriginCode *) BGP_ATTRPTR(attr);
}

/// Test whether the length of the provided NEXT_HOP attribute is sensible.
FORCE_INLINE Boolean BGP_CHKNEXTHOPLEN(const Bgpattr *nextHop)
{
	return BGP_ATTRLEN(nextHop) == IPV4_SIZE;
}

FORCE_INLINE Boolean BGP_CHKATOMICAGGRSIZ(const Bgpattr *aggr)
{
	return BGP_ATTRLEN(aggr) == 0;
}

/**
 * \brief Contents of attributes of types `BGP_ATTR_AGGREGATOR` or `BGP_ATTR_AS4_AGGREGATOR`.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	union {
		struct {
			Asn16   asn;
			Ipv4adr addr;
		} a16;
		struct {
			Asn32   asn;
			Ipv4adr addr;
		} a32;
	};
} Bgpaggr;

FORCE_INLINE Boolean BGP_CHKAGGRSIZ(const Bgpattr *aggr, Boolean isAsn32bit)
{
	return (2uLL << (isAsn32bit != 0)) + IPV4_SIZE == BGP_ATTRLEN(aggr);
}

FORCE_INLINE Asn BGP_AGGRAS(const Bgpaggr *aggr, Boolean isAsn32bit)
{
	return (isAsn32bit) ? ASN32BIT(aggr->a32.asn) : ASN16BIT(aggr->a16.asn);
}

FORCE_INLINE Ipv4adr BGP_AGGRADDR(const Bgpaggr *aggr, Boolean isAsn32bit)
{
	return (isAsn32bit) ? aggr->a32.addr : aggr->a16.addr;
}

#define AS_SET      U8_C(1)
#define AS_SEQUENCE U8_C(2)

typedef Uint8 AsSegType;


/// AS_PATH segment.
typedef ALIGNED(1, struct) {
	AsSegType type;              ///< segment type
	Uint8     len;               ///< count of ASes (as opposed to bytes!) in `data`
	Uint8     data[FLEX_ARRAY];  ///< contains `len` ASes (as opposed to `len` bytes!)
} Asseg;

/**
 * \name BGP communities
 *
 * \see [IANA Well known communities list](https://www.iana.org/assignments/bgp-well-known-communities/bgp-well-known-communities.xhtml)
 *
 * @{
 */
#define BGP_COMMUNITY_PLANNED_SHUT               BE32(0xffff0000u)
#define BGP_COMMUNITY_ACCEPT_OWN                 BE32(0xffff0001u)
#define BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V4 BE32(0xffff0002u)
#define BGP_COMMUNITY_ROUTE_FILTER_V4            BE32(0xffff0003u)
#define BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V6 BE32(0xffff0004u)
#define BGP_COMMUNITY_ROUTE_FILTER_V6            BE32(0xffff0005u)
#define BGP_COMMUNITY_LLGR_STALE                 BE32(0xffff0006u)
#define BGP_COMMUNITY_NO_LLGR                    BE32(0xffff0007u)
#define BGP_COMMUNITY_ACCEPT_OWN_NEXTHOP         BE32(0xffff0008u)
#define BGP_COMMUNITY_STANDBY_PE                 BE32(0xffff0009u)
#define BGP_COMMUNITY_BLACKHOLE                  BE32(0xffff029au)
#define BGP_COMMUNITY_NO_EXPORT                  BE32(0xffffff01u)
#define BGP_COMMUNITY_NO_ADVERTISE               BE32(0xffffff02u)
#define BGP_COMMUNITY_NO_EXPORT_SUBCONFED        BE32(0xffffff03u)
#define BGP_COMMUNITY_NO_PEER                    BE32(0xffffff04u)

/// BGP Community code type, 4 octects, network byte order (big endian).
typedef Uint32 BgpCommCode;

/**
 * \brief BGP COMMUNITY type.
 *
 * \warning **Misaligned union**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, union) {
	struct {
		Uint16 hi;
		Uint16 lo;
	};
	BgpCommCode code;
} Bgpcomm;

/**
 * \brief BGP EXTENDED_COMMUNITY type.
 *
 * \warning **Misaligned union**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint8 typeHi;
	union {
		struct {
			Uint8 typeLo;
			Uint8 value[6];
		};
		Uint8 exValue[7];
	};
} Bgpexcomm;

/**
 * \brief BGP LARGE_COMMUNITY type.
 *
 * \warning **Misaligned union**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint32 global;
	Uint32 local1;
	Uint32 local2;
} Bgplgcomm;

/** @} */

/**
 * \brief Address family information found in `MP_REACH_NLRI` and
 * `MP_UNREACH_NLRI` attributes.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Afi  afi;   ///< Address Family Identifier
	Safi safi;  ///< Subsequent Address Family Identifier
} Bgpmpfam;

/**
 * MP_REACH_NLRI NEXT_HOP segment: length followed by network addresses.
 *
 * \warning **Misaligned struct**.
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Uint8 len;               ///< Subsequent address length in bytes
	Uint8 data[FLEX_ARRAY];  ///< Actual stored address, `len` bytes wide
} Bgpmpnexthop;

FORCE_INLINE Bgpmpfam *BGP_MPFAMILY(const Bgpattr *attr)
{
	return (Bgpmpfam *) BGP_ATTRPTR(attr);
}

FORCE_INLINE Bgpmpnexthop *BGP_MPNEXTHOP(const Bgpattr *attr)
{
	return (Bgpmpnexthop *) ((Uint8 *) BGP_ATTRPTR(attr) + 2 + 1);
}

FORCE_INLINE Boolean BGP_CHKMPREACH(const Bgpattr *attr)
{
	// AFI(2), SAFI(1), NEXT_HOP(1 + var.), RESERVED(1), NLRI(var.)
	size_t len = BGP_ATTRLEN(attr);

	return len >= 2uLL + 1uLL + 1uLL &&
	       len >= 2uLL + 1uLL + 1uLL + BGP_MPNEXTHOP(attr)->len + 1uLL;
}

FORCE_INLINE Boolean BGP_CHKMPUNREACH(const Bgpattr *attr)
{
	return BGP_ATTRLEN(attr) >= 2 + 1;  // AFI(2), SAFI(1), NLRI
}

/**
 * \brief BGP NOTIFICATION message.
 *
 * \warning **Misaligned struct**
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Bgphdr        hdr;
	BgpErrCode    code;     ///< Error code
	BgpErrSubCode subcode;  ///< Error category subcode
	Uint8         msg[FLEX_ARRAY];
} Bgpnotification;

/**
 * \brief BGP ROUTE_REFRESH message.
 * \see   [RFC 2918](https://tools.ietf.org/html/rfc2918)
 *
 * \warning **Misaligned struct**
 * \note    Fields are in network order (big endian).
 */
typedef ALIGNED(1, struct) {
	Bgphdr hdr;       ///< Common BGP message header
	Afi    afi;
	Uint8  reserved;  ///< Reserved field, should be set to 0
	Safi   safi;
} Bgprouteref;

#pragma pack(pop)  // --- misaligned packing END ---

STATIC_ASSERT(BGP_HDRSIZ == sizeof(Bgphdr), "BGP_HDRSIZ vs Bgphdr size mismatch");

typedef struct {
	Uint8 *base, *lim;
	Uint8 *ptr;
} Bgpparmiter;

typedef struct {
	Bgpparmiter pi;
	Uint8      *base, *lim;
	Uint8      *ptr;
} Bgpcapiter;

/**
 * \brief BGP attribute iterator.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Uint8 *base, *lim;
	Uint8 *ptr;

	Sint16 *table;        // NOTE: accessed atomically as if Uint16 *
	Uint64  attrMask[4];  // bitset to keep track of what we've seen so far
} Bgpattriter;

/**
 * \brief AS segment iterator.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Uint8   *base, *lim;
	Uint8   *ptr;
	Boolean8 asn32bit;
} Assegiter;

/**
 * \brief AS path iterator, reconstructs a BGP message AS path.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Assegiter segs;        ///< Segments iterator
	Bgpattr  *nextAttr;    ///< If any AS4_PATH is found, this references it
	Uint8    *base, *lim;
	Uint8    *ptr;
	Uint16    asIdx;       // NOTE: Equals to 0xffffu when no ASN has been returned yet
	Uint16    maxCount;    // NOTE: Could be set to 0xffffu when no limit is imposed
} Aspathiter;

/// Retrieve the current AS segment being iterated inside message AS_PATH.
FORCE_INLINE Asseg *BGP_CURASSEG(const Aspathiter *it)
{
	return (Asseg *) it->base;
}

FORCE_INLINE Uint16 BGP_CURASINDEX(const Aspathiter *it)
{
	return it->asIdx;
}

/**
 * \brief MultiProtocol prefix iterator.
 *
 * Traverse prefixes coming from a multiprotocol enabled BGP message.
 * Returns prefixes from the ATTRIBUTE
 * field, when MP_REACH or MP_UNREACH attribute is available,
 * and from the WITHDRAWN or NLRI fields.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Prefixiter rng;        ///< Current iterator
	Bgpattr   *nextAttr;   ///< Additional MP_REACH or MP_UNREACH attribute to be iterated
	Prefix     pfx;        ///< Storage for prefix
} Bgpmpiter;

FORCE_INLINE Uint8 *BGP_CURMPBASE(const Bgpmpiter *it)
{
	return it->rng.base;
}

FORCE_INLINE Uint8 *BGP_CURMPLIM(const Bgpmpiter *it)
{
	return it->rng.lim;
}

/**
 * \brief Retrieve pointer to current prefix inside BGP data.
 *
 * If prefix had PATH ID information the returned pointer references an
 * `ApRawPrefix`, otherwise a `RawPrefix`.
 */
FORCE_INLINE void *BGP_CURMPPFX(const Bgpmpiter *it)
{
	return it->rng.ptr - PFXLEN(it->pfx.width) - 1 - ((it->rng.isAddPath != 0) << 2);
}

/**
 * \brief Retrieve a pointer to the current prefix inside the BGP data.
 *
 * Always returns a `RawPrefix`  pointer, if PATH ID information is
 * available it is discarded.
 */
FORCE_INLINE RawPrefix *BGP_CURMPRAWPFX(const Bgpmpiter *it)
{
	return (RawPrefix *) (it->rng.ptr - PFXLEN(it->pfx.width) - 1);
}

/**
 * \brief BGP UPDATE message NEXT_HOP iterator, returns every NEXT_HOP listed
 *        inside message across different attributes in sequence.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Bgpattr *nextHop;
	Bgpattr *mpReach;
	Uint8   *base, *lim;
	Uint8   *ptr;
	Afi      afi;
	Safi     safi;
	Boolean8 isRibv2;     // whether we're dealing with a TABLE_DUMPV2 RIB mpReach
	Afi      afiRibv2;    // only meaningful if isRibv2
	Safi     safiRibv2;   // ditto
	Ipadr    addr;
} Nexthopiter;

FORCE_INLINE Bgpattr *BGP_CURNEXTHOPATTR(const Nexthopiter *it)
{
	return (Bgpattr *) it->base;
}

FORCE_INLINE Afi BGP_CURNEXTHOPAFI(const Nexthopiter *it)
{
	return it->afi;
}

FORCE_INLINE Safi BGP_CURNEXTHOPSAFI(const Nexthopiter *it)
{
	return it->safi;
}

/**
 * \brief BGP community attribute iterator.
 *
 * May iterate COMMUNITY, EXTENDED_COMMUNITY or LARGE_COMMUNITY.
 *
 * \note `struct` should be considered opaque.
 */
typedef struct {
	Uint8 *base, *lim;
	Uint8 *ptr;
} Bgpcommiter;

/**
 * \brief Initialize message from existing data.
 *
 * \param [in,out] msg    Cleared BGP message, must not be `NULL`
 * \param [in]     data   Raw bytes to populate the message
 * \param [in]     nbytes Bytes count inside `data`
 * \param [in]     flags  BGP initialization flags
 *
 * \return `OK` on success, `NG` on failure.
 *         Sets BGP error, see `Bgp_GetErrStat()`.
 *
 * \note Uses `msg->allocp` and `msg->memOps` to allocate memory as needed.
 */
Judgement Bgp_MsgFromBuf(Bgpmsg *msg, const void *data, size_t nbytes, unsigned flags);

/**
 * \brief Read a BGP message from stream.
 *
 * \param [in,out] msg    Cleared BGP message, must not be `NULL`
 * \param [in]     streamp Stream pointer to read from
 * \param [in]     ops     Stream operations, must not be `NULL` and provide `Read()`
 * \param [in]     flags  BGP initialization flags
 *
 * \return `OK` on success, `NG` on failure.
 *         Sets BGP error, see `Bgp_GetErrStat()`.
 *
 * \note Uses `msg->allocp` and `msg->memOps` to allocate memory as needed.
 *
 * \note Flag `BGPF_UNOWNED` is ignored by this function.
 */
Judgement Bgp_ReadMsg(Bgpmsg *msg, void *streamp, const StmOps *ops, unsigned flags);

/**
 * \brief Access contents of a BGP message.
 *
 * Function shall fail if `msg` doesn't belong to the appropriate
 * message type (e.g. trying to access a BGP UPDATE message while `msg` type
 * is `BGP_OPEN`).
 *
 * \return Direct pointer to BGP message contents, `NULL` on error.
 *         Sets BGP error, see `Bgp_GetErrStat()`.
 *
 * @{
 */

Bgpopen *Bgp_GetMsgOpen(Bgpmsg *msg);
Bgpupdate *Bgp_GetMsgUpdate(Bgpmsg *msg);
Bgpnotification *Bgp_GetMsgNotification(Bgpmsg *msg);
Bgprouteref *Bgp_GetMsgRouteRefresh(Bgpmsg *msg);

/** @} */

/**
 * \brief Assume correct BGP message type and retrieve a pointer to its content.
 *
 * \param [in] msg A BGP message, must not be `NULL` and have correct header and type!
 *
 * \return Direct pointer to BGP message contents.
 *
 * @{
 */

FORCE_INLINE Bgpopen *BGP_MSGOPEN(const Bgpmsg *msg)
{
	return (Bgpopen *) msg->buf;
}

FORCE_INLINE Bgpupdate *BGP_MSGUPDATE(const Bgpmsg *msg)
{
	return (Bgpupdate *) msg->buf;
}

FORCE_INLINE Bgpnotification *BGP_MSGNOTIFICATION(const Bgpmsg *msg)
{
	return (Bgpnotification *) msg->buf;
}

FORCE_INLINE Bgprouteref *BGP_MSGROUTEREFRESH(const Bgpmsg *msg)
{
	return (Bgprouteref *) msg->buf;
}

/** @} */

// NOTE: No function is provided to access BGP KEEPALIVE or CLOSE message,
//       since their entire content is the BGP header.

/// Clear BGP message and free any owned memory.
void Bgp_ClearMsg(Bgpmsg *msg);

// OPEN Message specific
// =====================

/**
 * \brief Retrieve pointer to BGP OPEN message's parameters field.
 *
 * \see `Bgp_GetMsgOpen()`
 */
Bgpparmseg *Bgp_GetOpenParms(const Bgpopen *msg);
Judgement Bgp_StartMsgParms(Bgpparmiter *it, Bgpmsg *msg);
void Bgp_StartParms(Bgpparmiter *it, const Bgpparmseg *tpa);
Bgpparm *Bgp_NextParm(Bgpparmiter *it);
Judgement Bgp_StartMsgCaps(Bgpcapiter *it, Bgpmsg *msg);
void Bgp_StartCaps(Bgpcapiter *it, const Bgpparmseg *tpa);
Bgpcap *Bgp_NextCap(Bgpcapiter *it);

// UPDATE Message specific
// =======================

Bgpwithdrawnseg *Bgp_GetUpdateWithdrawn(const Bgpupdate *msg);
Bgpattrseg *Bgp_GetUpdateAttributes(const Bgpupdate *msg);
void *Bgp_GetUpdateNlri(const Bgpupdate *msg, size_t *nbytes);

/// Like `Bgp_GetMsgAttribute()`, but operates directly on TPA.
Bgpattr *Bgp_GetUpdateAttribute(const Bgpattrseg *tpa, BgpAttrCode code, Bgpattrtab tab);
/**
 * \brief Direct lookup of a specific BGP UPDATE attribute inside message TPA.
 *
 * Uses `msg->tab` to accelerate common attribute lookup.
 *
 * \return Attribute with type `code` inside `msg` TPA on success,
 *         `NULL` if attribute wasn't found or when error occurred.
 *         Sets BGP error, see `Bgp_GetErrStat()`.
 */
Bgpattr *Bgp_GetMsgAttribute(Bgpmsg *msg, BgpAttrCode code);

void Bgp_StartUpdateAttributes(Bgpattriter *it, const Bgpattrseg *tpa, Bgpattrtab tab);
Judgement Bgp_StartMsgAttributes(Bgpattriter *it, Bgpmsg *msg);

Bgpattr *Bgp_NextAttribute(Bgpattriter *it);

/// Start iterating BGP UPDATE message WITHDRAWN segment.
Judgement Bgp_StartMsgWithdrawn(Prefixiter *it, Bgpmsg *msg);
Judgement Bgp_StartAllMsgWithdrawn(Bgpmpiter*it, Bgpmsg *msg);

/**
 * \brief Start iterating BGP UPDATE message NLRI segment.
 *
 * \param [out] it  Iterator to be initialized, must not be `NULL`
 * \param [in]  msg Message whose NLRI segment is to be iterated, must not be `NULL`
 *
 * \return `OK` on success, `NG` on error. Sets BGP error state,
 *         see `Bgp_GetErrStat()`.
 */
Judgement Bgp_StartMsgNlri(Prefixiter *it, Bgpmsg *msg);
Judgement Bgp_StartAllMsgNlri(Bgpmpiter *it, Bgpmsg *msg);

/**
 * \brief Get current prefix and advance iterator
 *
 * \return Current prefix, `NULL` on error or end of iteration.
 *         Sets BGP error, see `Bgp_GetErrStat()`.
 */
Prefix *Bgp_NextMpPrefix(Bgpmpiter *it);

/**
 * \brief Return the BGP aggregator attribute.
 *
 * Second argument is an input-output ASN32BIT flag argument.
 * It should be initially set to TRUE if the BGP message has ASN32BIT
 * capability, FALSE otherwise.
 * Flag is set to TRUE on return if the resolved AGGREGATOR attribute is
 * 32-bits wide.
 */
Bgpattr *Bgp_GetRealAggregator(const Bgpattrseg *tpa, Boolean *isAsn32bit, Bgpattrtab tab);
/// Like `Bgp_GetRealAggregator()`, but operates on `Bgpmsg`.
Bgpattr *Bgp_GetRealMsgAggregator(Bgpmsg *msg, Boolean *isAsn32bit);
Judgement Bgp_StartAllNextHops(Nexthopiter *it, const Bgpattrseg *tpa, Bgpattrtab tab);
Judgement Bgp_StartAllMsgNextHops(Nexthopiter *it, Bgpmsg *msg);
Ipadr *Bgp_NextNextHop(Nexthopiter *it);

Judgement Bgp_StartRealAsPath(Aspathiter *it, const Bgpattrseg *tpa, Boolean isAsn32bit, Bgpattrtab tab);
Judgement Bgp_StartMsgRealAsPath(Aspathiter *it, Bgpmsg *msg);
Asn Bgp_NextAsPath(Aspathiter *it);

// BGP Attributes
// =======================

Judgement Bgp_StartAsSegments(Assegiter *it, const Bgpattr *asPath, Boolean isAsn32bit);
Judgement Bgp_StartAs4Segments(Assegiter *it, const Bgpattr *asPath);
Asseg *Bgp_NextAsSegment(Assegiter *it);

Judgement Bgp_StartCommunity(Bgpcommiter *it, const Bgpattr *comm);
Bgpcomm *Bgp_NextCommunity(Bgpcommiter *it);
Bgpexcomm *Bgp_NextExtendedCommunity(Bgpcommiter *it);
Bgplgcomm *Bgp_NextLargeCommunity(Bgpcommiter *it);

Bgpmpfam *Bgp_GetMpFamily(const Bgpattr *attr);  // DEPRECATED
void *Bgp_GetMpNextHop(const Bgpattr *attr, size_t *nbytes); // DEPRECATED
void *Bgp_GetMpRoutes(const Bgpattr *attr, size_t *nbytes);  // DEPRECATED

#endif
