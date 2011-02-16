#ifdef WIN32
#define _WIN32_WINNT 0x0400
#include "windows.h"
#endif
#include "ntlm.h"

int GenerateRandom(uint8 *ptr, int len)
{
	int Status = 0;

	#ifdef WIN32

	typedef BOOLEAN (APIENTRY *RtlGenRandom)(void*, ULONG);
	HCRYPTPROV	phProv = 0;
	HMODULE		hADVAPI32 = 0;
	RtlGenRandom GenRandom = 0;
	if (!CryptAcquireContext(&phProv, 0, 0, PROV_RSA_FULL, 0))
	{
		// f***ing windows... try a different strategy.
		hADVAPI32 = LoadLibrary("ADVAPI32.DLL");
		if (hADVAPI32)
		{
			GenRandom = (RtlGenRandom) GetProcAddress(hADVAPI32, "SystemFunction036");
		}
	}			

	if (phProv)
	{
		if (CryptGenRandom(phProv, len, (uint8*)ptr))
			Status = len;
	}
	else if (GenRandom)
	{
		if (GenRandom(ptr, len))
			Status = len;
	}

	if (phProv)
		CryptReleaseContext(phProv, 0);
	else if (hADVAPI32)
		FreeLibrary(hADVAPI32);

	#else
	#error "Impl me."
	#endif

	return Status;
}