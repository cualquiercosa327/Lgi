#include <stdio.h>
#include "Lgi.h"
#include "IDns.h"
#include "GToken.h"
#include "INet.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif

bool GetDnsServers(GArray<char*> &Servers)
{
	bool Status = false;

	#if defined(WIN32)
	
	GRegKey k("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces");
	List<char> Sub;
	k.GetKeyNames(Sub);
	if (Sub.First())
	{
		for (char *s = Sub.First(); s; s = Sub.Next())
		{
			GRegKey Interface("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\%s", s);
			
			char *NameServers = Interface.GetStr("DhcpNameServer");

			if (!ValidStr(NameServers))
				NameServers = Interface.GetStr("NameServer");

			if (NameServers)
			{
				GToken Ns(NameServers, " ");
				for (int i=0; i<Ns.Length(); i++)
				{
					bool Has = false;
					for (int n=0; n<Servers.Length(); n++)
					{
						if (!stricmp(Ns[i], Servers[n]))
						{
							Has = true;
							break;
						}
					}
					if (!Has)
					{
						Servers.Add(NewStr(Ns[i]));
						Status = true;
					}
				}
			}
		}
		Sub.DeleteArrays();
	}
	
	#else

	char *s = ReadTextFile("/etc/resolv.conf");
	if (s)
	{
		GToken t(s, "\r\n");
		for (int i=0; i<t.Length(); i++)
		{
			GToken s(t[i], " ");
			if (s.Length() == 2 && !stricmp(s[0], "nameserver"))
			{
				Servers.Add(NewStr(s[1]));
				Status = true;
			}
		}
	}
	
	#endif

	return Status;
}

uchar *DnsMethods::ReadLabel(void *Header, uchar *s, char *&Label)
{
	GStringPipe p;
	while (*s)
	{
		if (p.GetSize()) p.Push(".");
		if ((*s & 0xc0) == 0xc0)
		{
			int Offset = ((s[0] & 0x3f) << 8) | s[1];
			uchar *Ptr = (uchar*)Header + Offset;
			
			char *l = 0;
			ReadLabel(Header, Ptr, l);
			if (l)
			{
				p.Push(l);
				DeleteArray(l);
			}
			
			s += 2;
			Label = p.NewStr();
			return s;
		}
		else
		{
			p.Push((char*)s + 1, *s);
			s += *s + 1;
		}
	}
	s++;
	Label = p.NewStr();
	return s;
}

uchar *DnsMethods::WriteLabel(uchar *s, char *Label)
{
	GToken n(Label, ".");
	for (int i=0; i<n.Length(); i++)
	{
		*s = min(strlen(n[i]), 255);
		memcpy(s + 1, n[i], *s);
		s += *s + 1;
	}
	*s++ = 0;
	return s;
}

// Functions
bool IDnsResolve(GArray<char*> &Results, char *Name, int Type, int Class)
{
	LgiAssert(sizeof(DnsHeader) == 12);

	if (!Name)
		return false;

	int NameLen = strlen(Name);

	GArray<char*> Ns;
	if (!GetDnsServers(Ns))
		return false;

	for (int n = 0; Results.Length() == 0 && n < Ns.Length(); n++)
	{
		#if 0
		// TCP
		GSocketI *s = new GSocket;
		if (s)
		{
			if (s->Open(Ns[n], DNS_PORT))
			{
				DnsMessage m;
				ZeroObj(m);
				m.Header.Id = htons(DNS_MY_ID);
				m.Header.QueryType = DNS_Q_QUERY;
				m.Header.QueryCount = htons(1);
				m.Header.RecursionDesired = 1;

				uint16 *p = (uint16*) m.WriteLabel(m.Data, Name);
				*p++ = htons(Type);
				*p++ = htons(Class);

				int Length = (int)p - (int)&m;
				int Bytes;
				m.Length = htons(Length);
				if ((Bytes = s->Write(&m, Length + 2)) > 0)
				{
					bool Readable = s->IsReadable(DNS_TIMEOUT);

					if (Readable && (Bytes = s->Read(&m, sizeof(m))) > 0)
					{
						m.Length = ntohs(m.Length);
						m.Header.Id = ntohs(m.Header.Id);
						m.Header.QueryCount = ntohs(m.Header.QueryCount);
						m.Header.ResourceCount = ntohs(m.Header.ResourceCount);
						m.Header.NameServerCount = ntohs(m.Header.NameServerCount);
						m.Header.AdditionalCount = ntohs(m.Header.AdditionalCount);
						
						uchar *d = m.Data;
						
						// Parse queries
						for (int q=0; q<m.Header.QueryCount; q++)
						{
							char *Label = 0;
							p = (uint16*) m.ReadLabel(d, Label);
							DeleteArray(Label);
							p += 2;
							d = (uchar*) p;
						}

						// Parse resource records
						for (int r=0; r<m.Header.ResourceCount; r++)
						{
							char *Name = 0;
							p = (uint16*) m.ReadLabel(d, Name);
							DeleteArray(Name);
							uint16 Type = ntohs(*p); p++;
							uint16 Class = ntohs(*p); p++;
							uint32 *l = (uint32*)p;
							uint32 Ttl = ntohl(*l); l++;
							p = (uint16*) l;
							uint16 DataLen = ntohs(*p); p++;
							
							if (Type == DNS_TYPE_A AND
								Class == DNS_CLASS_IN AND
								DataLen == 4)
							{
								char Ip[32];
								uchar *j = (uchar*)p;
								sprintf(Ip, "%u.%u.%u.%u", j[0], j[1], j[2], j[3]);
								Results.Add(NewStr(Ip));
							}
							else if (Type == DNS_TYPE_MX AND
								Class == DNS_CLASS_IN)
							{
								char *Mx = 0;
								m.ReadLabel((uchar*)(p+1), Mx);
								if (Mx)
									Results.Add(Mx);
							}

							d = (uchar*)p + DataLen;
						}
					}
				}
			}

			DeleteObj(s);
		}

		#else
		// UDP

		DnsUdpMessage m;
		ZeroObj(m);
		m.Header.Id = htons(DNS_MY_ID);
		m.Header.QueryType = DNS_Q_QUERY;
		m.Header.QueryCount = htons(1);
		m.Header.RecursionDesired = 1;

		uint16 *p = (uint16*) m.WriteLabel(m.Data, Name);
		*p++ = htons(Type);
		*p++ = htons(Class);

		NativeInt Length = (uchar*)p - (uchar*)&m;
		int Bytes;

		GSocket s;
		s.SetUdp(true);

		Bytes = s.WriteUdp(&m, Length + 2, 0, inet_addr(Ns[n]), DNS_PORT);
		if (Bytes > 0)
		{
			bool Readable = s.IsReadable(DNS_TIMEOUT);
			if (Readable)
			{
				uint32 Ip;
				uint16 Port = DNS_PORT;
				Bytes = s.ReadUdp(&m, sizeof(m), 0, &Ip, &Port);
				if (Bytes > 0)
				{
					m.Header.Id = ntohs(m.Header.Id);
					m.Header.QueryCount = ntohs(m.Header.QueryCount);
					m.Header.ResourceCount = ntohs(m.Header.ResourceCount);
					m.Header.NameServerCount = ntohs(m.Header.NameServerCount);
					m.Header.AdditionalCount = ntohs(m.Header.AdditionalCount);
					
					uchar *d = m.Data;
					
					// Parse queries
					for (int q=0; q<m.Header.QueryCount; q++)
					{
						char *Label = 0;
						p = (uint16*) m.ReadLabel(&m.Header, d, Label);
						DeleteArray(Label);
						p += 2;
						d = (uchar*) p;
					}

					// Parse resource records
					for (int r=0; r<m.Header.ResourceCount; r++)
					{
						char *Name = 0;
						p = (uint16*) m.ReadLabel(&m.Header, d, Name);
						DeleteArray(Name);
						uint16 Type = ntohs(*p); p++;
						uint16 Class = ntohs(*p); p++;
						uint32 *l = (uint32*)p;
						uint32 Ttl = ntohl(*l); l++;
						p = (uint16*) l;
						uint16 DataLen = ntohs(*p); p++;
						
						if (Type == DNS_TYPE_A AND
							Class == DNS_CLASS_IN AND
							DataLen == 4)
						{
							char Ip[32];
							uchar *j = (uchar*)p;
							sprintf(Ip, "%u.%u.%u.%u", j[0], j[1], j[2], j[3]);
							Results.Add(NewStr(Ip));
						}
						else if (Type == DNS_TYPE_MX AND
							Class == DNS_CLASS_IN)
						{
							char *Mx = 0;
							m.ReadLabel(&m.Header, (uchar*)(p+1), Mx);
							if (Mx)
								Results.Add(Mx);
						}

						d = (uchar*)p + DataLen;
					}
				}
			}
		}

		#endif
	}

	Ns.DeleteArrays();

	return Results.Length() > 0;
}

