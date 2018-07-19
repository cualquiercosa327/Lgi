#include "Lgi.h"
#include "UnitTests.h"
#include "LUnrolledList.h"
#include "LHashTable.h"

class GContainersPriv
{
public:
	bool ListDeleteOne(int pos, int sz)
	{
		GArray<int> a;
		List<int> l;
		for (int i=0; i<sz; i++)
		{
			l.Insert(new int(i));
			a.Add(i);
		}

		l.DeleteAt(pos);
		a.DeleteAt(pos, true);
		
		if (l.Length() != a.Length())
			return false;

		int idx = 0;
		for (auto i : l)
		{
			if (*i != a[idx])
				return false;
			idx++;
		}

		return true;
	}

	bool ListInsert(int pos, int sz)
	{
		GArray<int> a;
		List<int> l;
		for (int i=0; i<sz; i++)
		{
			l.Insert(new int(i));
			a.Add(i);
		}

		l.Insert(new int(pos), pos);
		a.AddAt(pos, pos);
		
		if (l.Length() != a.Length())
			return false;

		int idx = 0;
		for (auto i : l)
		{
			if (*i != a[idx])
				return false;
			idx++;
		}

		return true;
	}
};

GContainers::GContainers() : UnitTest("GContainers")
{
	UnitTest_ListClass();
}

GContainers::~GContainers()
{
}

bool GContainers::Run()
{
	int ref[] = {1, 4, 5, 8, -1, -1};

	{
		GArray<int> a;
		a.Add(1);
		a.Add(4);
		a.Add(5);
		a.Add(8);

		int n = 0;
		for (auto &i : a)
		{
			if (i != ref[n++])
				return FAIL(_FL, "iterator error");
		}
	}

	{
		List<int> a;
		a.Add(new int(1));
		a.Add(new int(4));
		a.Add(new int(5));
		a.Add(new int(8));

		int n = 0;
		for (auto i : a)
		{
			if (*i != ref[n++])
				return FAIL(_FL, "iterator error");
		}
		if (n != 4)
			return FAIL(_FL, "count error");

		n = 0;
		auto i3 = a.begin();
		for (int *v = *i3; i3; v = *++i3)
		{
			if (*a[n] != **i3)
				return FAIL(_FL, "iterator error");
			n++;
		}
		if (n != 4)
			return FAIL(_FL, "count error");

		if (!d->ListInsert(45, 80))
			return FAIL(_FL, "list insert");
		if (!d->ListInsert(63, 80))
			return FAIL(_FL, "list insert");
		if (!d->ListInsert(64, 80))
			return FAIL(_FL, "list insert");

		if (!d->ListDeleteOne(45, 80))
			return FAIL(_FL, "list delete one");
		if (!d->ListDeleteOne(63, 80))
			return FAIL(_FL, "list delete one");
		if (!d->ListDeleteOne(64, 80))
			return FAIL(_FL, "list delete one");
	}

	{
		GHashTbl<int, int> h;
		for (int i=1; i<=100; i++)
		{
			h.Add(i, i << 1);
		}
		int count = 0;
		for (auto i : h)
		{
			if (i.key << 1 != i.value)
				return FAIL(_FL, "hash iterate");
			count++;
		}
		if (count != h.Length())
			return FAIL(_FL, "hash iterate");
		count = 0;
		for (auto i : h)
		{
			if (i.key == 50)
				h.Delete(i.key);
			else
				count++;
		}
		if (count != h.Length())
			return FAIL(_FL, "hash delete during iterate");
	}

	{
		LUnrolledList<int> IntLst;
		IntLst.Add(123);

		LUnrolledList<GString> StrLst;
		StrLst.Add("Hello");
		for (auto i : StrLst)
		{
			if (_stricmp(i, "Hello") != 0)
				return FAIL(_FL, "str lst iteration");
		}

		StrLst.Add("Aacc");
		StrLst.Add("Tyertw");
		StrLst.Add("Bbbsd");

		StrLst.Sort
		(
			[](GString &a, GString &b, int Dir)
			{
				return _stricmp(a, b) * Dir;
			},
			1
		);

		StrLst.Delete("Tyertw");
	}

	{
		LHashTbl<IntKey<int>,int> IntMap;
		IntMap.Add(45, 67);
		for (auto i : IntMap)
		{
			printf("Int: %i -> %i\n", i.key, i.value);
		}
		IntMap.Delete(45);

		LHashTbl<StrKey<char,true>,int> CaseSenMap;
		CaseSenMap.Add("Abc", 34);
		CaseSenMap.Add("abc", 23);
		if (CaseSenMap.Length() != 2)
			return FAIL(_FL, "Wrong len.");
		for (auto i : CaseSenMap)
		{
			printf("Case: %s -> %i\n", i.key, i.value);
		}

		LHashTbl<StrKey<char,false>,int> CaseInsenMap;
		CaseInsenMap.Add("Abc", 34);
		CaseInsenMap.Add("abc", 23);
		if (CaseInsenMap.Length() != 1)
			return FAIL(_FL, "Wrong len.");
		for (auto i : CaseInsenMap)
		{
			printf("Insensitive: %s -> %i\n", i.key, i.value);
		}

		LHashTbl<ConstStrKey<char16,false>,int> ConstWideMap;
		ConstWideMap.Add(L"Asd", 456);
		for (auto i : ConstWideMap)
		{
			printf("ConstWide: %S -> %i\n", i.key, i.value);
		}

		LHashTbl<StrKeyPool<char,false>,int> PoolMap;
		PoolMap.Add("Asd", 123);
		PoolMap.Add("Def", 124);
		for (auto i : PoolMap)
		{
			printf("PoolMap: %s -> %i\n", i.key, i.value);
		}
	}

	return true;
}

