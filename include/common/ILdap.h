/*hdr
**      FILE:           ILdap.h
**      AUTHOR:         Matthew Allen
**      DATE:           21/6/2000
**      DESCRIPTION:    Open LDAP plugin
**
**      Copyright (C) 2000, Matthew Allen
**              fret@memecode.com
*/

#ifndef __ILDAP_H
#define __ILDAP_H

#include "INet.h"
#include "lber.h"
#include "ldap.h"
//#include "ldapconfig.h"

#define LDAP_PORT					389

#define LDAP_ENTRY_PERSON			0x0001
#define LDAP_ENTRY_GROUP			0x0002

class ILdapEntry : public GDomI
{
public:
	char *Name;
	char *Email;
	int Type;

	bool IsGroup() { return TestFlag(Type, LDAP_ENTRY_GROUP); }

	ILdapEntry();
	virtual ~ILdapEntry();
};

class ILdap
{
	LDAP *Ldap;
	int Opt_ProtocolVer;
	int Opt_Deref;
	bool Opt_Restart;

public:
	ILdap();
	virtual ~ILdap();

	void SetProtocol(int i);
	void SetDeref(int i);
	void SetRestart(bool i);

	bool Open(int Version, char *RemoteServer, int Port = LDAP_PORT, char *BindDn = NULL, char *Pass = NULL);
	bool Close();

	bool Search(GDom *Results, const char *BaseDn, const char *UserFilter, const char *Search, bool Recursive = true);
};

#endif
