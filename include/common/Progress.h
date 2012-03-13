/// \file
/// \author Matthew Allen
#ifndef __PROGRESS_H
#define __PROGRESS_H

#include "GSemaphore.h"

/// Generic progress class, keeps track of how far through a task you are.
class LgiClass Progress : public GSemaphore
{
protected:
	GAutoString Description;
	int64 Start;
	int64 Val;
	int64 Low, High;
	const char *Type;
	double Scale;
	bool Canceled;

public:
	uint64 UserData;
	
	Progress();
	Progress(char *desc, int64 l, int64 h, char *type = NULL, double scale = 1.0);
	virtual ~Progress() {}

	virtual char *GetDescription() { return Description; }
	virtual void SetDescription(const char *d = 0);
	virtual void GetLimits(int64 *l, int64 *h);
	virtual void SetLimits(int64 l, int64 h);
	virtual int64 Value() { return Val; }
	virtual void Value(int64 v) { Val = v; }
	virtual double GetScale() { return Scale; }
	virtual void SetScale(double s) { Scale = s; }
	virtual const char *GetType() { return Type; }
	virtual void SetType(const char *t) { Type = t; }
	
	virtual bool Cancel() { return Canceled; }
	virtual void Cancel(bool i) { Canceled = i; }
	virtual void SetParameter(int Which, int What) {}

	virtual Progress &operator =(Progress &p);
};


#endif
