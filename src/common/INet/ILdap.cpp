/*hdr
**      FILE:           ILdap.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           21/6/2000
**      DESCRIPTION:    Open LDAP plugin
**
**      Copyright (C) 2000, Matthew Allen
**              fret@memecode.com
**
**		API doc URL:
**			http://docs.iplanet.com/docs/manuals/dirsdk/csdk30/functi99.htm#2215851
*/

#include <stdio.h>

#include "Lgi.h"
#include "GVariant.h"
#include "ILdap.h"

void _do_nothing(const char *s, ...) {}
#define Ldap_Trace _do_nothing

////////////////////////////////////////////////////////////////
ILdapEntry::ILdapEntry()
{
	Name = 0;
	Email = 0;
	Type = 0;
}

ILdapEntry::~ILdapEntry()
{
	DeleteArray(Name);
	DeleteArray(Email);
}

////////////////////////////////////////////////////////////////
ILdap::ILdap()
{
	Ldap = 0;
	Opt_ProtocolVer = LDAP_VERSION2;
	Opt_Deref = LDAP_DEREF_ALWAYS;
	Opt_Restart = true;
}

ILdap::~ILdap()
{
}

void ILdap::SetProtocol(int i)
{
	if (i >= LDAP_VERSION_MIN &&
		i <= LDAP_VERSION_MAX)
	{
		Opt_ProtocolVer = i;
	}
}

void ILdap::SetDeref(int i)
{
	Opt_Deref = i;
}

void ILdap::SetRestart(bool i)
{
	Opt_Restart = i;
}

bool ILdap::Open(int Version, char *RemoteServer, int Port, char *BindDn, char *Pass)
{
	Ldap = ldap_open(RemoteServer, Port ? Port : LDAP_PORT);
	if (Ldap)
	{
		ldap_set_option(Ldap, LDAP_OPT_RESTART, Opt_Restart ? LDAP_OPT_ON : LDAP_OPT_OFF);
		ldap_set_option(Ldap, LDAP_OPT_SIZELIMIT, LDAP_NO_LIMIT);
		ldap_set_option(Ldap, LDAP_OPT_DEREF, &Opt_Deref);
		ldap_set_option(Ldap, LDAP_OPT_PROTOCOL_VERSION, &Opt_ProtocolVer);

		
		if (ValidStr(User) && ValidStr(Pass))
		{
			int Result = ldap_simple_bind_s(Ldap, BindDn, Pass);
			if (Result != LDAP_SUCCESS)
			{
				return false;
			}
		}
		
		return true;
	}

	return false;
}

bool ILdap::Close()
{
	if (Ldap)
	{
		ldap_unbind(Ldap);
		Ldap = 0;
	}

	return false;
}

/*
ILdapEntry *ILdap::RetreiveOne(char *Name, char *Base, bool Recursive)
{
	if (Ldap && Name)
	{
		// create a filter string
		char Filter[256];
		sprintf(Filter, "(|(cn=%s)(givenName=%s))", Name, Name);

		// call a sync search
		LDAPMessage *Msg = 0;
	    int Result = ldap_search_s(	Ldap,
									Base ? Base : "",
									Recursive ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_ONELEVEL,
									Filter,
									NULL, // attr's to return
									false,
									&Msg);

		// bool SizeExceeded = Result == LDAP_SIZELIMIT_EXCEEDED;
		// bool TimeExceeded = Result == LDAP_TIMELIMIT_EXCEEDED;

		switch (Result)
		{
			case LDAP_SUCCESS:
			case LDAP_SIZELIMIT_EXCEEDED:
			case LDAP_TIMELIMIT_EXCEEDED:
			{
				// int Entries = ldap_count_entries(Ldap, Msg);

				for (LDAPMessage *m = ldap_first_entry(Ldap, Msg);
					m;
					m = ldap_next_entry(Ldap, m))
				{
					char **Type = ldap_get_values(Ldap, m, "objectClass");
					char **Name = ldap_get_values(Ldap, m, "cn");
					char **Email = ldap_get_values(Ldap, m, "rfc822Mailbox");

					if (Type && Name && Email)
					{
						ILdapEntry *e = new ILdapEntry;
						if (e)
						{
							if (Type && stricmp(Type[0], "groupOfNames") == 0)
							{
								e->Type = LDAP_ENTRY_GROUP;
							}
							else
							{
								e->Type = LDAP_ENTRY_PERSON;
							}

							if (Name)
							{
								e->Name = NewStr(Name[0]);
							}

							if (Email)
							{
								e->Email = NewStr(Email[0]);
							}

							return e;
						}
					}

					ldap_value_free(Type);
					ldap_value_free(Name);
					ldap_value_free(Email);
				}
				break;
			}
			default:
			{
				// error
				break;
			}
		}
	}

	return 0;
}
*/

/*
LDAP_F( int )
ldap_search_ext_s LDAP_P((
	LDAP			*ld,
	LDAP_CONST char	*base,
	int				scope,
	LDAP_CONST char	*filter,
	char			**attrs,
	int				attrsonly,
	LDAPControl		**serverctrls,
	LDAPControl		**clientctrls,
	struct timeval	*timeout,
	int				sizelimit,
	LDAPMessage		**res ));

*/

bool ILdap::Search(GDom *Results, const char *BaseDn, const char *UserFilter, const char *Search, bool Recursive)
{
	bool Status = false;
	if (Ldap)
	{
		// create a filter string
		char Filter[256] = "";
		const char *Attr[] = { "objectClass", "cn", "rfc822Mailbox", "mail", 0 };

		sprintf_s(Filter, sizeof(Filter), "(&(objectclass=person)(cn=%s*))", Search);

		// call a sync search
		LDAPMessage *Msg = 0;
		int Result = ldap_search_ext_s(	Ldap,
                                        BaseDn ? BaseDn : "",
                                        Recursive ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_ONELEVEL,
                                        Filter,
                                        (char**)Attr,
                                        false, // attrsonly
                                        NULL, // serverctrls
                                        NULL, // clientctrls
                                        NULL, // timeout
                                        0, // sizelimit
                                        &Msg);
            
		// bool SizeExceeded = Result == LDAP_SIZELIMIT_EXCEEDED;
		// bool TimeExceeded = Result == LDAP_TIMELIMIT_EXCEEDED;

		switch (Result)
		{
			case LDAP_SUCCESS:
			case LDAP_SIZELIMIT_EXCEEDED:
			case LDAP_TIMELIMIT_EXCEEDED:
			{
				// int Entries = ldap_count_entries(Ldap, Msg);
				// Ldap_Trace("ldap_search_s('%s'), ldap_count_entries=%i\n", Str, Entries);

				GArray<GVariant*> Args;
				for (LDAPMessage *m = ldap_first_entry(Ldap, Msg);
					m;
					m = ldap_next_entry(Ldap, m))
				{
					// objectClass: groupOfNames
					// objectClass: organizationalPerson
					
					char **Type = ldap_get_values(Ldap, m, "objectClass");
					char **Name = ldap_get_values(Ldap, m, "cn");
					char **Email = ldap_get_values(Ldap, m, "rfc822Mailbox");
					char **Email2 = ldap_get_values(Ldap, m, "mail");

					// Ldap_Trace("ldap_get_values, Type=%p, Name=%p, Email=%p, Email2=%p\n", Type, Name, Email, Email2);
					if (Type && Name && (Email || Email2) )
					{
						Ldap_Trace(	"real values, Type=%s, Name=%s, Email=%s, Email2=%s",
									*Type,
									*Name,
									Email ? *Email : 0,
									Email2 ? *Email2 : 0);

						ILdapEntry *e = new ILdapEntry;
						if (e)
						{
							e->Type = LDAP_ENTRY_PERSON;
							for (int t=0; Type[t]; t++)
							{
								if (stricmp(Type[t], "groupOfNames") == 0)
								{
									e->Type = LDAP_ENTRY_GROUP;
									break;
								}
							}

							if (Name)
							{
								e->Name = NewStr(Name[0]);
							}

							if (Email)
							{
								e->Email = NewStr(Email[0]);
							}
							else if (Email2)
							{
								e->Email = NewStr(Email2[0]);
							}

							Args.Add(new GVariant(e));
							Status = true;
						}
					}

					ldap_value_free(Type);
					ldap_value_free(Name);
					ldap_value_free(Email);
					ldap_value_free(Email2);
				}
				
				if (Args.Length())
				{
					Results->CallMethod(NULL, NULL, Args);
				}
				break;
			}
			default:
			{
				Ldap_Trace("%s:%i - error: ldap_search_s=%i\n", __FILE__, __LINE__, Result);
				break;
			}
		}
	}

	return Status;
}



