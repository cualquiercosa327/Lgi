#ifndef _GMAP_H
#define _GMAP_H

#define GMapTemplate		template <typename N, typename V>
#define GMapClass			GMap<N,V>

GMapTemplate
class GMap
{
	class Mapping
	{
	public:
		N Name;
		V Value;

		Mapping()
		{
			Name = 0;
			Value = 0;
		}
	};

	List<Mapping> Maps;
	N NewName;
	bool CaseSen;
	
	// New
	const char *New(const char *s) { return (const char*)NewStr(s); }
	char *New(char *s) { return NewStr(s); }
	const char16 *New(const char16 *s) { return (const char16*) NewStrW(s); }
	char16 *New(char16 *s) { return NewStrW(s); }
	int New(int i) { return i; }

	// Cmp
	int Cmp(const char *a, const char *b)
	{
		return CaseSen ? strcmp(a, b) : _stricmp(a, b);
	}
	
	int Cmp(char *a, char *b)
	{
		return CaseSen ? strcmp(a, b) : _stricmp(a, b);
	}
	
	int Cmp(const char16 *a, const char16 *b)
	{
		return CaseSen ? StrcmpW(a, b) : StricmpW(a, b);
	}
	
	int Cmp(char16 *a, char16 *b)
	{
		return CaseSen ? StrcmpW(a, b) : StricmpW(a, b);
	}
	
	int Cmp(int a, int b)
	{
		return a - b;
	}

	// Delete
	void Delete(char *&a) { DeleteArray(a); }
	void Delete(char16 *&a) { DeleteArray(a); }
	void Delete(int &a) { }

protected:
	Mapping *Get(N n)
	{
		for (Mapping *m=Maps.First(); m; m=Maps.Next())
		{
			if (Cmp(m->Name, n) == 0)
			{
				return m;
			}
		}

		return 0;
	}

public:
	GMap(bool Case = false)
	{
		NewName = 0;
		CaseSen = Case;
	}
	
	~GMap()
	{
		Empty();
	}

	void Empty()
	{
		for (Mapping *m=Maps.First(); m; m=Maps.Next())
		{
			Delete(m->Name);
			Delete(m->Value);
		}

		Maps.DeleteObjects();
		NewName = 0;
	}

	N NameAt(int i)
	{
		Mapping *m = Maps[i];
		if (m)
		{
			return m->Name;
		}
		return 0;
	}

	N Reverse(const V v)
	{
		for (Mapping *m=Maps.First(); m; m=Maps.Next())
		{
			if (Cmp(v, m->Value) == 0)
			{
				return m->Name;
			}
		}

		return 0;
	}

	int Length()
	{
		return Maps.Length();
	}

	bool HasMap(N s)
	{
		return Get(s) != 0;
	}
	
	GMapClass &operator [](N s)
	{
		NewName = s;
		return *this;
	}

	GMapClass &operator =(V v)
	{
		if (NewName)
		{
			Mapping *m = Get(NewName);
			if (!m)
			{
				m = new Mapping;
				if (m)
				{
					m->Name = New(NewName);
					Maps.Insert(m);
				}
			}

			if (m)
			{
				Delete(m->Value);
				m->Value = New(v);
			}
		}

		return *this;
	}

	operator V()
	{
		Mapping *m;
		if (m = Get(NewName))
		{
			return m->Value;
		}

		return 0;
	}
};

typedef GMap<char*, char*> GStrMap;

#endif
