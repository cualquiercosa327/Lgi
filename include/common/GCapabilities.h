/// \file
/// \author Matthew Allen <fret@memecode.com>
#ifndef _GCAPABILITIES_H
#define _GCAPABILITIES_H
#ifdef __cplusplus

class GCapabilityTarget;

/// This class is a parent for objects that need external dependencies 
/// to be installed by an external source.
class LgiClass GCapabilityClient
{
    friend class GCapabilityTarget;
    GArray<GCapabilityTarget*> Targets;

protected:
	/// Call this when you need to fulfill an external dependency.
    bool NeedsCapability(char *Name);

public:
    virtual ~GCapabilityClient();
    
    /// Call this to register a target that can install dependencies.
    void Register(GCapabilityTarget *t);
};

class LgiClass GCapabilityTarget
{
    friend class GCapabilityClient;
    GArray<GCapabilityClient*> Clients;

public:
    virtual ~GCapabilityTarget();
    
    /// This is called to install a dependency.
    virtual bool NeedsCapability(char *Name) = 0;
};

#endif
#endif