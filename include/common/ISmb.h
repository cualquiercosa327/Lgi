#ifndef __ISMB_H
#define __ISMB_H

#include "INet.h"

// Port
#define SMB_TCP_PORT						3020 // I think

// Command codes
#define SMB_COM_CREATE_DIRECTORY			0x00
#define SMB_COM_DELETE_DIRECTORY			0x01
#define SMB_COM_OPEN						0x02
#define SMB_COM_CREATE						0x03
#define SMB_COM_CLOSE						0x04
#define SMB_COM_FLUSH						0x05
#define SMB_COM_DELETE						0x06
#define SMB_COM_RENAME						0x07
#define SMB_COM_QUERY_INFORMATION			0x08
#define SMB_COM_SET_INFORMATION				0x09
#define SMB_COM_READ						0x0A
#define SMB_COM_WRITE						0x0B
#define SMB_COM_LOCK_BYTE_RANGE				0x0C
#define SMB_COM_UNLOCK_BYTE_RANGE			0x0D
#define SMB_COM_CREATE_TEMPORARY			0x0E
#define SMB_COM_CREATE_NEW					0x0F
#define SMB_COM_CHECK_DIRECTORY				0x10
#define SMB_COM_PROCESS_EXIT				0x11
#define SMB_COM_SEEK						0x12
#define SMB_COM_LOCK_AND_READ				0x13
#define SMB_COM_WRITE_AND_UNLOCK			0x14
#define SMB_COM_READ_RAW					0x1A
#define SMB_COM_READ_MPX					0x1B
#define SMB_COM_READ_MPX_SECONDARY			0x1C
#define SMB_COM_WRITE_RAW					0x1D
#define SMB_COM_WRITE_MPX					0x1E
#define SMB_COM_WRITE_COMPLETE				0x20
#define SMB_COM_SET_INFORMATION2			0x22
#define SMB_COM_QUERY_INFORMATION2			0x23
#define SMB_COM_LOCKING_ANDX				0x24
#define SMB_COM_TRANSACTION					0x25
#define SMB_COM_TRANSACTION_SECONDARY		0x26
#define SMB_COM_IOCTL						0x27
#define SMB_COM_IOCTL_SECONDARY				0x28
#define SMB_COM_COPY						0x29
#define SMB_COM_MOVE						0x2A
#define SMB_COM_ECHO						0x2B
#define SMB_COM_WRITE_AND_CLOSE				0x2C
#define SMB_COM_OPEN_ANDX					0x2D
#define SMB_COM_READ_ANDX					0x2E
#define SMB_COM_WRITE_ANDX					0x2F
#define SMB_COM_CLOSE_AND_TREE_DISC			0x31
#define SMB_COM_TRANSACTION2				0x32
#define SMB_COM_TRANSACTION2_SECONDARY		0x33
#define SMB_COM_FIND_CLOSE2					0x34
#define SMB_COM_FIND_NOTIFY_CLOSE			0x35
#define SMB_COM_TREE_CONNECT				0x70
#define SMB_COM_TREE_DISCONNECT				0x71
#define SMB_COM_NEGOTIATE					0x72
#define SMB_COM_SESSION_SETUP_ANDX			0x73
#define SMB_COM_LOGOFF_ANDX					0x74
#define SMB_COM_TREE_CONNECT_ANDX			0x75
#define SMB_COM_QUERY_INFORMATION_DISK		0x80
#define SMB_COM_SEARCH						0x81
#define SMB_COM_FIND						0x82
#define SMB_COM_FIND_UNIQUE					0x83
#define SMB_COM_NT_TRANSACT					0xA0
#define SMB_COM_NT_TRANSACT_SECONDARY		0xA1
#define SMB_COM_NT_CREATE_ANDX				0xA2
#define SMB_COM_NT_CANCEL					0xA4
#define SMB_COM_OPEN_PRINT_FILE				0xC0
#define SMB_COM_WRITE_PRINT_FILE			0xC1
#define SMB_COM_CLOSE_PRINT_FILE			0xC2
#define SMB_COM_GET_PRINT_QUEUE				0xC3
#define SMB_COM_READ_BULK					0xD8
#define SMB_COM_WRITE_BULK					0xD9
#define SMB_COM_WRITE_BULK_DATA				0xDA

// Flags field
#define SMB_FLAGS_CASELESS					0x04
#define SMB_FLAGS_SERVER_RESP				0x80

// Flags2 field
#define SMB_FLAGS2_KNOWS_LONG_NAMES			0x0001 
#define SMB_FLAGS2_KNOWS_EAS				0x0002
#define SMB_FLAGS2_SECURITY_SIGNATURE		0x0004
#define SMB_FLAGS2_IS_LONG_NAME				0x0040
#define SMB_FLAGS2_EXT_SEC					0x0800
#define SMB_FLAGS2_DFS						0x1000
#define	SMB_FLAGS2_PAGING_IO				0x2000
#define SMB_FLAGS2_ERR_STATUS				0x4000
#define	SMB_FLAGS2_UNICODE					0x8000

// Classes
class ISmb
{
	char Buffer[1024];
	GSocketI *Socket;	// commands
	int ResumeFrom;

public:
	Progress *Meter;

	ISmb();
	virtual ~ISmb();

	// Data
	GSocketI *Handle() { return Socket; }
	
	// Connection
	bool Open(GSocketI *S, char *RemoteHost, int Port = 0);
	bool Close();
	bool IsOpen();

	// File
	void SetResume(int i) { ResumeFrom = i; }
	bool GetFile(char *File, GBytePipe &Out);
};

#endif
