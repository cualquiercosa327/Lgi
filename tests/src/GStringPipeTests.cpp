#include "Lgi.h"
#include "UnitTests.h"
#include "GStringClass.h"

class GStringPipeTestPriv
{
public:
};

GStringPipeTest::GStringPipeTest() : UnitTest("GStringPipeTest")
{
	d = new GStringPipeTestPriv;
}

GStringPipeTest::~GStringPipeTest()
{
	DeleteObj(d);
}

bool GStringPipeTest::Run()
{


	return true;
}
