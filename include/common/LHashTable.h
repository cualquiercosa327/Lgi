/*
	More modern take on the GHashTbl I had been using for a while.
	Moved the key management into a parameter class. All the key pooling
	is also now managed by the param class rather than the hash table itself.
*/
#ifndef _LHashTbl_H_
#define _LHashTbl_H_

#include <ctype.h>
#include "GMem.h"
#include "GArray.h"
#include "GString.h"
#include "LgiClass.h"

#ifndef LHASHTBL_MAX_SIZE
#define LHASHTBL_MAX_SIZE	(64 << 10)
#endif

template<typename RESULT, typename CHAR>
RESULT LHash(const CHAR *v, int l, bool Case)
{
	RESULT h = 0;

	if (Case)
	{
		// case sensitive
		if (l > 0)
		{
			while (l--)
				h = (h << 5) - h + *v++;
		}
		else
		{
			for (; *v; v ++)
				h = (h << 5) - h + *v;
		}
	}
	else
	{
		// case insensitive
		CHAR c;
		if (l > 0)
		{
			while (l--)
			{
				c = tolower(*v);
				v++;
				h = (h << 5) - h + c;
			}
		}
		else
		{
			for (; *v; v++)
			{
				c = tolower(*v);
				h = (h << 5) - h + c;
			}
		}
	}

	return h;
}

#define HASH_TABLE_SHRINK_THRESHOLD			15
#define HASH_TABLE_GROW_THRESHOLD			50

template<typename T, T DefaultNull = -1>
class IntKey
{
public:
	typedef T Type;

	T NullKey;

	IntKey<T,DefaultNull>()
	{
		NullKey = DefaultNull;
	}

	void EmptyKeys() {}
	uint32 Hash(T k) { return (uint32)k; }
	T CopyKey(T a) { return a; }
	size_t SizeKey(T a) { return sizeof(a); }
	void FreeKey(T &a) { a = NullKey; }
	bool CmpKey(T a, T b)
	{
		return a == b;
	}
};

template<typename T, bool CaseSen = true, T *DefaultNull = NULL>
class StrKey
{
public:
	typedef T *Type;

	T *NullKey;

	StrKey<T,CaseSen,DefaultNull>()
	{
		NullKey = DefaultNull;
	}

	void EmptyKeys() {}
	uint32 Hash(T *k) { return LHash<uint32,T>(k, Strlen(k), CaseSen); }
	T *CopyKey(T *a) { return Strdup(a); }
	size_t SizeKey(T *a) { return (Strlen(a)+1)*sizeof(*a); }
	void FreeKey(T *&a) { if (a) delete [] a; a = NullKey; }
	bool CmpKey(T *a, T *b) { return !(CaseSen ? Strcmp(a, b) : Stricmp(a, b)); }
};

template<typename T, bool CaseSen = true, int BlockSize = 0, T *DefaultNull = NULL>
class StrKeyPool
{
	struct Buf : public GArray<T>
	{
		int Used;
		Buf(int Sz = 0) { Length(Sz); }
		size_t Free() { return Length() - Used; }
	};

	GArray<Buf> Mem;
	Buf *GetMem(size_t Sz)
	{
		if (!Mem.Length() || Mem.Last().Free() < Sz)
			Mem.New().Length(PoolSize);
		return Mem.Last().Free() >= Sz ? &Mem.Last() : NULL;
	}

public:
	typedef T *Type;

	const int DefaultPoolSize = (64 << 10) / sizeof(T);
	T *NullKey;
	int PoolSize;

	StrKeyPool<T,CaseSen,BlockSize,DefaultNull>()
	{
		NullKey = DefaultNull;
		PoolSize = BlockSize ? BlockSize : DefaultPoolSize;
	}

	void EmptyKeys()
	{
		Mem.Length(0);
	}

	uint32 Hash(T *k) { return LHash<uint32,T>(k, Strlen(k), CaseSen); }
	size_t SizeKey(T *a) { return (Strlen(a)+1)*sizeof(*a); }
	bool CmpKey(T *a, T *b) { return !(CaseSen ? Strcmp(a, b) : Stricmp(a, b)); }

	T *CopyKey(T *a)
	{
		size_t Sz = Strlen(a) + 1;
		Buf *m = GetMem(Sz);
		if (!m) return NullKey;
		T *r = m->AddressOf(m->Used);
		memcpy(r, a, Sz*sizeof(*a));
		m->Used += Sz;
		return r;
	}
	
	void FreeKey(T *&a)
	{
		// Do nothing...
	}
};

template<typename T, bool CaseSen = true, T *DefaultNull = NULL>
class ConstStrKey
{
public:
	typedef const T *Type;

	const T *NullKey;

	ConstStrKey<T,CaseSen,DefaultNull>()
	{
		NullKey = DefaultNull;
	}

	void EmptyKeys() {}
	uint32 Hash(const T *k) { return LHash<uint32,T>(k, Strlen(k), CaseSen); }
	T *CopyKey(const T *a) { return Strdup(a); }
	size_t SizeKey(const T *a) { return (Strlen(a)+1)*sizeof(*a); }
	void FreeKey(const T *&a) { if (a) delete [] a; a = NullKey; }
	bool CmpKey(const T *a, const T *b) { return !(CaseSen ? Strcmp(a, b) : Stricmp(a, b)); }
};

/// General hash table container for O(1) access to table data.
template<typename KeyTrait, typename Value>
class LHashTbl : public KeyTrait
{
public:
	typedef typename KeyTrait::Type Key;
	const int DefaultSize = 256;

	struct Pair
	{
		Key key;
		Value value;
	};

protected:
	Value NullValue;

	size_t Used;
	size_t Size;
	size_t MaxSize;
	Pair *Table;

	int Percent()
	{
		return (int) (Used * 100 / Size);
	}

	bool GetEntry(const Key k, int &Index, bool Debug = false)
	{
		if (k != NullKey && Table)
		{
			uint32 h = Hash(k);

			for (int i=0; i<Size; i++)
			{
				Index = (h + i) % Size;

				if (Table[Index].key == NullKey)
					return false;
					
				if (CmpKey(Table[Index].key, k))
					return true;
			}
		}

		return false;
	}

	bool Between(int Val, int Min, int Max)
	{
		if (Min <= Max)
		{
			// Not wrapped
			return Val >= Min && Val <= Max;
		}
		else
		{
			// Wrapped
			return Val <= Max || Val >= Min;
		}
	}


	void InitializeTable(Pair *e, int len)
	{
		if (!e || len < 1) return;
		while (len--)
		{
			e->key = NullKey;
			e->value = NullValue;
			e++;
		}
	}

public:
	/// Constructs the hash table
	LHashTbl
	(
		/// Sets the initial table size. Should be 2x your data set.
		size_t size = 0,
		/// Sets the case sensitivity of the keys.
		bool is_case = true,
		/// The default empty value
		Value nullvalue = (Value)0
	)
	{
		Size = size;
		NullValue = nullvalue;
		Used = 0;
		MaxSize = LHASHTBL_MAX_SIZE;
		// LgiAssert(Size <= MaxSize);
		
		if ((Table = new Pair[Size]))
		{
			InitializeTable(Table, Size);
		}
	}
	
	/// Deletes the hash table removing all contents from memory
	virtual ~LHashTbl()
	{
		Empty();
		DeleteArray(Table);
	}

	Key GetNullKey()
	{
		return NullKey;
	}

	/// Copy operator
	LHashTbl<Key, Value> &operator =(LHashTbl<Key, Value> &c)
	{
		if (IsOk() && c.IsOk())
		{
			Empty();

			NullKey = c.NullKey;
			NullValue = c.NullValue;
			Case = c.Case;
			Pool = c.Pool;

			int Added = 0;
			for (int i=0; i<c.Size; i++)
			{
				if (c.Table[i].k != c.NullKey)
				{
					Added += Add(c.Table[i].k, c.Table[i].v) ? 1 : 0;
					LgiAssert(Added <= c.Used);
				}
			}
		}
		return *this;
	}

	void SetMaxSize(int m)
	{
		MaxSize = m;
	}

	/// Gets the total available entries
	int64 GetSize()
	{
		return IsOk() ? Size : 0;
	}
	
	/// Sets the total available entries
	bool SetSize(int64 s)
	{
		bool Status = false;

		if (!IsOk())
			return false;

		int OldSize = Size;
		int NewSize = MAX((int)s, Used * 10 / 7);
		if (NewSize != Size)
		{
			Pair *OldTable = Table;

			Used = 0;
			LgiAssert(NewSize <= MaxSize);
			Size = NewSize;

			Table = new Pair[Size];
			if (Table)
			{
				int i;
				InitializeTable(Table, Size);
				for (i=0; i<OldSize; i++)
				{
					if (OldTable[i].key != NullKey)
					{
						if (!Add(OldTable[i].key, OldTable[i].value))
						{
							LgiAssert(0);
						}
						FreeKey(OldTable[i].key);
					}
				}

				Status = true;
			}
			else
			{
				LgiAssert(Table != 0);
				Table = OldTable;
				Size = OldSize;
				return false;
			}

			DeleteArray(OldTable);
		}

		return Status;
	}
	
	/// Gets the string pooling setting
	bool GetStringPool()
	{
		return IsOk() ? Pool : false;
	}
	
	/// Sets the string pooling setting. String pooling lowers the number of memory
	/// allocs/frees but will waste memory if you delete keys. Good for fairly large
	/// static tables.
	///
	/// (only works with using Key='char*')
	void SetStringPool(bool b)
	{
		if (IsOk())
			Pool = b;
	}
	
	/// Returns whether the keys are case sensitive
	bool IsCase()
	{
		return IsOk() ? Case : false;
	}

	/// Sets whether the keys are case sensitive
	void IsCase(bool c)
	{
		if (IsOk())
			Case = c;
	}
	
	/// Returns true if the object appears to be valid
	bool IsOk()
	{
		bool Status =
						#ifndef __llvm__
						this != 0 &&
						#endif
						Table != 0;
		if (!Status)
		{
			#ifndef LGI_STATIC
			LgiStackTrace("%s:%i - this=%p Table=%p Used=%i Size=%i\n", _FL, this, Table, Used, Size);
			#endif
			LgiAssert(0);
		}		
		return Status;
	}

	/// Gets the number of entries used
	int Length()
	{
		return IsOk() ? Used : 0;
	}

	/// Adds a value under a given key
	bool Add
	(
		/// The key to insert the value under
		Key k,
		/// The value to insert
		Value v
	)
	{
		if (!Size)
			SetSize(DefaultSize);

		if (IsOk() &&
			k == NullKey &&
			v == NullValue)
		{
			LgiAssert(!"Adding NULL key or value.");
			return false;
		}

		uint32 h = Hash(k);

		int Index = -1;
		for (int i=0; i<Size; i++)
		{
			int idx = (h + i) % Size;
			if
			(
				Table[idx].key == NullKey
				||
				CmpKey(Table[idx].key, k)
			)
			{
				Index = idx;
				break;
			}
		}

		if (Index >= 0)
		{
			if (Table[Index].key == NullKey)
			{
				Table[Index].key = CopyKey(k);
				Used++;
			}
			Table[Index].value = v;

			if (Percent() > HASH_TABLE_GROW_THRESHOLD)
			{
				SetSize(Size << 1);
			}
			return true;
		}

		LgiAssert(!"Couldn't alloc space.");
		return false;
	}
	
	/// Deletes a value at 'key'
	bool Delete
	(
		/// The key of the value to delete
		Key k
	)
	{
		int Index = -1;
		if (GetEntry(k, Index))
		{
			// Delete the entry
			FreeKey(Table[Index].key);
			Table[Index].value = NullValue;
			Used--;
			
			// Bubble down any entries above the hole
			int Hole = Index;
			for (int i = (Index + 1) % Size; i != Index; i = (i + 1) % Size)
			{
				if (Table[i].key != NullKey)
				{
					uint32 Hsh = Hash(Table[i].key);
					uint32 HashIndex = Hsh % Size;
					
					if (HashIndex != i && Between(Hole, HashIndex, i))
					{
						// Do bubble
						if (Table[Hole].key != NullKey)
						{
							LgiAssert(0);
						}
						memmove(Table + Hole, Table + i, sizeof(Table[i]));
						InitializeTable(Table + i, 1);
						Hole = i;
					}
				}
				else
				{
					// Reached the end of entries that could have bubbled
					break;
				}
			}

			// Check for auto-shrink limit
			if (Percent() < HASH_TABLE_SHRINK_THRESHOLD)
			{
				SetSize(Size >> 1);
			}
			
			return true;
		}
		else
		{
			GetEntry(k, Index, true);
		}

		return false;
	}

	/// Returns the value at 'key'
	Value Find(const Key k)
	{
		int Index = -1;
		if (IsOk() && GetEntry(k, Index))
		{
			return Table[Index].value;
		}

		return NullValue;
	}

	/// Returns the Key at 'val'
	Key FindKey(const Value val)
	{
		if (IsOk())
		{
			Pair *c = Table;
			Pair *e = Table + Size;
			while (c < e)
			{
				if (CmpKey(c->v, val))
				{
					return c->k;
				}
				c++;
			}
		}

		return NullKey;
	}

	/// Removes all key/value pairs from memory
	void Empty()
	{
		if (!IsOk())
			return;

		for (int i=0; i<Size; i++)
		{
			if (Table[i].key != NullKey)
			{
				FreeKey(Table[i].key);
				LgiAssert(Table[i].key == NullKey);
			}
			Table[i].value = NullValue;
		}

		Used = 0;
		EmptyKeys();
	}

	/// Returns the amount of memory in use by the hash table.
	int64 Sizeof()
	{
		int64 Sz = sizeof(*this);

		Sz += Sz * sizeof(Pair);

		// LgiFormatSize(s, Size);
		// LgiTrace("%s in %i hash entries (%i used)\n", s, Size, Used);

		int Keys = 0;
		int64 KeySize = 0;
		if (Pool)
		{
			for (unsigned i=0; i<Pools.Length(); i++)
			{
				KeyPool<Key> *p = Pools[i];
				KeySize += sizeof(*p) + p->Size;
			}
		}
		else
		{
			for (int i=0; i<Size; i++)
			{
				if (Table[i].k != NullKey)
				{
					Keys++;
					KeySize += SizeKey(Table[i].k);
				}
			}
		}

		// LgiFormatSize(s, KeySize);
		// LgiTrace("%s in %i keys\n", s, Keys);

		return Sz + KeySize;
	}

	/// Deletes values as objects
	void DeleteObjects()
	{
		for (int i=0; i<Size; i++)
		{
			if (Table[i].k != NullKey)
				FreeKey(Table[i].k);

			if (Table[i].v != NullValue)
				DeleteObj(Table[i].v);
		}

		Used = 0;
	}

	/// Deletes values as arrays
	void DeleteArrays()
	{
		for (int i=0; i<Size; i++)
		{
			if (Table[i].k != NullKey)
				FreeKey(Table[i].k);

			if (Table[i].v != NullValue)
				DeleteArray(Table[i].v);
		}

		Used = 0;
	}

	struct PairIterator
	{
		LHashTbl<KeyTrait,Value> *t;
		int Idx;

	public:
		PairIterator(LHashTbl<KeyTrait,Value> *tbl, int i)
		{
			t = tbl;
			Idx = i;
			if (Idx < 0)
				Next();
		}

		bool operator !=(const PairIterator &it) const
		{
			bool Eq = t == it.t &&
					Idx == it.Idx;
			return !Eq;
		}

		PairIterator &Next()
		{
			if (t->IsOk())
			{
				while (++Idx < t->Size)
				{
					if (t->Table[Idx].key != t->NullKey)
						break;
				}
			}

			return *this;
		}

		PairIterator &operator ++() { return Next(); }
		PairIterator &operator ++(int) { return Next(); }

		Pair &operator *()
		{
			LgiAssert(	Idx >= 0 &&
						Idx < t->Size &&
						t->Table[Idx].key != t->NullKey);
			return t->Table[Idx];
		}
	};

	PairIterator begin()
	{
		return PairIterator(this, -1);
	}

	PairIterator end()
	{
		return PairIterator(this, Size);
	}
};


/*
// Type specific implementations

// char
uint32 Hash(char *s) { return LHash<uint, uchar>((uchar*)s, 0, Case); }
char *CopyKey(char *a) { return NewStr(a); }
size_t SizeKey(char *a) { return strlen(a) + 1; }
void FreeKey(char *&a)
{
	if (!Pool) { DeleteArray(a); }
	else { a = NULL; }
}
bool CmpKey(char *a, char *b)
{
	return strcompare(a, b, Case) == 0;
}

// const char
uint32 Hash(const char *s) { return LHash<uint, uchar>((uchar*)s, 0, Case); }
char *CopyKey(const char *a) { return NewStr(a); }
size_t SizeKey(const char *a) { return strlen(a) + 1; }
void FreeKey(const char *&a)
{
	if (Pool) a = NULL;
	else DeleteArray((char*&)a);
}
bool CmpKey(const char *a, const char *b)
{
	return strcompare(a, b, Case) == 0;
}
	
// char16
uint32 Hash(char16 *s) { return LHash<uint, char16>(s, 0, Case); }
char16 *CopyKey(char16 *a) { return NewStrW(a); }
size_t SizeKey(char16 *a) { return StrlenW(a) + 1; }
void FreeKey(char16 *&a)
{
	if (Pool) a = NULL;
	else DeleteArray(a);
}
bool CmpKey(char16 *a, char16 *b)
{
	if (Case)
		return StrcmpW(a, b) == 0;
	else
		return StricmpW(a, b) == 0;
}

// const char16
uint32 Hash(const char16 *s) { return LHash<uint, char16>((char16*)s, 0, Case); }
const char16 *CopyKey(const char16 *a) { return NewStrW(a); }
size_t SizeKey(const char16 *a) { return StrlenW(a) + 1; }
void FreeKey(const char16 *&a)
{
	if (Pool) a = NULL;
	else DeleteArray(a);
}
bool CmpKey(const char16 *a, const char16 *b)
{
	if (Case)
		return StrcmpW(a, b) == 0;
	else
		return StricmpW(a, b) == 0;
}

// int
uint32 Hash(int s) { return s; }
int CopyKey(int a) { return a; }
size_t SizeKey(int a) { return sizeof(a); }
void FreeKey(int &a) { memcpy(&a, &NullKey, sizeof(a)); }
bool CmpKey(int a, int b)
{
	return a == b;
}

// unsigned
uint32 Hash(unsigned s) { return s; }
int CopyKey(unsigned a) { return a; }
size_t SizeKey(unsigned a) { return sizeof(a); }
void FreeKey(unsigned &a) { memcpy(&a, &NullKey, sizeof(a)); }
bool CmpKey(unsigned a, unsigned b)
{
	return a == b;
}

// int64
uint32 Hash(int64 s) { return (uint32)s; }
int64 CopyKey(int64 a) { return a; }
size_t SizeKey(int64 a) { return sizeof(a); }
void FreeKey(int64 &a) { memcpy(&a, &NullKey, sizeof(a)); }
bool CmpKey(int64 a, int64 b)
{
	return a == b;
}

// uint64
uint32 Hash(uint64 s) { return (uint32)s; }
uint64 CopyKey(uint64 a) { return a; }
size_t SizeKey(uint64 a) { return sizeof(a); }
void FreeKey(uint64 &a) { memcpy(&a, &NullKey, sizeof(a)); }
bool CmpKey(uint64 a, uint64 b)
{
	return a == b;
}

// void*
uint32 Hash(void *s) { return (uint32)(((NativeInt)s)/31); }
void *CopyKey(void *a) { return a; }
size_t SizeKey(void *a) { return sizeof(a); }
void FreeKey(void *&a) { memcpy(&a, &NullKey, sizeof(a)); }
bool CmpKey(void *a, void *b)
{
	return a == b;
}

    template<typename T>
    struct KeyPool
    {
	    uint32 Size;
	    size_t Used;
	    char *Mem;

	    KeyPool()
	    {
		    Size = 64 << 10;
		    Used = 0;
		    Mem = new char[Size];
	    }

	    ~KeyPool() { DeleteArray(Mem); }
        int New(int i) { return i; }        
        int New(unsigned i) { return i; }        
        int64 New(int64 i) { return i; }
		uint64 New(uint64 i) { return i; }
		void *New(void *i) { return i; }
        
	    char *New(char *s)
	    {
		    size_t Len = strlen(s) + 1;
		    if (Used < Size - Len)
		    {
			    char *p = Mem + Used;
			    strcpy_s(p, Len, s);
			    Used += Len;
			    return p;
		    }
		    return 0;
	    }

		char *New(const char *s)
	    {
		    size_t Len = strlen(s) + 1;
		    if (Used < Size - Len)
		    {
			    char *p = Mem + Used;
			    strcpy_s(p, Len, s);
			    Used += Len;
			    return p;
		    }
		    return 0;
	    }
		
	    char16 *New(char16 *s)
	    {
		    size_t Len = (StrlenW(s) + 1) * sizeof(char16);
		    if (Used < Size - Len)
		    {
			    char16 *p = (char16*) (Mem + Used);
			    StrcpyW(p, s);
			    Used += Len;
			    return p;
		    }
		    return 0;
	    }

	    char16 *New(const char16 *s)
	    {
		    size_t Len = (StrlenW(s) + 1) * sizeof(char16);
		    if (Used < Size - Len)
		    {
			    char16 *p = (char16*) (Mem + Used);
			    StrcpyW(p, s);
			    Used += Len;
			    return p;
		    }
		    return 0;
	    }
    };

	bool Pool;
	
	typedef GArray<KeyPool<Key>*> KeyPoolArr;
	KeyPoolArr Pools;

*/


#endif

