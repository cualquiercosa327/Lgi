#include "Lgi.h"
#include "UnitTests.h"

void UnitTests()
{
	GArray<UnitTest*> Tests;

	Tests.Add(new GAutoPtrTest);
	Tests.Add(new GCssTest);
	Tests.Add(new GContainers);

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
	GApp a("application/x-lgi-unit-tests", Args);
	if (a.IsOk())
	{
		UnitTests();
	}
	LgiSleep(2000);
	return 0;
}