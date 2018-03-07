#ifndef _GHashTbl_H_
#define _GHashTbl_H_

#include <ctype.h>
#include "GMem.h"
#include "GArray.h"
#include "GString.h"
#include "LgiClass.h"

#ifndef GHASHTBL_MAX_SIZE
#define GHASHTBL_MAX_SIZE	(64 << 10)
#endif

template<typename RESULT, typename CHAR>
RESULT LgiHash(CHAR *v, int l, bool Case)
{
	RESULT h = 0;

	if (Case)
	{
		// case sensitive
		if (l > 0)
		{
			while (l--)
			{
				h = (h << 5) - h + *v++;
			}
		}
		else
		{
			for (; *v; v ++)
			{
				h = (h << 5) - h + *v;
			}
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

/// General hash table container for O(1) access to table data.
template<typename Key, typename Value>
class GHashTbl
{
	Key NullKey;
	Value NullValue;

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

	    ~KeyPool()
	    {
		    DeleteArray(Mem);
	    }

        int New(int i)
        {
            return i;
        }
        
        int New(unsigned i)
        {
            return i;
        }
        
        int64 New(int64 i)
        {
            return i;
        }
        
        void *New(void *i)
        {
            return i;
        }
        
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

	struct Entry
	{
		Key k;
		Value v;
	};

	int Size;
	int MaxSize;
	int Cur;
	int Used;
	bool Case;
	Entry *Table;
	int SizeBackup; // temp variable for debugging

	bool Pool;
	
	typedef GArray<KeyPool<Key>*> KeyPoolArr;
	KeyPoolArr Pools;

	int Percent()
	{
		return Used * 100 / Size;
	}

	bool GetEntry(const Key k, int &Index, bool Debug = false)
	{
		if (k != NullKey && Table)
		{
			uint32 h = Hash(k);

			for (int i=0; i<Size; i++)
			{
				Index = (h + i) % Size;

				if (Table[Index].k == NullKey)
					return false;
					
				if (CmpKey(Table[Index].k, k))
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

	// Type specific implementations

		// char
			uint32 Hash(char *s) { return LgiHash<uint, uchar>((uchar*)s, 0, Case); }
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
			uint32 Hash(const char *s) { return LgiHash<uint, uchar>((uchar*)s, 0, Case); }
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
			uint32 Hash(char16 *s) { return LgiHash<uint, char16>(s, 0, Case); }
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
			uint32 Hash(const char16 *s) { return LgiHash<uint, char16>((char16*)s, 0, Case); }
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

	void InitializeTable(Entry *e, int len)
	{
		if (!e || len < 1) return;
		while (len--)
		{
			e->k = NullKey;
			e->v = NullValue;
			e++;
		}
	}

public:
	/// Constructs the hash table
	GHashTbl
	(
		/// Sets the initial table size. Should be 2x your data set.
		int size = 0,
		/// Sets the case sensitivity of the keys.
		bool is_case = true,
		/// The default empty value
		Key nullkey = 0,
		/// The default empty value
		Value nullvalue = (Value)0
	)
	{
		NullKey = nullkey;
		NullValue = nullvalue;
		Used = 0;
		Cur = -1;
		Case = is_case;
		Pool = false;
		SizeBackup = Size = size ? MAX(size, 16) : 512;
		MaxSize = GHASHTBL_MAX_SIZE;
		// LgiAssert(Size <= MaxSize);
		
		if ((Table = new Entry[Size]))
		{
			InitializeTable(Table, Size);
		}
	}
	
	/// Deletes the hash table removing all contents from memory
	virtual ~GHashTbl()
	{
		Empty();
		DeleteArray(Table);
	}

	Key GetNullKey()
	{
		return NullKey;
	}

	/// Copy operator
	GHashTbl<Key, Value> &operator =(GHashTbl<Key, Value> &c)
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
			Entry *OldTable = Table;

			Used = 0;
			LgiAssert(NewSize <= MaxSize);
			SizeBackup = Size = NewSize;

			Table = new Entry[Size];
			if (Table)
			{
				KeyPoolArr OldPools;
				OldPools = Pools;
				Pools.Length(0);

				int i;
				InitializeTable(Table, Size);
				for (i=0; i<OldSize; i++)
				{
					if (OldTable[i].k != NullKey)
					{
						if (!Add(OldTable[i].k, OldTable[i].v))
						{
							LgiAssert(0);
						}
						FreeKey(OldTable[i].k);
					}
				}

				OldPools.DeleteObjects();
				Status = true;
			}
			else
			{
				LgiAssert(Table != 0);
				Table = OldTable;
				SizeBackup = Size = OldSize;
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
		if (IsOk() &&
			k != NullKey &&
			v != NullValue)
		{
			uint32 h = Hash(k);

			int Index = -1;
			for (int i=0; i<Size; i++)
			{
				int idx = (h + i) % Size;
				if
				(
					Table[idx].k == NullKey
					||
					CmpKey(Table[idx].k, k)
				)
				{
					Index = idx;
					break;
				}
			}

			if (Index >= 0)
			{
				if (Table[Index].k == NullKey)
				{
					if (Pool)
					{
						KeyPool<Key> *p = 0;
						if (Pools.Length() == 0)
						{
							Pools[0] = p = new KeyPool<Key>;
						}
						else
						{
							p = Pools[Pools.Length()-1];
						}
						
						if (!(Table[Index].k = p->New(k)))
						{
							Pools.Add(p = new KeyPool<Key>);
							Table[Index].k = p->New(k);
						}
					}
					else
					{
						Table[Index].k = CopyKey(k);
					}
					Used++;
				}
				Table[Index].v = v;

				if (Percent() > HASH_TABLE_GROW_THRESHOLD)
				{
					SetSize(Size << 1);
				}
				return true;
			}
		}
		else LgiAssert(!"Adding NULL key or value.");

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
			if (Pool)
			{
				// Memory is owned by the string pool, not us.
				Table[Index].k = NullKey;
			}
			else
			{
				FreeKey(Table[Index].k);
			}
			Table[Index].v = NullValue;
			Used--;
			
			// Bubble down any entries above the hole
			int Hole = Index;
			for (int i = (Index + 1) % Size; i != Index; i = (i + 1) % Size)
			{
				if (Table[i].k != NullKey)
				{
					uint32 Hsh = Hash(Table[i].k);
					uint32 HashIndex = Hsh % Size;
					
					if (HashIndex != i && Between(Hole, HashIndex, i))
					{
						// Do bubble
						if (Table[Hole].k != NullKey)
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
			return Table[Index].v;
		}

		return NullValue;
	}

	/// Returns the Key at 'val'
	Key FindKey(const Value val)
	{
		if (IsOk())
		{
			Entry *c = Table;
			Entry *e = Table + Size;
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
	
	/// Returns the first value
	Value First(Key *k = 0)
	{
		Cur = 0;
		if (IsOk())
		{
			while (Cur < Size)
			{
				if (Table[Cur].k != NullKey)
				{
					if (k)
					{
						*k = Table[Cur].k;
					}
					return Table[Cur].v;
				}
				else Cur++;
			}
		}

		return NullValue;
	}
	
	/// Returns the current value
	Value Current(Key *k = 0)
	{
		if (Cur >= 0 &&
			Cur < Size &&
			Table &&
			Table[Cur].k != NullKey)
		{
			if (k)
			{
				*k = Table[Cur].k;
			}
			return Table[Cur].v;
		}

		return NullValue;
	}
	
	/// Returns the next value
	Value Next(Key *k = 0)
	{
		if (IsOk() && Cur >= 0)
		{
			while (++Cur < Size)
			{
				if (Table[Cur].k != NullKey)
				{
					if (k)
					{
						*k = Table[Cur].k;
					}
					return Table[Cur].v;
				}
			}
		}

		return NullValue;
	}

	/// Removes all key/value pairs from memory
	void Empty()
	{
		if (!IsOk())
			return;

		if (Pool)
		{
			Pools.DeleteObjects();

			for (int i=0; i<Size; i++)
			{
				Table[i].k = NullKey;
				Table[i].v = NullValue;
			}
		}
		else
		{
			for (int i=0; i<Size; i++)
			{
				if (Table[i].k != NullKey)
				{
					FreeKey(Table[i].k);
					LgiAssert(Table[i].k == NullKey);
				}
				Table[i].v = NullValue;
			}
		}

		Used = 0;
		SetSize(512);
	}

	/// Returns the amount of memory in use by the hash table.
	int64 Sizeof()
	{
		int64 Sz = sizeof(*this);

		Sz += Sz * sizeof(Entry);

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
};

/* Deprecated...
class LgiClass GHashTable : public GHashTbl<char*, void*>
{
public:
	GHashTable(int Size = 0, bool Case = true)
		: GHashTbl<char*, void*>(Size, Case)
	{
	}
	
	bool Add(char *k, void *v = (void*)1)
	{
		return GHashTbl<char*, void*>::Add(k, v);
	}
	
};
*/

#endif