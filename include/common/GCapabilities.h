#ifndef _GCAPABILITIES_H
#define _GCAPABILITIES_H
#ifdef __cplusplus

class GCapabilityTarget;
class LgiClass GCapabilityClient
{
    friend class GCapabilityTarget;
    GArray<GCapabilityTarget*> Targets;

protected:
    bool NeedsCapability(char *Name);

public:
    virtual ~GCapabilityClient();
    void Register(GCapabilityTarget *t);
};

class LgiClass GCapabilityTarget
{
    friend class GCapabilityClient;
    GArray<GCapabilityClient*> Clients;

public:
    virtual ~GCapabilityTarget();    
    virtual bool NeedsCapability(char *Name) = 0;
};

#endif
#endif