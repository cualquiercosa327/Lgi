#ifndef __ILDAP_SERVER_H
#define __ILDAP_SERVER_H

#include "../../src/common/INet/Ber/Ber.h"
#include "GVariant.h"
#include "INet.h"

#define LDAP_PORT				389

#define WM_ILDAP_SERVER_DONE	(M_USER+0x240)
#define WM_ILDAP_ERROR_MSG		(M_USER+0x241)

enum ILdapField
{
	LdapFirstName = 1,
	LdapLastName,
	LdapEmail
};

class ILdapServerDb
{
public:
	virtual char *MapField(ILdapField Id) = 0;
	virtual int GetData(List<GDom> &Lst) = 0;
};

class ILdapServer : public GThread
{
	GView *Parent;
	ILdapServerDb *Db;
	GFile Log;
	GSocket Socket;
	GSocket Listen;
	List<GDom> Contacts;

	// Internal
	char *GetContactUid(GDom *c);
	void WriteField(EncBer *Parent, char *Name, char *Value, List<char> &Return);
	void ProcessFilter(DecBer *Filter, int FilterType, List<GDom> *Input, List<GDom> &Results);

	// Debug
	int TokenDump(uchar *Data, int Len, int Indent);
	void BerDump(char *Title, GBytePipe &Data);
	void HexDump(char *Title, GBytePipe &Data);

public:
	ILdapServer(GView *p, ILdapServerDb *db);
	~ILdapServer();

	int Main();
	void Send(EncBer &b);
	void OnMessage(DecBer &Msg);

	void Server();
};

#endif
