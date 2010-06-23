/// \file
/// \author Matthew Allen
#ifndef _IDNS_H_
#define _IDNS_H_

/////////////////////////////////////////////////////////////////////////////////////////////////
// Defines
//
// Taken from:
// http://www.faqs.org/rfcs/rfc1035.html

// 3.2.2. TYPE values
#define DNS_TYPE_A			1	// Address
#define DNS_TYPE_NS			2	// Auth name server
#define DNS_TYPE_MD			3	// Mail destination
#define DNS_TYPE_MF			4	// Mail forwarder
#define DNS_TYPE_CNAME		5	// Canonical name for an alias
#define DNS_TYPE_SOA		6	// Start of authority
#define DNS_TYPE_MB			7
#define DNS_TYPE_MG			8
#define DNS_TYPE_MR			9
#define DNS_TYPE_NULL		10
#define DNS_TYPE_WKS		11
#define DNS_TYPE_PTR		12
#define DNS_TYPE_HINFO		13
#define DNS_TYPE_MINFO		14
#define DNS_TYPE_MX			15	// Mail exchange
#define DNS_TYPE_TXT		16

// 3.2.3. QTYPE values
#define DNS_QTYPE_AXFR		252 // A request for a transfer of an entire zone
#define DNS_QTYPE_MAILB		253	// A request for mailbox-related records (MB, MG or MR)
#define DNS_QTYPE_MAILA		254 // A request for mail agent RRs (Obsolete - see MX)
#define DNS_QTYPE_ALL		255 // A request for all records

// 3.2.4. CLASS values
#define DNS_CLASS_IN		1	// The Internet
#define DNS_CLASS_CS		2	// The CSNET class (Obsolete - used only for examples in some obsolete RFCs)
#define DNS_CLASS_CH		3	// The CHAOS class
#define DNS_CLASS_HS		4	// Hesiod [Dyer 87]

// 3.2.5. QCLASS values
#define DNS_QCLASS_ALL		255 // Any class

// Query types
#define DNS_Q_QUERY			0
#define DNS_Q_IQUERY		1
#define DNS_Q_STATUS		2

// Response codes
#define DNS_R_SUCCESS		0	// No error condition
#define DNS_R_FORMAT_ERROR	1	// The name server was unable to interpret the query.
#define DNS_R_SERVER_FAILED	2	// The name server was unable to process this query due
								// to a problem with the name server.
#define DNS_R_NAME_ERROR	3	// Meaningful only for responses from an authoritative name
                                // server, this code signifies that the
                                // domain name referenced in the query does not exist.
#define DNS_R_NOT_IMPL		4	// The name server does not support the requested kind of query.
#define DNS_R_REFUSED		5	// The name server refuses to perform the specified operation.

// General
#define DNS_PORT			53
#define DNS_MY_ID			2
#define DNS_TIMEOUT			10000	// 10s

/////////////////////////////////////////////////////////////////////////////////////////////////
// Data Structures

struct DnsMethods
{
	uchar *ReadLabel(void *Header, uchar *s, char *&Label);
	uchar *WriteLabel(uchar *s, char *Label);
};

// Header, sizeof = 12 bytes
struct DnsHeader
{
	uint16 Id;
	
	// Byte of flags
	uint16 RecursionDesired:1;
	uint16 IsTruncated:1;
	uint16 IsAuthoritative:1;
	uint16 QueryType:4;			// One of DNS_Q_???
	uint16 IsResponse:1;

	// Byte of flags
	uint16 ResponseCode:4;		// One of DNS_R_???
	uint16 Reserved:3;
	uint16 RecursionAvailable:1;
	
	// Number of querys
	uint16 QueryCount;

	// Number of resources
	uint16 ResourceCount;

	// Number of name servers
	uint16 NameServerCount;

	// Number of additionals
	uint16 AdditionalCount;
};

struct DnsTcpMessage : public DnsMethods
{
	uint16 Length;
	DnsHeader Header;
	uchar Data[4096];
};

struct DnsUdpMessage : public DnsMethods
{
	DnsHeader Header;
	uchar Data[4096];
};

/////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
bool GetDnsServers(GArray<char*> &Servers);
bool IDnsResolve(GArray<char*> &Results, char *Name, int Type = DNS_TYPE_A, int theclass = DNS_CLASS_IN);

#endif