#ifndef _LJSON_H_
#define _LJSON_H_

#include "GArray.h"
#include "GStringClass.h"

#define SkipWs(s) while (*s && strchr(" \t\r\n", *s)) s++;

class LJson
{
	struct Key
	{
		GString Name;
		
		GString Str;
		GArray<Key> Obj;
		GArray<Key> Array;

		Key *Get(const char *name, bool create = false)
		{
			if (Name.Equals(name))
				return this;
			for (unsigned i=0; i<Obj.Length(); i++)
			{
				if (Obj[i].Get(name))
					return Obj.AddressOf(i);
			}
			if (create)
			{
				char *ArrStart = strchr(name, '[');
				char *ArrEnd = ArrStart ? strchr(ArrStart+1, ']') : NULL;
				if (ArrStart && ArrEnd)
				{
					int Idx = atoi(ArrStart + 1);
					if (Idx < 0)
						return NULL;

					GString n;
					n.Set(name, ArrStart - name);
					for (unsigned i=0; i<Obj.Length(); i++)
					{
						if (Obj[i].Get(n))
							return &(Obj[i].Array[Idx]);
					}

					Key &k = Obj.New();
					k.Name = n;
					return &k.Array[Idx];
				}

				Key &k = Obj.New();
				k.Name = name;
				return &k;
			}
			return NULL;
		}

		void Empty()
		{
			Name.Empty();
			Str.Empty();
			Obj.Length(0);
		}

		GString Print(int Depth = 0)
		{
			GStringPipe r(512);
			GString s;
			GString d("");
			if (Depth)
			{
				int bytes = Depth << 2;
				d.Length(bytes);
				memset(d.Get(), ' ', bytes);
				d.Get()[bytes] = 0;
			}

			if (Name)
			{
				s.Printf("%s\"%s\" : ", d.Get(), Name.Get());
				r += s;
			}
			if (Str)
			{
				GString q = GString::Escape(Str, Str.Length(), "\"\\");
				s.Printf("\"%s\"", q.Get());
				r += s;
			}

			if (Obj.Length())
			{
				// if (Name)
				{
					s.Printf("{\n");
					Depth++;
				}
				
				for (unsigned i=0; i<Obj.Length(); i++)
				{
					if (i)
						s += ",\n";
					Key &c = Obj[i];
					s += c.Print(Depth);
				}
				r += s;
				
				// if (Name)
				{
					s.Printf("\n%s}", d.Get());
					r += s;
					Depth--;
				}
			}
			else if (!Str)
			{
				r += "[ ";
				for (unsigned i=0; i<Array.Length(); i++)
				{
					Key &k = Array[i];
					if (i)
						r += ", ";
					if (k.Obj.Length())
						s = GString("{") + k.Print() + "}";
					else
						s = k.Print();
					r += s;
				}
				r += " ]";
			}

			return r.NewGStr();
		}

		Key *Deref(GString Addr, bool Create)
		{
			GString::Array p = Addr.SplitDelimit(".");
			Key *k = this;
			for (unsigned i=0; k && i<p.Length(); i++)
			{
				auto n = p[i];
				k = k->Get(n, Create);
			}
			return k;
		}
	};

	Key Root;
	const char *Start;

	bool ParseString(GString &s, const char *&c)
	{
		SkipWs(c);

		if (*c != '\"')
			return false;

		c++;
		const char *e = c;
		while (*e)
		{
			if (*e == '\\')
				e += 2;
			else if (*e == '\"')
				break;
			else
				e++;
		}
		if (*e != '\"')
			return false;
		s.Set(c, e - c);
		c = e + 1;
		return true;
	}

	bool ParseChar(char ch, const char *&c)
	{
		SkipWs(c);
		if (*c != ch)
			return false;
		c++;
		SkipWs(c);
		return true;
	}

	bool IsNumeric(char s)
	{
		return IsDigit(s) || strchr("-.e", s) != NULL;
	}

	bool ParseValue(Key &k, const char *&c)
	{
		SkipWs(c);

		if (*c == '{')
		{
			// Objects
			c++;
			if (*c == '}')
			{
				int asd=0;
			}

			if (!Parse(k.Obj, c))
				return false;
			if (!ParseChar('}', c))
				return false;
		}
		else if (*c == '[')
		{
			// Array
			c++;
			SkipWs(c);
			while (*c != ']')
			{
				Key &a = k.Array.New();
				if (!ParseValue(a, c))
				{
					return false;
				}

				SkipWs(c);
				if (*c == ',')
				{
					c++;
					SkipWs(c);
				}
			}
			if (*c == ']')
				c++;
			else
			{
				LgiAssert(!"Unexpected token.");
				return false;
			}
		}
		else if (*c == '\"')
		{
			if (!ParseString(k.Str, c))
			{
				return false;
			}
		}
		else if (IsNumeric(*c))
		{
			const char *e = c;
			while (*e && IsNumeric(*e))
				e++;
			k.Str.Set(c, e - c);
			c = e;
		}
		else if (IsAlpha(*c))
		{
			// Boolean?
			const char *e = c;
			while (*e && IsAlpha(*e))
				e++;
			k.Str.Set(c, e - c);
			c = e;
		}
		else
		{
			return false;
		}

		return true;
	}

	bool Parse(GArray<Key> &Ks, const char *&c)
	{
		while (true)
		{
			SkipWs(c);

			if (*c == '\"')
			{
				Key &k = Ks.New();
				if (!ParseString(k.Name, c))
					return false;
				if (!ParseChar(':', c))
					return false;
				if (!ParseValue(k, c))
					return false;
			}
			else if (*c == '{')
			{
				// Objects
				c++;
				if (!Parse(Ks, c))
					return false;
				if (!ParseChar('}', c))
					return false;
			}
			else if (*c == '}')
			{
				// Empty object i.e. {}
				return true;
			}
			else 
				return false;

			SkipWs(c);
			if (*c != ',')
				break;
			c++;
		}

		return true;
	}

	Key *Deref(GString Addr, bool Create)
	{
		return Root.Deref(Addr, Create);
	}

public:
	LJson()
	{
		Start = NULL;
	}

	LJson(const char *c)
	{
		SetJson(c);
	}

	void Empty()
	{
		Root.Empty();
	}

	bool SetJson(const char *c)
	{
		Empty();
		if (!c)
			return false;

		Start = c;
		bool b = Parse(Root.Obj, c);
		if (!b)
			LgiTrace("%s:%i - Error at char " LPrintfSizeT "\n", _FL, c - Start);
		return b;
	}

	GString GetJson()
	{
		GString s = Root.Print(0);
		#ifdef _DEBUG
		LgiAssert(LgiIsUtf8(s));
		#endif
		return s;
	}

	GString Get(GString Addr)
	{
		Key *k = Deref(Addr, false);
		return k ? k->Str : GString();
	}

	class Iter
	{
		LJson *j;
		GArray<Key> *a;

	public:
		struct IterPos
		{
			Iter *It;
			size_t Pos;

			IterPos(Iter *i, size_t p)
			{
				It = i;
				Pos = p;
			}

			bool operator !=(const IterPos &i) const
			{
				bool Eq = It == i.It && Pos == i.Pos;
				return !Eq;
			}

			IterPos &operator *()
			{
				return *this;
			}

			operator GString()
			{
				return (*It->a)[Pos].Str;
			}

			IterPos &operator++()
			{
				Pos++;
				return *this;
			}

			GString Get(GString Addr)
			{
				auto Arr = *It->a;
				if (Pos >= Arr.Length())
					return GString();
				Key &k = Arr[Pos];
				Key *v = k.Deref(Addr, false);
				if (!v)
					return GString();

				return v->Str;
			}

			Iter GetArray(GString Addr)
			{
				Iter a(It->j);

				auto Arr = *It->a;
				if (Pos < Arr.Length())
				{
					Key *k = Arr[Pos].Deref(Addr, false);
					if (k)
						a.Set(&k->Array);
				}

				return a;
			}
		};


		Iter(LJson *J = NULL) : j(J)
		{
			a = NULL;
		}

		void Set(LJson *J)
		{
			j = J;
		}

		void Set(GArray<Key> *arr)
		{
			a = arr;
		}

		IterPos begin()
		{
			return IterPos(this, 0);
		}

		IterPos end()
		{
			return IterPos(this, a ? a->Length() : 0);
		}
	};

	Iter GetArray(GString Addr)
	{
		Iter a(this);
		Key *k = Deref(Addr, false);
		if (k)
			a.Set(&k->Array);

		return a;
	}

	bool Set(GString Addr, const char *Val)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		k->Str = Val;
		return true;
	}

	bool Set(GString Addr, GArray<GString> &Array)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		for (auto &a : Array)
		{
			k->Array.New().Str = a;
		}
		return true;
	}

	GArray<GString> GetKeys(const char *Addr = NULL)
	{
		Key *k = Deref(Addr, true);
		GArray<GString> a;
		if (k)
			for (auto &i : k->Obj)
				a.Add(i.Name);
		return a;
	}
};

#endif