#ifndef _GREGION_CLIP_H_
#define _GREGION_CLIP_H_

class GRegionClipDC : public GSurface
{
	GRegion c;
	GSurface *s;
	
public:
	GRegionClipDC(GSurface *Surface, GRegion *Rgn = NULL)
	{
		s = Surface;
		pMem = s->pMem;
		if (Rgn)
			c = *Rgn;
	}
	
	~GRegionClipDC()
	{
		pMem = NULL;
	}
	
	void SetClipRegion(GRegion *Rgn)
	{
		if (Rgn)
			c = *Rgn;
		else
			c.Empty();
	}
};

#endif