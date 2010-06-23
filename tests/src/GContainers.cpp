#include "Lgi.h"
#include "UnitTests.h"

class GContainersPriv
{
};

GContainers::GContainers() : UnitTest("GContainers")
{
}

GContainers::~GContainers()
{
}

bool GContainers::Run()
{
	{
		GArray<int> a;
		a.Add(1);
		a.Add(4);
		a.Add(5);
		a.Add(8);

		int n = 0;
		for (GArray<int>::I i=a.Start(); i.In(); i++, n++)
		{
			if (a[n] != *i)
				return FAIL(_FL, "iterator error");
		}
		n = 0;
		GArray<int>::I i2 = a.Start();
		while (i2.Each())
		{
			if (a[n] != *i2)
				return FAIL(_FL, "iterator error");
			n++;
		}
	}

	{
		List<int> a;
		a.Add(new int(1));
		a.Add(new int(4));
		a.Add(new int(5));
		a.Add(new int(8));

		int n = 0;
		for (List<int>::I i=a.Start(); i.In(); i++, n++)
		{
			if (*a[n] != **i)
				return FAIL(_FL, "iterator error");
		}
		n = 0;
		List<int>::I i2 = a.Start();
		while (i2.Each())
		{
			if (*a[n] != **i2)
				return FAIL(_FL, "iterator error");
			n++;
		}

		n = 0;
		List<int>::I i3 = a.Start();
		for (int *v = *i3; i3; v = *++i3)
		{
			if (*a[n] != **i3)
				return FAIL(_FL, "iterator error");
			n++;
		}
	}


	return true;
}

