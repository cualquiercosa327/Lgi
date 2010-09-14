#include "Lgi.h"
#include "GDb.h"
#include "INet.h"
#include "GToken.h"
#include "../../src/common/Hash/sha1/sha1.h"

#define CLIENT_LONG_PASSWORD		1		/* new more secure passwords */
#define CLIENT_FOUND_ROWS			2		/* Found instead of affected rows */
#define CLIENT_LONG_FLAG			4		/* Get all column flags */
#define CLIENT_CONNECT_WITH_DB		8		/* One can specify db on connect */
#define CLIENT_NO_SCHEMA			16		/* Don't allow database.table.column */
#define CLIENT_COMPRESS				32		/* Can use compression protocol */
#define CLIENT_ODBC					64		/* Odbc client */
#define CLIENT_LOCAL_FILES			128		/* Can use LOAD DATA LOCAL */
#define CLIENT_IGNORE_SPACE			256		/* Ignore spaces before '(' */
#define CLIENT_PROTOCOL_41			512		/* New 4.1 protocol */
#define CLIENT_INTERACTIVE			1024	/* This is an interactive client */
#define CLIENT_SSL					2048	/* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE		4096	/* IGNORE sigpipes */
#define CLIENT_TRANSACTIONS			8192	/* Client knows about transactions */
#define CLIENT_RESERVED				16384	/* Old flag for 4.1 protocol  */
#define CLIENT_SECURE_CONNECTION	32768	/* New 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS		65536	/* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS		131072	/* Enable/disable multi-results */

#define COM_SLEEP					0x00	// (none, this is an internal thread state)
#define COM_QUIT					0x01	// mysql_close
#define COM_INIT_DB					0x02	// mysql_select_db 
#define COM_QUERY					0x03	// mysql_real_query
#define COM_FIELD_LIST				0x04	// mysql_list_fields
#define COM_CREATE_DB				0x05	// mysql_create_db (deprecated)
#define COM_DROP_DB					0x06	// mysql_drop_db (deprecated)
#define COM_REFRESH					0x07	// mysql_refresh
#define COM_SHUTDOWN				0x08	// mysql_shutdown
#define COM_STATISTICS				0x09	// mysql_stat
#define COM_PROCESS_INFO			0x0a	// mysql_list_processes
#define COM_CONNECT					0x0b	// (none, this is an internal thread state)
#define COM_PROCESS_KILL			0x0c	// mysql_kill
#define COM_DEBUG					0x0d	// mysql_dump_debug_info
#define COM_PING					0x0e	// mysql_ping
#define COM_TIME					0x0f	// (none, this is an internal thread state)
#define COM_DELAYED_INSERT			0x10	// (none, this is an internal thread state)
#define COM_CHANGE_USER				0x11	// mysql_change_user
#define COM_BINLOG_DUMP				0x12	// sent by the slave IO thread to request a binlog
#define COM_TABLE_DUMP				0x13	// LOAD TABLE ... FROM MASTER (deprecated)
#define COM_CONNECT_OUT				0x14	// (none, this is an internal thread state)
#define COM_REGISTER_SLAVE			0x15	// sent by the slave to register with the master (optional)
#define COM_STMT_PREPARE			0x16	// mysql_stmt_prepare
#define COM_STMT_EXECUTE			0x17	// mysql_stmt_execute
#define COM_STMT_SEND_LONG_DATA		0x18	// mysql_stmt_send_long_data
#define COM_STMT_CLOSE				0x19	// mysql_stmt_close
#define COM_STMT_RESET				0x1a	// mysql_stmt_reset
#define COM_SET_OPTION				0x1b	// mysql_set_server_option
#define COM_STMT_FETCH				0x1c	// mysql_stmt_fetch

#define FIELD_TYPE_DECIMAL			0x00
#define FIELD_TYPE_TINY				0x01
#define FIELD_TYPE_SHORT			0x02
#define FIELD_TYPE_LONG				0x03
#define FIELD_TYPE_FLOAT			0x04
#define FIELD_TYPE_DOUBLE			0x05
#define FIELD_TYPE_NULL				0x06
#define FIELD_TYPE_TIMESTAMP		0x07
#define FIELD_TYPE_LONGLONG			0x08
#define FIELD_TYPE_INT24			0x09
#define FIELD_TYPE_DATE				0x0a
#define FIELD_TYPE_TIME				0x0b
#define FIELD_TYPE_DATETIME			0x0c
#define FIELD_TYPE_YEAR				0x0d
#define FIELD_TYPE_NEWDATE			0x0e
#define FIELD_TYPE_VARCHAR			0x0f	// new in MySQL 5.0
#define FIELD_TYPE_BIT				0x10	// new in MySQL 5.0
#define FIELD_TYPE_NEWDECIMAL		0xf6	// new in MYSQL 5.0
#define FIELD_TYPE_ENUM				0xf7
#define FIELD_TYPE_SET				0xf8
#define FIELD_TYPE_TINY_BLOB		0xf9
#define FIELD_TYPE_MEDIUM_BLOB		0xfa
#define FIELD_TYPE_LONG_BLOB		0xfb
#define FIELD_TYPE_BLOB				0xfc
#define FIELD_TYPE_VAR_STRING		0xfd
#define FIELD_TYPE_STRING			0xfe
#define FIELD_TYPE_GEOMETRY			0xff

#define NOT_NULL_FLAG				0x0001
#define PRI_KEY_FLAG				0x0002
#define UNIQUE_KEY_FLAG				0x0004
#define MULTIPLE_KEY_FLAG			0x0008
#define BLOB_FLAG					0x0010
#define UNSIGNED_FLAG				0x0020
#define ZEROFILL_FLAG				0x0040
#define BINARY_FLAG					0x0080
#define ENUM_FLAG					0x0100
#define AUTO_INCREMENT_FLAG			0x0200
#define TIMESTAMP_FLAG				0x0400
#define SET_FLAG					0x0800

GDbField Null;

GArray<char> Hex(char *h)
{
	GArray<char> r;
	GToken t(h, " ");
	for (int i=0; i<t.Length(); i++)
	{
		r.Add(htoi(t[i]));
	}
	return r;
}

GArray<char> Sha1(GArray<char> &a, GArray<char> *b = 0)
{
	GArray<char> r;
	SHA1Context s;
	SHA1Reset(&s);
	SHA1Input(&s, (const unsigned char*) &a[0], a.Length());
	if (b)
		SHA1Input(&s, (const unsigned char*) &(*b)[0], b->Length());
	SHA1Result(&s);

	if (s.Computed &&
		r.Length(sizeof(s.Message_Digest)))
	{
		for (int i=0; i<CountOf(s.Message_Digest); i++)
		{
			s.Message_Digest[i] = LgiSwap32(s.Message_Digest[i]);
		}

		memcpy(&r[0], &s.Message_Digest, r.Length());
	}
	else
	{
		LgiAssert(0);
	}

	return r;
}

struct MysqlDirectContext
{
	class ErrorSocket : public GSocket
	{
	public:
		int Code;
		GAutoString Err;

		ErrorSocket()
		{
			Code = 0;
		}

		void OnError(int c, char *Desc)
		{
			Code = c;
			Err.Reset(NewStr(Desc));
		}
	};

	uint8 PacketIdx;
	char ReadBuf[512];
	int ReadPos, ReadUsed;
	GAutoPtr<ErrorSocket> Socket;

	MysqlDirectContext()
	{
		ReadPos = 0;
		ReadUsed = 0;
		PacketIdx = 0;
	}

	int Length()
	{
		return ReadUsed - ReadPos;
	}

	bool Read()
	{
		int Remain = sizeof(ReadBuf) - ReadUsed;
		if (Remain == 0)
		{
			// Shift data down...
			if (ReadPos > 0)
			{
				memmove(ReadBuf, ReadBuf + ReadPos, Length());
				ReadUsed -= ReadPos;
				ReadPos = 0;
			}
			else
			{
				LgiAssert(!"No buffer...");
				return false;
			}
		}

		if (!Socket)
		{
			LgiAssert(!"No socket...");
			return false;
		}

		int r = Socket->Read(ReadBuf + ReadUsed, sizeof(ReadBuf) - ReadUsed);
		if (r <= 0)
		{
			return false;
		}

		ReadUsed += r;
		return true;
	}
};

class MysqlPacket
{
	int Ptr;

public:
	GArray<char> Data;

	MysqlPacket()
	{
		Ptr = 0;
		Data.Length(4);
	}

	void Eof()
	{
		LgiAssert(!"End of packet");
	}

	void Empty()
	{
		Data.Length(4);
		Ptr = 0;
	}

	uint32 *GetHdr()
	{
		LgiAssert(Data.Length() >= 4);
		return (uint32*) &Data[0];
	}

	void HexDump(bool Read)
	{
		return;

		LgiTrace("%s: %i bytes\n", Read?"Read":"Write", Data.Length());
		for (int i=0; i<Data.Length(); i+=16)
		{
			char t[256];
			int n;
			int ch = sprintf(t, "%08.8x    ", i);
			for (n=0; n<16 && i+n<Data.Length(); n++)
				ch += sprintf(t+ch, "%02.2x ", (uint8)Data[i+n]);

			int Remain = Data.Length() - i;
			if (Remain > 0 && Remain < 16)
			{
				for (n=0; n<16-Remain; n++)
					ch += sprintf(t+ch, "   ");
			}

			for (n=0; n<16 && i+n<Data.Length(); n++)
				ch += sprintf(t+ch, "%c", Data[i+n]>=' '?Data[i+n]:'.');
			LgiTrace("%s\n", t);
		}
	}

	bool Read(MysqlDirectContext &c)
	{
		do
		{
			// First look in read buf and consume any bytes in there...
			if (c.Length() > 4)
			{
				// Decode header
				uint32 *Hdr = (uint32*) (&c.ReadBuf[c.ReadPos]);
				c.PacketIdx = (*Hdr >> 24) & 0xff;
				uint32 PktSize = *Hdr & 0xffffff;
				
				// Yes, so put the data into in the array
				int Total = 4 + PktSize;
				int Cpy = min(Total, c.Length());
				Data.Length(Cpy);
				memcpy(&Data[0], c.ReadBuf + c.ReadPos, Cpy);
				Ptr = 4;
				c.ReadPos += Cpy;

				while (Data.Length() < Total)
				{
					if (!c.Read())
						return false;

					int Cpy = min(Total - Data.Length(), c.Length());
					LgiAssert(Cpy > 0);
					Data.Add(c.ReadBuf + c.ReadPos, Cpy);
					c.ReadPos += Cpy;
				}

				if (Data.Length() >= Total)
					break;
			}
		}
		while (c.Read());

		HexDump(true);
		c.PacketIdx++;
		return Data.Length() > 0;
	}

	bool Write(MysqlDirectContext &c)
	{
		*GetHdr() = (Data.Length() - 4) | ((uint32)c.PacketIdx << 24);

		HexDump(false);
		for (int Done = 0; Done < Data.Length(); )
		{
			int w = c.Socket->Write(&Data[Done], Data.Length() - Done);
			if (w <= 0)
			{
				return false;
			}

			Done += w;
		}

		return true;
	}

	void Skip(int i)
	{
		Ptr += i;
	}

	void Add(int i)
	{
		Data.Length(Data.Length() + i);
	}

	int Read(GArray<char> &a, int Bytes)
	{
		int Remain = Data.Length() - Ptr;
		int Cpy = min(Remain, Bytes);
		a.Add(&Data[Ptr], Cpy);
		Ptr += Cpy;
		return Cpy;
	}

	GAutoString LenStr(uint8 Bytes = 0)
	{
		if (!Bytes)
			(*this) >> Bytes;

		GAutoString a;
		a.Reset(NewStr(&Data[Ptr], Bytes));
		Ptr += Bytes;
		return a;
	}

	int Write(GArray<char> &a)
	{
		Data.Add(&a[0], a.Length());
		return a.Length();
	}

	int Write(char *s)
	{
		if (!s)
			return 0;

		int Len = strlen(s);
		Data.Add(s, Len);
		return Len;
	}

	// Read operators
	MysqlPacket &operator >>(GAutoString &a)
	{
		int e = Ptr;
		while (e < Data.Length() && Data[e])
			e++;
		a.Reset(NewStr(&Data[Ptr], e-Ptr));
		Ptr = e + 1;
		return *this;
	}

	MysqlPacket &operator >>(uint8 &a)
	{
		if (Ptr < Data.Length())
			a = Data[Ptr++];
		else
			Eof();
		return *this;
	}

	MysqlPacket &operator >>(uint16 &a)
	{
		if (Ptr < Data.Length())
		{
			a = *((uint16*)&Data[Ptr]);
			Ptr += sizeof(a);
		}
		else Eof();
		return *this;
	}

	MysqlPacket &operator >>(uint32 &a)
	{
		if (Ptr < Data.Length())
		{
			a = *((uint32*)&Data[Ptr]);
			Ptr += sizeof(a);
		}
		else Eof();
		return *this;
	}

	MysqlPacket &operator >>(uint64 &a)
	{
		if (Ptr < Data.Length())
		{
			a = *((uint64*)&Data[Ptr]);
			Ptr += sizeof(a);
		}
		else Eof();
		return *this;
	}

	// Write operators
	MysqlPacket &operator <<(char *&a)
	{
		if (a)
		{
			int Len = strlen(a);
			Data.Add(a, Len);
		}
		
		Data.Add(0);
		return *this;
	}

	MysqlPacket &operator <<(GArray<char> &a)
	{
		Data.Add(a);
		return *this;
	}

	MysqlPacket &operator <<(uint8 &a)
	{
		Data.Add(a);
		return *this;
	}

	MysqlPacket &operator <<(uint16 &a)
	{
		int l = Data.Length();
		Data.Length(l + sizeof(a));
		*( (uint16*)&Data[l] ) = a;
		return *this;
	}

	MysqlPacket &operator <<(uint32 &a)
	{
		int l = Data.Length();
		Data.Length(l + sizeof(a));
		*( (uint32*)&Data[l] ) = a;
		return *this;
	}

	MysqlPacket &operator <<(uint64 &a)
	{
		int l = Data.Length();
		Data.Length(l + sizeof(a));
		*( (uint64*)&Data[l] ) = a;
		return *this;
	}
};

class MysqlDirectField : public GDbField
{
public:
	class MysqlDirectRs *Rs;
	int col;
	uint8 type, decimals;
	uint16 charset, flags;
	uint32 length;
	GAutoString cat;
	GAutoString db;
	GAutoString table;
	GAutoString org_tbl;
	GAutoString name;
	GAutoString org_name;
	GAutoString default_val;

	MysqlDirectField(MysqlDirectRs *rs);
	operator char*();

	char *Name();
	bool Name(char *Str);
	int Type();
	bool Type(int NewType);
	int Length();
	bool Length(int NewLength);
	char *Description();
	bool Description(char *NewDesc);
	bool Set(GVariant &v);
	bool Get(GVariant &v);
};

class MysqlDirectRs : public GDbRecordset
{
public:
	typedef GArray<GAutoString> Row;

	char *Sql;
	GArray<MysqlDirectField*> Field;
	GHashTbl<char*,MysqlDirectField*> Idx;
	int Cursor;
	GArray<Row> Rows;

	MysqlDirectRs(char *s);
	~MysqlDirectRs();

	char *Name();
	bool Eof() { return Cursor < 0 || Cursor >= Rows.Length(); }

	GDbField &operator [](int Index);
	GDbField &operator [](char *Name);	
	GDbField *InsertField(	char *Name,
							int Type,
							int Length = 0,
							int Index = -1);
	bool DeleteField(GDbField *Fld);
	int Fields();

	bool End();
	bool AddNew();
	bool Edit();
	bool Update();
	void Cancel();
	bool MoveFirst();
	bool MoveNext();
	bool MovePrev();
	bool MoveLast();
	int SeekRecord(int i);
	int RecordIndex();
	int Records();
	bool DeleteRecord();
};

MysqlDirectField::MysqlDirectField(MysqlDirectRs *rs)
{
	Rs = rs;
	col = Rs->Fields();
	Rs->Field.Add(this);
}

MysqlDirectField::operator char*()
{
	if (Rs->Eof())
		return 0;
	
	return Rs->Rows[Rs->Cursor][col];
}

char *MysqlDirectField::Name()
{
	return name;
}

bool MysqlDirectField::Name(char *Str)
{
	return false;
}

int MysqlDirectField::Type()
{
	return GV_STRING;
}

bool MysqlDirectField::Type(int NewType)
{
	return false;
}

int MysqlDirectField::Length()
{
	return 0;
}

bool MysqlDirectField::Length(int NewLength)
{
	return false;
}

char *MysqlDirectField::Description()
{
	return 0;
}

bool MysqlDirectField::Description(char *NewDesc)
{
	return false;
}

bool MysqlDirectField::Set(GVariant &v)
{
	return false;
}

bool MysqlDirectField::Get(GVariant &v)
{
	if (!Rs->End())
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
MysqlDirectRs::MysqlDirectRs(char *s)
{
	Sql = NewStr(s);
	Cursor = -1;

}

MysqlDirectRs::~MysqlDirectRs()
{
	Field.DeleteObjects();
	DeleteArray(Sql);
}

char *MysqlDirectRs::Name()
{
	return Sql;
}

GDbField &MysqlDirectRs::operator [](int Index)
{
	if (Index >= 0 && Index < Field.Length())
		return *Field[Index];

	return Null;
}

GDbField &MysqlDirectRs::operator [](char *Name)
{
	GDbField *f = Idx.Find(Name);
	return f ? *f : Null;
}

GDbField *MysqlDirectRs::InsertField(	char *Name,
						int Type,
						int Length,
						int Index)
{
	return 0;
}

bool MysqlDirectRs::DeleteField(GDbField *Fld)
{
	return 0;
}

int MysqlDirectRs::Fields()
{
	return Field.Length();
}

bool MysqlDirectRs::End()
{
	return Cursor < 0 OR Cursor >= Rows.Length();
}

bool MysqlDirectRs::AddNew()
{
	return 0;
}

bool MysqlDirectRs::Edit()
{
	return 0;
}

bool MysqlDirectRs::Update()
{
	return 0;
}

void MysqlDirectRs::Cancel()
{
}

bool MysqlDirectRs::MoveFirst()
{
	return SeekRecord(0);
}

bool MysqlDirectRs::MoveNext()
{
	return SeekRecord(Cursor + 1);
}

bool MysqlDirectRs::MovePrev()
{
	return SeekRecord(Cursor - 1);
}

bool MysqlDirectRs::MoveLast()
{
	return SeekRecord(Rows.Length() - 1);
}

int MysqlDirectRs::SeekRecord(int i)
{
	Cursor = i;

	if (!End())
	{
		return true;
	}

	return false;
}

int MysqlDirectRs::RecordIndex()
{
	return Cursor;
}

int MysqlDirectRs::Records()
{
	return Rows.Length();
}

bool MysqlDirectRs::DeleteRecord()
{
	return Rows.DeleteAt(Cursor);
}

//////////////////////////////////////////////////////////////////////
class MysqlDirectDb : public GDb, public MysqlDirectContext
{
	char Err[1024];
	
	uint8 ProtocolVer;
	GAutoString ServerVer;
	uint32 ThreadId;
	uint16 ServerCap;
	uint8 ServerLang;
	uint16 ServerStatus;
	GArray<char> ScrambleBuf;
	bool LoggedIn;
	GArray<GAutoString> Dbs;

public:
	enum MysqlResponse
	{
		MyOk = 0x0,
		MyEof = 0xfe,
		MyErr = 0xff,
	};
	#define ValidDataResponse(c) ((c) >= 1 && (c) <= 250)

	MysqlDirectDb(char *s)
	{
		LoggedIn = 0;
		if (s)
			Connect(s);
	}

	~MysqlDirectDb()
	{
	}

	bool GetVariant(char *n, GVariant &val, char *arr)
	{
		if (!stricmp(n, Dom_IsConnected))
		{
			val = Socket && Socket->IsOpen();
		}
		else return false;

		return true;
	}

	bool Connect(char *Init)
	{
		if (!Init)
			return false;

		GUri u(Init);
		if (u.User &&
			u.Pass &&
			u.Host &&
			Socket.Reset(new ErrorSocket))
		{
			if (!u.Port)
				u.Port = 3306;
			
			if (Socket->Open(u.Host, u.Port))
			{
				// Read signon packet...
				GAutoPtr<MysqlPacket> p(new MysqlPacket);
				if (!p->Read(*this))
				{
					sprintf(Err, "Failed read signon packet.");
					Socket.Reset();
				}
				else
				{
					uint8 Filler;
					*p >> ProtocolVer >> ServerVer >> ThreadId;
					p->Read(ScrambleBuf, 8);
					*p >> Filler >> ServerCap >> ServerLang >> ServerStatus;
					p->Skip(13);
					p->Read(ScrambleBuf, 13);
					ScrambleBuf.Length(20);

					// Write authentication packet...
					p->Empty();
					uint32 ClientFlags = CLIENT_LONG_PASSWORD |
										CLIENT_PROTOCOL_41 |
										CLIENT_SECURE_CONNECTION;
					uint32 MaxPacketSize = 16 << 20;
					uint8 CharsetId = ServerLang;

					*p << ClientFlags << MaxPacketSize << CharsetId;
					p->Add(23);
					*p << u.User;

					GArray<char> Pass;
					Pass.Add(u.Pass, strlen(u.Pass));
					GArray<char> stage1_hash = Sha1(Pass);
					GArray<char> stage2_hash = Sha1(stage1_hash);
					GArray<char> crypt = Sha1(ScrambleBuf, &stage2_hash);
					GArray<char> token;
					LgiAssert(stage1_hash.Length() == crypt.Length());
					for (int i=0; i<stage1_hash.Length(); i++)
					{
						token[i] = stage1_hash[i] ^ crypt[i];
					}

					uint8 PassLen = token.Length();
					*p << PassLen << token;

					if (!p->Write(*this))
					{
						sprintf(Err, "Failed to write auth packet.");
						Socket.Reset();
					}
					else
					{
						// Get response
						p->Empty();
						if (Response(p) == MyOk)
						{
							LoggedIn = true;

							if (u.Path && u.Path[0] == '/')
							{
								InitDb(u.Path + 1);
							}
							else
							{
								// Read in a list of databases
								GAutoPtr<GDbRecordset> r(Open("show databases;"));
								if (r)
								{
									for (bool b = r->MoveFirst(); b; b = r->MoveNext())
									{
										GDbField &asd = (*r)[0];
										char *db = asd;
										Dbs.New().Reset(NewStr(db));
									}
								}
							}
						}
					}
				}
			}
			else
			{
				sprintf(Err,
						"Failed to connect to '%s:%i' (%i - %s)",
						u.Host,
						u.Port,
						Socket->Code,
						Socket->Err.Get());
				Socket.Reset();
			}
		}
		else sprintf(Err, "Missing parameters.");

		return Socket && Socket->IsOpen();
	}

	bool Disconnect()
	{
		Socket.Reset();
		return true;
	}

	bool IsOk()
	{
		return Socket && Socket->IsOpen();
	}

	uint8 Response(MysqlPacket *p)
	{
		if (!p)
			return MyErr;

		p->Empty();
		if (!p->Read(*this))
		{
			sprintf(Err, "Failed to read response packet.");
			return MyErr;
		}

		uint8 r;
		*p >> r;
		if (r == 0xff)
		{
			uint16 Code;
			GAutoString Desc;
			*p >> Code >> Desc;
			sprintf(Err, "Login failed: %i - %s", Code, Desc.Get());
			Socket.Reset();
		}
		return r;
	}

	bool InitDb(char *Name)
	{
		GAutoPtr<MysqlPacket> p(new MysqlPacket);
		uint8 Cmd = COM_INIT_DB;
		*p << Cmd;
		PacketIdx = 0;
		p->Write(Name);
		if (!p->Write(*this))
			return false;
		p->Empty();
		return Response(p) == MyOk;
	}

	GDbRecordset *Open(char *Name)
	{
		if (!IsOk())
			return 0;

		GAutoPtr<MysqlPacket> p(new MysqlPacket);
		
		uint8 Cmd = COM_QUERY, Cols;
		*p << Cmd;
		p->Write(Name);
		PacketIdx = 0;
		if (!p->Write(*this))
			return false;

		GAutoPtr<MysqlDirectRs> Rs(new MysqlDirectRs(Name));
		Cols = Response(p);
		if (!ValidDataResponse(Cols))
			return 0;

		uint8 r = Response(p), filler;
		while (ValidDataResponse(r))
		{
			MysqlDirectField *fld = new MysqlDirectField(Rs);

			fld->cat = p->LenStr(r);
			fld->db = p->LenStr();
			fld->table = p->LenStr();
			fld->org_tbl = p->LenStr();
			fld->name = p->LenStr();
			fld->org_name = p->LenStr();
			*p >> filler >> fld->charset >> fld->length >> fld->type >> fld->flags >> fld->decimals >> filler;
			fld->default_val = p->LenStr();
			Rs->Idx.Add(fld->name, fld);

			r = Response(p);
		}

		if (r != MyEof)
			return 0;

		r = Response(p);
		while (r >= 1 && r <= 251)
		{
			MysqlDirectRs::Row *CurRow = &Rs->Rows.New();
			for (int c=0; c<Cols; c++)
			{
				GAutoString &field = CurRow->New();
				if (c)
					*p >> r;

				if (r == 0)
					break;
				if (r != 251)
					field = p->LenStr(r);
			}

			r = Response(p);
		}

		if (r != MyEof)
			return 0;

		return Rs.Release();
	}

	bool Execute(char *Name)
	{
		return false;
	}

	const char *GetErrorMsg()
	{
		return Err;
	}

	GDbRecordset *TableAt(int i)
	{
		return 0;
	}
};

GDb *OpenMysqlDirectDatabase(char *s)
{
	return new MysqlDirectDb(s);
}

