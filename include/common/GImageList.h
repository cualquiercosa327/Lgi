#ifndef _IMAGE_LIST_H_
#define _IMAGE_LIST_H_

////////////////////////////////////////////////////////////////////////

/// GImageList::Draw() flag: Draw as selected
#define IMGLST_SELECTED				0x0001
/// GImageList::Draw() flag: Draw disabled
#define IMGLST_DISABLED				0x0002
/// GImageList::Draw() flag: Draw with GDC
#define IMGLST_GDC					0x0004

/// \brief A list of images/icons all the same size
///
/// Currently the image list treats the pixel at (0,0) as the transparent colour key.
class LgiClass GImageList : public GMemDC
{
	class GImageListPriv *d;

public:
	/// Create the image list
	GImageList
	(
		/// The width of each tile
		int TileX,
		/// The height of each tile
		int TileY,
		/// Initial data for the images
		GSurface *pDC = NULL
	);
	
	~GImageList();

	/// Returns the width of each image
	int TileX();
	/// Returns the height of each image
	int TileY();
	/// Gets the number of images in the list
	int GetItems();
	/// Finds the bounds of valid data for each image and returns it in an array
	/// valid from 0 to GetItems()-1.
	GRect *GetBounds();

	/// Notifies the image list that it's image data has changed and it should flush any
	/// cached info about the images
	void Update(int Flags);
	/// Draw an image onto a graphics surface
	void Draw
	(
		/// The output surface
		GSurface *pDest,
		/// The x coord to draw the top-left corner
		int x,
		/// The y coord to draw the top-left corner
		int y,
		/// A 0 based index into the list to draw
		int Image,
		/// The background colour if not alpha compositing is available
		GColour Background,
		/// Drawing options
		/// \sa The defines starting at IMGLST_SELECTED in GToolBar.h
		int Flags = 0
	);
};

class LgiClass GImageListOwner
{
	bool OwnList;
	GImageList *ImageList;

public:
	GImageListOwner()
	{
		OwnList = true;
		ImageList = NULL;
	}
	
	~GImageListOwner()
	{
		if (OwnList)
			DeleteObj(ImageList);
	}

	GImageList *GetImageList() { return ImageList; }
	
	bool SetImageList(GImageList *List, bool Own = true)
	{
		if (OwnList)
			DeleteObj(ImageList);

		ImageList = List;
		OwnList = Own;
		AskImage(true);
		
		return ImageList != NULL;
	}
	
	bool LoadImageList(char *File, int x, int y)
	{
		GSurface *pDC = LoadDC(File);
		if (pDC)
		{
			ImageList = new GImageList(x, y, pDC);
			if (ImageList)
			{
				#ifdef WIN32
				ImageList->Create(pDC->X(), pDC->Y(), pDC->GetBits());
				#endif
			}
		}
		return pDC != 0;
	}

	virtual void AskImage(bool b) {}
};

/** \brief Loads an image list from a file

	This requires that somewhere in your project you have the image codec to load the type of 
	file you pass in. For instance if the file is a png, then include the Png.cpp file in your
	project. Uses LoadDC() internally.
*/
LgiFunc GImageList *LgiLoadImageList
(
	/// The file to load
	const char *File,
	/// The width of the icons
	int x,
	/// The height of the icons
	int y
);

#endif