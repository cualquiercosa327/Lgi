#include "Lgi.h"
#include "UnitTests.h"
#include "GMatrix.h"

class GMatrixTestPriv
{
public:
	bool Error(char *Fmt, ...)
	{
		va_list Args;
		va_start(Args, Fmt);
		vprintf(Fmt, Args);
		va_end(Args);
		return false;
	}

	bool Test1()
	{
		GMatrix<double, 3, 3> m1;
		m1.SetIdentity();
		for (int y=0; y<m1.Y(); y++)
		{
			for (int x=0; x<m1.X(); x++)
			{
				if (m1.m[y][x] != (x == y ? 1.0 : 0.0))
					return false;
			}
		}
		
		GMatrix<double, 3, 2> a;
		a.SetStr(	"1 2 3\n"
					"4 5 6\n");
		GMatrix<double, 2, 3> b;
		b.SetStr(	"7 8\n"
					"9 10\n"
					"11 12\n");
		GMatrix<double, 2, 2> c, r;
		c = a * b;
		r.SetStr(	"58 64\n"
					"139 154\n");
		if (c != r)
			return false;
	
		return true;
	}
};

GMatrixTest::GMatrixTest() : UnitTest("GMatrixTest")
{
	d = new GMatrixTestPriv;
}

GMatrixTest::~GMatrixTest()
{
	DeleteObj(d);
}

bool GMatrixTest::Run()
{
	return	d->Test1();
}