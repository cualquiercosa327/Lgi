#ifndef _SERIALIZE_H_
#define _SERIALIZE_H_

enum LSTypes
{
	LInt,
	LString,
	LFloat,
	LObject,
	LBinary,
};

class LSerialize
{
	#ifdef WIN32
	#pragma pack(push, before_pack)
	#pragma pack(1)
	#endif
	struct Str
	{
		uint32 Length;
		union {
			char u[1];
			char16 w[1];
		};
	};
	struct Field
	{
		uint8 Type;
		uint8 Size;
		uint16 Id;
		union {
			char bytes[1];
			Str s;
			uint8 u8;
			uint16 u16;
			uint32 u32;
			uint64 u64;
			double f;
		};

		bool IsValid()
		{
			switch (Type)
			{
				case LInt:
					switch (Size)
					{
						case 1:
						case 2:
						case 4:
						case 8:
							return true;
					}
					return false;
				case LString:
					if (Size == 1)
						return s.u[s.Length] == 0;
					else if (Size == sizeof(char16))
						return s.w[s.Length] == 0;
					return false;
				case LBinary:
					if (Size != 1)
						return false;
					return true;
				case LFloat:
					return Size == sizeof(f);
				case LObject:
					return true;
			}

			return false;
		}

		uint32 Sizeof()
		{
			switch (Type)
			{
				case LInt:
				case LFloat:
					return 4 + Size;
				case LString:
					return 8 + (Size * (s.Length + 1));
				case LObject:
					return 4 + u32;
				case LBinary:
					return 8 + s.Length;
			}

			return sizeof(*this);
		}
	}
	#ifndef WIN32
	__attribute__((packed))
	#endif
		;
	#ifdef WIN32
	#pragma pack(pop, before_pack)
	#endif

	uint16 ObjectId;
	bool ToStream;
	GStreamI *Stream;
	GArray<char> FieldMem;
	GHashTbl<int, ssize_t> Fields;
	uint32 Bytes;

protected:
	Field *GetField(int Id)
	{
		ssize_t o = Fields.Find(Id);
		if (o >= 0)
			return (Field*)FieldMem.AddressOf(o);
		return NULL;
	}

	template<typename T>
	bool Int(uint16 Id, T &i)
	{
		if (ToStream)
		{
			Field f;
			f.Type = LInt;
			f.Id = Id;
			f.Size = sizeof(i);
			int Bytes = 4 + f.Size;
			memcpy(f.bytes, &i, sizeof(i));

			if (Stream->Write(&f, Bytes) != Bytes)
				return false;
		}
		else
		{
			Field *f = GetField(Id);
            if (!f)
                return false;

			switch (f->Size)
			{
				case 1: i = (T)f->u8; break;
				case 2: i = (T)f->u16; break;
				case 4: i = (T)f->u32; break;
				case 8: i = (T)f->u64; break;
				default: return false;
			}
		}

		return true;
	}

	bool String(uint16 Id, GString &s)
	{
		if (ToStream)
		{
			Field f;
			f.Type = LString;
			f.Size = 1;
			f.Id = Id;
			f.s.Length = (uint32)s.Length();
			ssize_t bytes = (char*)&f.s.u - (char*)&f;
			if (Stream->Write(&f, bytes) != bytes)
				return false;
			if (Stream->Write(s.Get(), s.Length()) != s.Length())
				return false;
		}
		else
		{
			Field *f = GetField(Id);
			if (!f || f->Type != LString)
				return false;
			return s.Set(f->s.u, f->s.Length);
		}

		return false;
	}

	Field *Alloc(int Id, size_t Sz)
	{
		Field *f = GetField(Id);
		if (f && f->Sizeof() >= Sz)
			return f;

		size_t Off = FieldMem.Length();
		FieldMem.Length(Off + Sz);
		f = (Field*) FieldMem.AddressOf(Off);
		f->Id = Id;
		Fields.Add(Id, Off);
		return f;		
	}

public:
	LSerialize(uint16 ObjId = 0) : Fields(64, false, -1, -1)
	{
		ObjectId = ObjId;
		ToStream = true;
		Stream = NULL;
	}

	virtual ~LSerialize()
	{
	}

	uint16 GetObjectId()
	{
		return ObjectId;
	}

	size_t GetSize()
	{
		size_t Sz = 0;
		
		for (ssize_t o = Fields.First(); o >= 0; o = Fields.Next())
		{
			Field *i = (Field*)FieldMem.AddressOf(o);
			Sz += i->Sizeof();
		}

		return Sz;
	}

	void Empty()
	{
		ObjectId = 0;
		Fields.Empty();
		FieldMem.Length(0);
	}

	int64 GetInt(int Id, int Default = -1)
	{
		Field *f = GetField(Id);
		if (!f || f->Type == LObject)
			return Default;

		switch (f->Type)
		{
			case LInt:
				switch (f->Size)
				{
					case 1:
						return f->u8;
					case 2:
						return f->u16;
					case 4:
						return f->u32;
					case 8:
						return f->u64;
				}
			case LFloat:
				return (int)f->f;
			case LString:
				if (f->Size == 1)
					return atoi(f->s.u);
				else if (f->Size == 2)
					return AtoiW(f->s.w);
				else
					LgiAssert(!"Invalid string size");
				break;
			default:
				LgiAssert(!"Invalid type.");
		}

		return Default;
	}

	const char *GetStr(int Id, const char *Default = NULL)
	{
		Field *f = GetField(Id);
		if (!f || f->Type != LString)
			return Default;
		
		if (f->Size == 1)
			return f->s.u;
		else
			LgiAssert(!"Request for wrong string width");

		return Default;
	}

	const char16 *GetStrW(int Id, const char16 *Default = NULL)
	{
		Field *f = GetField(Id);
		if (!f || f->Type != LString)
			return Default;
		
		if (f->Size == 2)
			return f->s.w;
		else
			LgiAssert(!"Request for wrong string width");

		return Default;
	}

	bool SetBinary(int Id, void *Ptr, uint32 Size)
	{
		Field *f = Alloc(Id, 8 + Size);
		if (!f)
			return false;
		
		f->Type = LBinary;
		f->Size = 1;
		f->s.Length = Size;
		if (Ptr && Size > 0)
			memcpy(f->s.u, Ptr, Size);
		
		return true;
	}

	bool GetBinary(int Id, uint8 *&Ptr, uint32 &Size)
	{
		Field *f = GetField(Id);
		if (!f || f->Type != LBinary)
			return false;
		
		Ptr = (uint8*) f->s.u;
		Size = f->s.Length;

		#ifdef _DEBUG
		uint8 *End = Ptr + Size;
		uint8 *Flds = (uint8*)FieldMem.AddressOf();
		LgiAssert(Ptr >= Flds && End <= Flds + FieldMem.Length());
		#endif
		
		return true;
	}

	template<typename T>
	bool SetInt(int Id, T i)
	{
		Field *f = Alloc(Id, 4 + sizeof(i));
		if (!f)
			return false;

		f->Type = LInt;
		f->Size = sizeof(i);
		memcpy(f->bytes, &i, sizeof(i));
		return true;
	}

	bool SetStr(int Id, const char *u)
	{
		if (!u)
			return false;

		size_t len = strlen(u) + 1;
		Field *f = Alloc(Id, 8 + len);
		if (!f)
			return false;

		f->Type = LString;
		f->Size = 1;
		f->s.Length = (uint32) (len - 1);
		memcpy(f->s.u, u, len);
		return true;
	}

	bool SetStr(int Id, const char16 *w)
	{
		if (!w)
			return false;

		size_t len = Strlen(w) + 1;
		Field *f = Alloc(Id, 8 + len);
		if (!f)
			return false;

		f->Type = LString;
		f->Size = sizeof(*w);
		f->s.Length = (uint32) (len - 1);
		memcpy(f->s.u, w, len * sizeof(*w));
		return true;
	}

	bool CanRead(void *ptr, unsigned size)
	{
		if (!ptr || size < 8)
			return false;

		Field *f = (Field*)ptr;
		if (f->Type != LObject)
			return false;

		if (f->u32 > size)
			return false;

		return true;
	}

	bool Serialize(GStream *stream, bool write)
	{
		if ((ToStream = write))
		{
			uint32 Size = 0;
			for (ssize_t o = Fields.First(); o >= 0; o = Fields.Next())
			{
				Field *i = (Field*)FieldMem.AddressOf(o);
				Size += i->Sizeof();
			}

			Field f;
			f.Size = 0;
			f.Type = LObject;
			f.Id = GetObjectId();
			f.u32 = Size;
			
			if (stream->Write(&f, 8) != 8)
				return false;

			for (ssize_t o = Fields.First(); o >= 0; o = Fields.Next())
			{
				Field *i = (Field*)FieldMem.AddressOf(o);
				int bytes = i->Sizeof();
				if (stream->Write(i, bytes) != bytes)
					return false;
			}
		}
		else
		{
			Empty();

			Field f;
			if (stream->Read(&f, 8) != 8)
				return false;

			if (f.Type != LObject)
				return false;
			ObjectId = f.Id;

			FieldMem.Length(f.u32);
			if (stream->Read(FieldMem.AddressOf(), f.u32) != f.u32)
				return false;

			// Parse all the fields into a hash table for O(1) access.
			char *s = FieldMem.AddressOf();
			char *e = s + FieldMem.Length();
			while (s < e)
			{
				Field *fld = (Field*)s;
				if (!fld->IsValid())
				{
					LgiAssert(!"Invalid field");
					return false;
				}

				Fields.Add(fld->Id, s - FieldMem.AddressOf(0));
				s += fld->Sizeof();
			}
		}

		return true;
	}

};


#endif
