#include "Lgi.h"
#include "UnitTests.h"

void UnitTests()
{
	GArray<UnitTest*> Tests;

	#if 0
	Tests.Add(new GAutoPtrTest);
	Tests.Add(new GCssTest);
	Tests.Add(new GMatrixTest);
	Tests.Add(new GContainers);
	Tests.Add(new GStringClassTest);
	Tests.Add(new LDateTimeTest);
	#endif
	Tests.Add(new GStringPipeTest);

	for (int i=0; i<Tests.Length(); i++)
	{
		bool b = Tests[i]->Run();
		if (b)
		{
			printf("Passed: %s\n", Tests[i]->Name());
		}
		else
		{
			printf("Error: %s test failed.\n", Tests[i]->Name());
			break;
		}
	}

	Tests.DeleteObjects();
}

int main(int args, char **arg)
{
	OsAppArguments Args(args, arg);
	GApp a(Args, "LgiUnitTests");
	if (a.IsOk())
	{
		UnitTests();
	}
	LgiSleep(2000);
	return 0;
}