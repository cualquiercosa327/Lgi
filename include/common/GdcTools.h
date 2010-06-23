/// \file
/// \author Matthew Allen

#ifndef _DC_TOOLS_H_
#define _DC_TOOLS_H_

/// Remap surface to new palette
extern bool RemapDC(GSurface *pDC, GPalette *DestPal);

/// Flip surface on the X axis
#define		FLIP_X					1
/// Flip surface on the Y axis
#define		FLIP_Y					2
/// Flip surface on an axis
extern bool FlipDC
(
	/// The input surface
	GSurface *pDC,
	/// The axis, one of #FLIP_X or #FLIP_Y
	int Dir
);

/// Return true if surface is greyscale
extern bool IsGreyScale(GSurface *pDC);

/// Convert surface to greyscale
extern GSurface *GreyScaleDC(GSurface *pDC);

/// Invert the surface
extern bool InvertDC(GSurface *pDC);

/// Rotate the surface
extern bool RotateDC(GSurface *pDC, double Angle);

/// Flip surface on the x axis
extern bool FlipXDC(GSurface *pDC);
/// Flip surface on the y axis
extern bool FlipYDC(GSurface *pDC);

/// Resample the surface to a new size
extern bool ResampleDC(GSurface *pTo, GSurface *pFrom, Progress *Prog = 0);

#endif
