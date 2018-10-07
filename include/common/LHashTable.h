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

#define HASH_TABLE_SHRINK_THRESHOLD			15
#define HASH_TABLE_GROW_THRESHOLD			50

template<typename T, T DefaultNull = (T)-1>
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

template<typename T, T DefaultNull = (T)NULL>
class PtrKey
{
public:
	typedef T Type;

	T NullKey;

	PtrKey<T,DefaultNull>()
	{
		NullKey = DefaultNull;
	}

	void EmptyKeys() {}
	uint32 Hash(T k) { return (uint32)(((size_t)k)/31); }
	T CopyKey(T a) { return a; }
	size_t SizeKey(T a) { return sizeof(a); }
	void FreeKey(T &a) { a = NullKey; }
	bool CmpKey(T a, T b)
	{
		return a == b;
	}
};

template<typename T, bool CaseSen = true, T *DefaultNull = (T*)NULL>
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

template<typename T, int BlockSize = 0>
class KeyPool
{
protected:
	struct Buf : public GArray<T>
	{
		size_t Used;
		Buf(size_t Sz = 0) { this->Length(Sz); }
		size_t Free() { return this->Length() - Used; }
	};

	GArray<Buf> Mem;
	Buf *GetMem(size_t Sz)
	{
		if (!Mem.Length() || Mem.Last().Free() < Sz)
			Mem.New().Length(PoolSize);
		return Mem.Last().Free() >= Sz ? &Mem.Last() : NULL;
	}

public:
	const int DefaultPoolSize = (64 << 10) / sizeof(T);
	int PoolSize;

	KeyPool<T,BlockSize>()
	{
		PoolSize = BlockSize ? BlockSize : DefaultPoolSize;
	}

	void EmptyKeys()
	{
		Mem.Length(0);
	}
};

template<typename T, bool CaseSen = true, T *DefaultNull = (T*)NULL, int BlockSize = 0>
class StrKeyPool : public KeyPool<T,BlockSize>
{
public:
	typedef T *Type;
	using Buf = typename KeyPool<T,BlockSize>::Buf;

	T *NullKey;

	StrKeyPool<T,CaseSen,DefaultNull,BlockSize>()
	{
		NullKey = DefaultNull;
	}

	uint32 Hash(T *k) { return LHash<uint32,T>(k, Strlen(k), CaseSen); }
	size_t SizeKey(T *a) { return (Strlen(a)+1)*sizeof(*a); }
	bool CmpKey(T *a, T *b) { return !(CaseSen ? Strcmp(a, b) : Stricmp(a, b)); }

	T *CopyKey(T *a)
	{
		size_t Sz = Strlen(a) + 1;
		Buf *m = this->GetMem(Sz);
		if (!m) return NullKey;
		T *r = m->AddressOf(m->Used);
		memcpy(r, a, Sz*sizeof(*a));
		m->Used += Sz;
		return r;
	}
	
	void FreeKey(T *&a)
	{
		// Do nothing... memory is own by KeyPool
		a = NullKey;
	}
};

template<typename T, bool CaseSen = true, const T *DefaultNull = (const T*)NULL>
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

template<typename T, bool CaseSen = true, const T *DefaultNull = (const T*)NULL, int BlockSize = 0>
class ConstStrKeyPool : public KeyPool<T,BlockSize>
{
public:
	typedef const T *Type;
	using Buf = typename KeyPool<T,BlockSize>::Buf;

	const T *NullKey;

	ConstStrKeyPool<T,CaseSen,DefaultNull,BlockSize>()
	{
		NullKey = DefaultNull;
	}

	uint32 Hash(const T *k) { return LHash<uint32,T>(k, Strlen(k), CaseSen); }
	size_t SizeKey(const T *a) { return (Strlen(a)+1)*sizeof(*a); }
	bool CmpKey(const T *a, const T *b) { return !(CaseSen ? Strcmp(a, b) : Stricmp(a, b)); }

	const T *CopyKey(const T *a)
	{
		size_t Sz = Strlen(a) + 1;
		Buf *m = this->GetMem(Sz);
		if (!m) return NullKey;
		T *r = m->AddressOf(m->Used);
		memcpy(r, a, Sz*sizeof(*a));
		m->Used += Sz;
		return r;
	}

	void FreeKey(const T *&a)
	{
		// Do nothing... memory is own by KeyPool
		a = NullKey;
	}
};

/// General hash table container for O(1) access to table data.
template<typename KeyTrait, typename Value>
class LHashTbl : public KeyTrait
{
public:
	typedef typename KeyTrait::Type Key;
	typedef LHashTbl<KeyTrait,Value> HashTable;
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
	int Version;	// This changes every time 'Table' is resized.
					// It's used to invalidate iterators.
	Pair *Table;

	int Percent()
	{
		return (int) (Used * 100 / Size);
	}

	bool GetEntry(const Key k, ssize_t &Index, bool Debug = false)
	{
		if (k != this->NullKey && Table)
		{
			uint32 h = this->Hash(k);

			for (size_t i=0; i<Size; i++)
			{
				Index = (h + i) % Size;

				if (Table[Index].key == this->NullKey)
					return false;
					
				if (this->CmpKey(Table[Index].key, k))
					return true;
			}
		}

		return false;
	}

	bool Between(ssize_t Val, ssize_t Min, ssize_t Max)
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


	void InitializeTable(Pair *e, ssize_t len)
	{
		if (!e || len < 1) return;
		while (len--)
		{
			e->key = this->NullKey;
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
		/// The default empty value
		Value nullvalue = (Value)0
	)
	{
		Size = size;
		NullValue = nullvalue;
		Used = 0;
		Version = 0;
		MaxSize = LHASHTBL_MAX_SIZE;
		// LgiAssert(Size <= MaxSize);
		
		if ((Table = new Pair[Size]))
		{
			InitializeTable(Table, Size);
		}
	}

	LHashTbl(const HashTable &init)
	{
		Size = init.Size;
		NullValue = init.NullValue;
		Used = 0;
		Version = 0;
		MaxSize = LHASHTBL_MAX_SIZE;
		if ((Table = new Pair[Size]))
		{
			for (size_t i=0; i<Size; i++)
			{
				Table[i].key = this->NullKey;
			}
			*this = init;
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
		return this->NullKey;
	}

	/// Copy operator
	HashTable &operator =(const HashTable &c)
	{
		if (IsOk() && c.IsOk())
		{
			Empty();

			this->NullKey = c.NullKey;
			NullValue = c.NullValue;

			size_t Added = 0;
			for (size_t i=0; i<c.Size; i++)
			{
				if (c.Table[i].key != c.NullKey)
				{
					Added += Add(c.Table[i].key, c.Table[i].value) ? 1 : 0;
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

		size_t OldSize = Size;
		size_t NewSize = MAX((size_t)s, Used * 10 / 7);
		if (NewSize != Size)
		{
			Pair *OldTable = Table;

			Used = 0;
			LgiAssert(NewSize <= MaxSize);
			Size = NewSize;

			Table = new Pair[Size];
			if (Table)
			{
				size_t i;
				InitializeTable(Table, Size);
				for (i=0; i<OldSize; i++)
				{
					if (OldTable[i].key != this->NullKey)
					{
						if (!Add(OldTable[i].key, OldTable[i].value))
						{
							LgiAssert(0);
						}
						this->FreeKey(OldTable[i].key);
					}
				}

				Version++;
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
	
	/// Returns true if the object appears to be valid
	bool IsOk() const
	{
		bool Status =
						#ifndef __llvm__
						this != 0 &&
						#endif
						Table != 0;
		if (!Status)
		{
			#ifndef LGI_STATIC
			LgiTrace("%s:%i - this=%p Table=%p Used=%i Size=%i\n", _FL, this, Table, Used, Size);
			#endif
			LgiAssert(0);
		}		
		return Status;
	}

	/// Gets the number of entries used
	size_t Length()
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
			k == this->NullKey &&
			v == NullValue)
		{
			LgiAssert(!"Adding NULL key or value.");
			return false;
		}

		uint32 h = this->Hash(k);

		ssize_t Index = -1;
		for (size_t i=0; i<Size; i++)
		{
			size_t idx = (h + i) % Size;
			if
			(
				Table[idx].key == this->NullKey
				||
				this->CmpKey(Table[idx].key, k)
			)
			{
				Index = idx;
				break;
			}
		}

		if (Index >= 0)
		{
			if (Table[Index].key == this->NullKey)
			{
				Table[Index].key = this->CopyKey(k);
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
		Key k,
		/// Turns off resizing, in case your iterating over the hash table,
		/// where resizing would invalidate the iterators.
		bool NoResize = false
	)
	{
		ssize_t Index = -1;
		if (GetEntry(k, Index))
		{
			// Delete the entry
			this->FreeKey(Table[Index].key);
			Table[Index].value = NullValue;
			Used--;
			
			// Bubble down any entries above the hole
			ssize_t Hole = Index;
			for (ssize_t i = (Index + 1) % Size; i != Index; i = (i + 1) % Size)
			{
				if (Table[i].key != this->NullKey)
				{
					uint32 Hsh = this->Hash(Table[i].key);
					uint32 HashIndex = Hsh % Size;
					
					if (HashIndex != i && Between(Hole, HashIndex, i))
					{
						// Do bubble
						if (Table[Hole].key != this->NullKey)
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
			if (!NoResize &&
				Percent() < HASH_TABLE_SHRINK_THRESHOLD)
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
		ssize_t Index = -1;
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

		return this->NullKey;
	}

	/// Removes all key/value pairs from memory
	void Empty()
	{
		if (!IsOk())
			return;

		for (size_t i=0; i<Size; i++)
		{
			if (Table[i].key != this->NullKey)
			{
				this->FreeKey(Table[i].key);
				LgiAssert(Table[i].key == this->NullKey);
			}
			Table[i].value = NullValue;
		}

		Used = 0;
		this->EmptyKeys();
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
		for (size_t i=0; i<Size; i++)
		{
			if (Table[i].key != this->NullKey)
			{
				Keys++;
				KeySize += SizeKey(Table[i].key);
			}
		}

		// LgiFormatSize(s, KeySize);
		// LgiTrace("%s in %i keys\n", s, Keys);

		return Sz + KeySize;
	}

	/// Deletes values as objects
	void DeleteObjects()
	{
		for (size_t i=0; i<Size; i++)
		{
			if (Table[i].key != this->NullKey)
				this->FreeKey(Table[i].key);

			if (Table[i].value != NullValue)
				DeleteObj(Table[i].value);
		}

		Used = 0;
	}

	/// Deletes values as arrays
	void DeleteArrays()
	{
		for (size_t i=0; i<Size; i++)
		{
			if (Table[i].key != this->NullKey)
				this->FreeKey(Table[i].key);

			if (Table[i].value != NullValue)
				DeleteArray(Table[i].value);
		}

		Used = 0;
	}

	/// Swaps the objects
	void Swap(LHashTbl<KeyTrait,Value> &h)
	{
		LSwap(this->NullKey, h.NullKey);
		LSwap(NullValue, h.NullValue);
		LSwap(Used, h.Used);
		LSwap(Size, h.Size);
		LSwap(MaxSize, h.MaxSize);
		LSwap(Version, h.Version);
		LSwap(Table, h.Table);
	}

	struct PairIterator
	{
		LHashTbl<KeyTrait,Value> *t;
		ssize_t Idx;
		int Version;

	public:
		PairIterator(LHashTbl<KeyTrait,Value> *tbl, ssize_t i)
		{
			t = tbl;
			Version = t->Version;
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
				if (Version != t->Version)
				{
					#ifndef LGI_UNIT_TESTS
					LgiAssert(!"Iterator invalidated");
					#endif
					*this = t->end();
				}
				else
				{
					while (++Idx < (ssize_t)t->Size)
					{
						if (t->Table[Idx].key != t->NullKey)
							break;
					}
				}
			}

			return *this;
		}

		PairIterator &operator ++() { return Next(); }
		PairIterator &operator ++(int) { return Next(); }

		Pair &operator *()
		{
			LgiAssert(	Idx >= 0 &&
						Idx < (ssize_t)t->Size &&
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

#endif

