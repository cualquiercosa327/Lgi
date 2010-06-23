#include <stdio.h>

#include "Lgi.h"
#include "ILdapServer.h"

/////////////////////////////////////////////////////////////////////////////////
ILdapServer::ILdapServer(GView *p, ILdapServerDb *db)
{
	Parent = p;
	Db = db;

	#ifdef _DEBUG
	// Log.Open("d:\\temp\\log.txt", O_WRITE);
	// Log.SetSize(0);
	#endif
	
	Run();
}

ILdapServer::~ILdapServer()
{
	Socket.Close();
	Listen.Close();
	while (!IsExited())
	{
		LgiSleep(100);
	}
}

char *ILdapServer::GetContactUid(GDom *c)
{
	static char Buf[256];

	Buf[0] = 0;
	GVariant First, Last;
	if (c AND
		Db AND
		c->GetValue(Db->MapField(LdapFirstName), First) AND
		c->GetValue(Db->MapField(LdapLastName), Last))
	{
		char *In=Buf, *Out=Buf;
		sprintf(Buf, "%s%s", First.Str(), Last.Str());

		// Clean out some invalid chars
		for (; *In; In++)
		{
			if (!strchr(" +", *In))
			{
				*Out++ = *In;
			}
		}

		*Out++ = 0;
	}

	return Buf;
}

void ILdapServer::WriteField(EncBer *Parent, char *Name, char *Value, List<char> &Return)
{
	if (Parent AND
		Name AND
		Value)
	{
		bool Ret = true;
		if (Return.Length())
		{
			Ret = false;
			for (char *c=Return.First(); c; c=Return.Next())
			{
				if (stricmp(c, Name) == 0)
				{
					Ret = true;
				}
			}
		}

		if (Ret)
		{
			EncBer *Lst = Parent->Sequence();
			if (Lst)
			{
				Lst->Str(Name);
				EncBer *Val = Lst->Set();
				if (Val)
				{
					Val->Str(Value);
					DeleteObj(Val);
				}
				DeleteObj(Lst);
			}
		}
	}
}

void ILdapServer::ProcessFilter(DecBer *Filter, int FilterType, List<GDom> *Input, List<GDom> &Results)
{
	switch (FilterType)
	{
		case 0:
		{
			int Type;
			DecBer *Sub;
			while (Sub = Filter->Context(Type))
			{
				List<GDom> Temp;
				ProcessFilter(Sub, Type, Input, Results);
			}
		}
		case 4:
		{
			int ArgType;
			char *Type;
			DecBer *Args, *Arg;

			if (Filter->Str(Type) AND
				(Args = Filter->Sequence()) AND
				(Arg = Args->Context(ArgType)))
			{
				//char *s = NewStr((char*)Arg->Ptr->Uchar(), Arg->Ptr->GetLen());
				char *s;
				if (Arg->Str(s))
				{
					for (GDom *c=Input->First(); c; c=Input->Next())
					{
						if (stricmp(Type, "cn") == 0)
						{
							GVariant CName;
							if (Db AND
								c->GetValue(Db->MapField(LdapFirstName), CName) AND
								CName.Str() AND
								strnicmp(CName.Str(), s, strlen(s)) == 0)
							{
								Results.Insert(c);
							}
						}
					}
				}
			}								
			break;
		}
		case 7:
		{
			char *Present = 0; // NewStr((char*)Filter->Ptr->Uchar(), Filter->Ptr->GetLen());
			if (Filter->Str(Present))
			{
				if (stricmp(Present, "objectClass") == 0 OR
					stricmp(Present, "cn") == 0 OR
					stricmp(Present, "mail") == 0)
				{
					for (GDom *c=Input->First(); c; c=Input->Next())
					{
						Results.Insert(c);
					}
				}
			}
			break;
		}
	}
}

void ILdapServer::OnMessage(DecBer &Buf)
{
	EncBer Encode;
	DecBer *App, *Msg;
	int MsgId, Request;

	if ((Msg = Buf.Sequence()) AND
		Msg->Int(MsgId) AND
		(App = Msg->Application(Request)))
	{
		switch (Request)
		{
			case 0: // Bind
			{
				int ProtocolVer, AuthType;
				char *Name;
				DecBer *Auth;

				if (App->Int(ProtocolVer) AND
					App->Str(Name) AND
					(Auth = App->Context(AuthType)))
				{
					if (ProtocolVer == 2 OR
						ProtocolVer == 3)
					{
						EncBer *e = Encode.Sequence();
						if (e)
						{
							e->Int(MsgId);

							EncBer *a = e->Application(1);
							if (a)
							{
								a->Enum(0);		// Sucess
								a->Str();		// Matched
								a->Str();		// Error
								DeleteObj(a);
							}

							DeleteObj(e);
						}

						Send(Encode);
					}
					else
					{
						Socket.Close();
					}
				}
				break;
			}
			case 2:
			{
				Socket.Close();
				break;
			}
			case 3: // Search Request
			{
				char *BaseObj;
				int Scope, DerefAlias, SizeLimit, TimeLimit, AttrOnly, FilterType;
				DecBer *Filter, *Attr;

				if (App->Str(BaseObj) AND
					App->Enum(Scope) AND
					App->Enum(DerefAlias) AND
					App->Int(SizeLimit) AND
					App->Int(TimeLimit) AND
					App->Bool(AttrOnly) AND
					(Filter = App->Context(FilterType)) AND
					(Attr = App->Sequence()))
				{
					List<GDom> Results, BaseInput;
					List<GDom> *Input = &Contacts;
					List<char> ReturnAttrs;
					char *Attribute;
					while (Attr->Str(Attribute))
					{
						ReturnAttrs.Insert(NewStr(Attribute));
					}

					if (BaseObj)
					{
						Input = &BaseInput;
						
						if (Scope == 0)
						{
							if (strnicmp(BaseObj, "uid=", 4) == 0)
							{
								for (GDom *c=Contacts.First(); c; c=Contacts.Next())
								{
									if (stricmp(GetContactUid(c), BaseObj+4) == 0)
									{
										BaseInput.Insert(c);
									}
								}
							}
						}
					}

					ProcessFilter(Filter, FilterType, Input, Results);

					for (GDom *c=Results.First(); c; c=Results.Next())
					{
						GVariant First, Last, Email;
						if (Db AND
							c->GetValue(Db->MapField(LdapFirstName), First) AND
							c->GetValue(Db->MapField(LdapLastName), Last) AND
							c->GetValue(Db->MapField(LdapEmail), Email))
						{
							EncBer *e = Encode.Sequence();
							if (e)
							{
								e->Int(MsgId);

								EncBer *a = e->Application(4);
								if (a)
								{
									// Create UID out of first + last
									char Buf[256], *In=Buf, *Out=Buf;
									sprintf(Buf, "uid=%s%s", First.Str(), Last.Str());
									
									// Clean out some invalid chars
									for (; *In; In++)
									{
										if (!strchr(" +", *In))
										{
											*Out++ = *In;
										}
									}
									*Out++ = 0;
									a->Str(Buf);	// Object Name

									// Encode the list of attributes
									EncBer *Body = a->Sequence();
									if (Body)
									{
										sprintf(Buf, "%s %s", First.Str(), Last.Str());
										WriteField(Body, "cn", Buf, ReturnAttrs);
										WriteField(Body, "mail", Email.Str(), ReturnAttrs);
										WriteField(Body, "objectClass", "organizationalPerson", ReturnAttrs);
										DeleteObj(Body);
									}

									DeleteObj(a);
								}

								DeleteObj(e);
							}
						}
					}
				}

				// Encode the terminator
				EncBer *e = Encode.Sequence();
				if (e)
				{
					e->Int(MsgId);

					EncBer *a = e->Application(5);
					if (a)
					{
						a->Enum(0);
						a->Str();
						a->Str();
						DeleteObj(a);
					}

					DeleteObj(e);
				}

				Send(Encode);

				DeleteObj(Filter);
				DeleteObj(Attr);
				break;
			}
			default:
			{
				break;
			}
		}

		DeleteObj(App);
	}

	DeleteObj(Msg);
}

void ILdapServer::Send(EncBer &b)
{
	int Len = b.Buf.GetSize();
	uchar *Buf = new uchar[Len];
	if (Buf)
	{
		b.Buf.Peek(Buf, Len);
		BerDump("Server -> Client", b.Buf);
		Socket.Write((char*)Buf, Len, 0);
		b.Buf.Empty();

		DeleteArray(Buf);
	}
}

bool PipeGetData(int This, uchar &c)
{
	GBytePipe *p = (GBytePipe*)This;
	return p->Read(&c, 1);
}

void ILdapServer::Server()
{
	if (Listen.Listen(LDAP_PORT))
	{
		while (Listen.Accept(&Socket))
		{
			// Get List of contacts
			Contacts.Empty();
			if (Db)
			{
				Db->GetData(Contacts);
			}

			// Do server loop
			while (Socket.IsOpen())
			{
				GBytePipe Buf;

				while (Socket.IsReadable())
				{
					char Data[256];
					int Len = Socket.Read(Data, sizeof(Data), 0);
					if (Len > 0)
					{
						Buf.Write((uchar*)Data, Len);
					}
				}

				if (Buf.GetSize() > 0)
				{
					BerDump("Client -> Server", Buf);

					DecBer Msg(PipeGetData, (int)&Buf);
					OnMessage(Msg);
				}

				LgiSleep(100);
			}
		}
	}
	else if (Parent)
	{
		char s[256];
		sprintf(s, "Couldn't bind to port %i", LDAP_PORT);
		Parent->PostEvent(WM_ILDAP_ERROR_MSG, (int)NewStr(s));
	}

}

int ILdapServer::TokenDump(uchar *Data, int Len, int Indent)
{
	char Tabs[256];
	memset(Tabs, '\t', Indent);
	Tabs[Indent] = 0;

	if (Len < 2) return Len;

	#if 0
	Log.Print("%s", Tabs);
	for (int i=0; i<Len; i++)
	{
		Log.Print("%02.2X ", Data[i]);
	}
	Log.Print("\n");
	#endif

	int Size = Data[1];
	uchar *Next = Data + 2;
	if (Data[1] & 0x80)
	{
		int Bytes = Data[1] & 0x7f;
		uchar *p = (uchar*)&Size;
		for (int n=0; n<Bytes; n++)
		{
			*p++ = Data[1+Bytes-n];
		}
		Next += Bytes;
	}

	switch (Data[0] >> 6)
	{
		case 0: // Universal
		{
			int Type = Data[0] & 0x1f;
			switch (Type)
			{
				case 1:		// Boolean
				case 2:		// Integer
				case 10:	// Enum
				{
					int Int = 0;
					uchar *Ptr = (uchar*)&Int;
					char *TypeInfo = "Int";
					if (Type == 1)
					{
						TypeInfo = "Bool";
					}
					else if (Type == 10)
					{
						TypeInfo = "Enum";
					}

					for (int i=0; i<sizeof(Int) && i<Size; i++)
					{
						*Ptr++ = Next[i];
					}

					Log.Print("%s%s: %i\n", Tabs, TypeInfo, Int);
					break;
				}
				case 3: // Bit String
				{
					break;
				}
				case 4: // Octet String
				{
					char *Str = (Size > 0) ? new char[Size+1] : 0;
					if (Str AND Size > 0)
					{
						memcpy(Str, Next, Size);
						Str[Size] = 0;
					}
					
					if (Str)
					{
						Log.Print("%sStr: '%s'\n", Tabs, Str);
					}
					else
					{
						Log.Print("%sStr: null\n", Tabs, Str);
					}

					DeleteArray(Str);
					break;
				}
				case 5:
				{
					// NULL??
					break;
				}
				case 16: // Sequence
				case 17: // Set
				{
					char *Name = ((Data[0] & 0x1f) == 16) ? (char*)"Sequence" : (char*)"Set";
					bool Constructed = Data[0] & 0x20 ? true : false;
					Log.Print("%s%s (Constr=%i, Size=%i)\n%s{\n", Tabs, Name, Constructed, Size, Tabs);
					if (Constructed)
					{
						for (int Done=0; Done<Size;)
						{
							Done += TokenDump(Next+Done, Size-Done, Indent+1);
						}
					}
					Log.Print("%s}\n", Tabs);
					break;
				}
				case 6: // Object Ident.
				case 7: // Object descriptor
				case 8: // External type
				case 9: // Real
				default:
				{
					break;
				}
			}

			break;
		}
		case 1: // Application
		case 2: // Context
		{
			char *Name = ((Data[0] >> 6) == 1) ? (char*)"Application" : (char*)"Context";
			bool Constructed = Data[0] & 0x20 ? true : false;
			Log.Print("%s%s[%i] (Constr=%i, Size=%i)\n%s{\n",
						Tabs,
						Name,
						Data[0] & 0x1f,
						Constructed,
						Size,
						Tabs);

			if (Constructed)
			{
				for (int Done=0; Done<Size;)
				{
					Done += TokenDump(Next+Done, Size-Done, Indent+1);
				}
			}

			Log.Print("%s}\n", Tabs);
			break;
		}
		case 3:
		{
			break;
		}
	}

	return ((int)Next - (int)Data) + Size;
}

void ILdapServer::BerDump(char *Title, GBytePipe &Data)
{
	int Len = Data.GetSize();
	if (Len > 0)
	{
		Log.Print("%s (%i bytes)\r\n", Title, Len);
		
		uchar *Buf = new uchar[Len];
		if (Buf)
		{
			Data.Peek(Buf, Len);
			
			int Done = 0;
			while (Done < Len)
			{
				Done += TokenDump(Buf+Done, Len-Done, 0);
			}

			DeleteArray(Buf);
		}

		Log.Print("\r\n");
	}
}

int ILdapServer::Main()
{
	Server();
	
	if (Parent)
	{
		Parent->PostEvent(WM_ILDAP_SERVER_DONE);
	}
	
	return 0;
}
