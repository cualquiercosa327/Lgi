/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef _GCAPABILITIES_H
#define _GCAPABILITIES_H
#ifdef __cplusplus

#include "LHashTable.h"

#define DEBUG_CAPABILITIES		0

class GCapabilityTarget;

/// This class is a parent for objects that need external dependencies 
/// to be installed by an external source.
class LgiClass GCapabilityClient
{
	friend class GCapabilityTarget;
	GArray<GCapabilityTarget*> Targets;

public:
	virtual ~GCapabilityClient();
	
	/// Call this when you need to fulfill an external dependency.
	bool NeedsCapability(const char *Name, const char *Param = NULL);

	/// Call this to register a target that can install dependencies.
	void Register(GCapabilityTarget *t);
};

class LgiClass GCapabilityTarget
{
	friend class GCapabilityClient;
	GArray<GCapabilityClient*> Clients;

public:
	LHashTbl<ConstStrKey<char,false>, bool> Map;
	typedef LHashTbl<ConstStrKey<char,false>, bool> CapsHash;

	virtual ~GCapabilityTarget();
	
	/// This is called to install a dependency.
	virtual bool NeedsCapability(const char *Name, const char *Param) = 0;
	
	/// This is called after the dependancy is installed.
	virtual void OnInstall(CapsHash *Caps, bool Status) = 0;

	/// This is called if the user closes the capability bar...
	virtual void OnCloseInstaller() = 0;
};

#endif
#endif