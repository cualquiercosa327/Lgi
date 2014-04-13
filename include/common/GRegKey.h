#if !defined(_GREGKEY_H_) && defined(WIN32)
#define _GREGKEY_H_

/// Registry access class
#include "GContainers.h"

class LgiClass GRegKey
{
	HKEY k, Root;
	char s[256];
	char *KeyName;

public:
	static bool AssertOnError;

	/// Constructor
	GRegKey
	(
		/// The access type required
		bool WriteAccess,
		/// The key name: you can use printf style syntax and extra arguments
		char *Key,
		...
	);
	~GRegKey();

	/// Returns true if the key was openned
	bool IsOk();
	/// Creates the key if not present
	bool Create();
	/// Returns the key name
	char *Name();

	/// Return a string value
	char *GetStr
	(
		/// Name of the subkey or NULL for the default string.
		const char *Name = 0
	);
	/// Sets a string value
	bool SetStr(const char *Name, const char *Value);
	/// Get an int value
	int GetInt(char *Name = 0);
	/// Set an int value
	bool SetInt(char *Name, int Value);
	/// Get a binary value
	bool GetBinary(char *Name, void *&Ptr, int &Len);
	/// Set a binary value
	bool SetBinary(char *Name, void *Ptr, int Len);
	/// Delete a value
	bool DeleteValue(char *Name = 0);
	/// Delete a key
	bool DeleteKey();

	/// List all the key names under this key
	bool GetKeyNames(List<char> &n);
	/// List all the key value name under this key
	bool GetValueNames(List<char> &n);
};

#endif