#include "Lgi.h"
#include "UnitTests.h"
#include "GStringClass.h"

#ifdef LGI_UNIT_TESTS
int32 GString::RefStrCount = 0;
#endif

class GStringClassTestPriv
{
public:
};

GStringClassTest::GStringClassTest() : UnitTest("GStringClassTest")
{
	d = new GStringClassTestPriv;
}

GStringClassTest::~GStringClassTest()
{
	DeleteObj(d);
}

bool GStringClassTest::Run()
{
	// Constructor test
	{
		GString a("test");
		GString b = a;
	}
	if (GString::RefStrCount != 0) return false;

	// Assignment operator test
	{
		GString a("test");
		GString b;
		b = a;
	}
	if (GString::RefStrCount != 0) return false;

	// Split/Join tests
	{
		GString a("123,456,789");
		if (GString::RefStrCount != 1) return false;
		GString::Array b = a.Split(",");
		if (GString::RefStrCount != 4) return false;
		GString sep("-");
		if (GString::RefStrCount != 5) return false;
		GString c = sep.Join(b);
		if (GString::RefStrCount != 6) return false;
		if (c.Get() && stricmp(c.Get(), "123-456-789"))
			return false;
		if (b[1].Int() != 456)
			return false;		
	}
	if (GString::RefStrCount != 0) return false;
	
	// C string assignment / cast testing
	{
		const char *cstr = "Test";
		GString a = cstr;
		if (_stricmp(cstr, a))
			return false;
	}
	if (GString::RefStrCount != 0) return false;
	
	// Find testing
	{
		GString a("This is a test string");
		GString b; // Empty
		if (a.Find("asdasdwer") >= 0)
			return false;
		if (a.Find(NULL) >= 0)
			return false;
		if (b.Find(NULL) >= 0)
			return false;
		if (b.Find("Asdwegewr") >= 0)
			return false;
		if (a.Find("This") != 0)
			return false;
		if (a.Find("test") != 10)
			return false;
		if (a.Find("string") != 15)
			return false;
		if (a.Find("is") != 2)
			return false;
		if (a.RFind("is") != 5)
			return false;
		if (a.RFind("string") != 15)
			return false;
		if (a.RFind(NULL) >= 0)
			return false;
	}
	if (GString::RefStrCount != 0) return false;
	
	// Substring testing
	{
		GString a("This is a test string");
		GString b;
		b = a(5);
		if (_stricmp(b, "i"))
			return false;
		b = a(-1);
		if (_stricmp(b, "g"))
			return false;
		b = a(5, 7);
		if (_stricmp(b, "is"))
			return false;
	}
	if (GString::RefStrCount != 0) return false;
	
	// Whitespace strip testing
	{
		GString a("  \ta test. \t\r\n");
		if (_stricmp(a.Strip(), "a test."))
			return false;
		if (_stricmp(a.LStrip(), "a test. \t\r\n"))
			return false;
		if (_stricmp(a.RStrip(), "  \ta test."))
			return false;
	}
	if (GString::RefStrCount != 0) return false;

	return true;
}
