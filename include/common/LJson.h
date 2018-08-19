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
			GString r, s;
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
				if (Str)
				{
					s.Printf("\"%s\"", Str.Get());
					r += s;
				}
			}

			if (Obj.Length())
			{
				if (Name)
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
				if (Name)
				{
					s.Printf("\n%s}", d.Get());
					r += s;
					Depth--;
				}
			}

			return r;
		}
	};

	Key Root;

	bool ParseString(GString &s, const char *&c)
	{
		SkipWs(c);

		if (*c != '\"')
			return false;

		c++;
		const char *e = strchr(c, '\"');
		if (!e)
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
				if (*c == '{')
				{
					// Objects
					c++;
					if (!Parse(k.Obj, c))
						return false;
					if (!ParseChar('}', c))
						return false;
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
				else
				{
					return false;
				}
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
			else return false;

			SkipWs(c);
			if (*c != ',')
				break;
			c++;
		}

		return true;
	}

	Key *Deref(GString Addr, bool Create)
	{
		GString::Array p = Addr.SplitDelimit(".");
		Key *k = &Root;
		for (unsigned i=0; k && i<p.Length(); i++)
			k = k->Get(p[i], Create);
		return k;
	}

public:
	LJson()
	{
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
		return Parse(Root.Obj, c);
	}

	GString GetJson()
	{
		return Root.Print(0);
	}

	GString Get(GString Addr)
	{
		Key *k = Deref(Addr, false);
		return k ? k->Str : GString();
	}

	bool Set(GString Addr, const char *Val)
	{
		Key *k = Deref(Addr, true);
		if (!k)
			return false;
		k->Str = Val;
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