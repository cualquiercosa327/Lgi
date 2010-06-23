#include "Lgi.h"
#include "UnitTests.h"

class GAutoPtrTestPriv
{
};

GAutoPtrTest::GAutoPtrTest() : UnitTest("GAutoPtrTest")
{
}

GAutoPtrTest::~GAutoPtrTest()
{
}

GAutoString CreateAutoString(char **Ptr)
{
	return GAutoString(*Ptr = NewStr("Something"));
}

bool GAutoPtrTest::Run()
{
	char *Ptr;
	GAutoString s1(Ptr = NewStr("string1"));
	GAutoString s2 = s1;
	if (s1.Get() != 0)
	{
		return FAIL(_FL, "Initial auto ptr should be zero now.");
	}
	if (s2.Get() != Ptr)
	{
		return FAIL(_FL, "Ptrs should be equal.");
	}

	char *Ptr2 = 0;
	GAutoString s3 = CreateAutoString(&Ptr2);
	if (s3.Get() != Ptr2)
	{
		return FAIL(_FL, "Ptr not the same.");
	}
	if (strnicmp(s3, "Something", 9))
	{
		return FAIL(_FL, "Ptr contents are wrong.");
	}

	GAutoString s4(NewStr("  asd dfg  "));
	if (s4.find("asd") != 2)
		return FAIL(_FL, "find returned wrong value");
	if (s4.find("dfg", 8) != -1)
		return FAIL(_FL, "find returned wrong value");
	if (s4.find("dfg", 0, 8) != -1)
		return FAIL(_FL, "find returned wrong value");
	if (s4.find("dfg", 0, 9) != 6)
		return FAIL(_FL, "find returned wrong value");

	GAutoString::A a1 = s4.split();
	if (a1.Length() != 2)
		return FAIL(_FL, "split failed");
	if (strcmp(a1[0], "asd"))
		return FAIL(_FL, "split failed");
	if (strcmp(a1[1], "dfg"))
		return FAIL(_FL, "split failed");

	GAutoString s5 = s4.strip().upper();
	if (strcmp(s5, "ASD DFG"))
		return FAIL(_FL, "upper failed");

	GAutoString s6 = s5.split()[1].lower();
	if (strcmp(s6, "dfg"))
		return FAIL(_FL, "lower failed");

	return true;
}

