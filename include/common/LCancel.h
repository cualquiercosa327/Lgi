//
//  LCancel.h
//  Lgi
//
//  Created by Matthew Allen on 19/10/2017.
//
//

#ifndef LCancel_h
#define LCancel_h

class LgiClass LCancel
{
	bool Cancelled;
	
public:
	LCancel()
	{
		Cancelled = false;
	}
	
	virtual ~LCancel() {}
	virtual bool IsCancelled() { return Cancelled; }
	virtual void Cancel(bool b) { Cancelled = b; }
};


#endif /* LCancel_h */
