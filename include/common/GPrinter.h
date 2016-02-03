/**
	\file
	\author Matthew Allen
*/

#ifndef _GPRINTER_H_
#define _GPRINTER_H_

class GPrintEvents
{
public:
	virtual ~GPrintEvents() {}

	/*
	/// Get the number of pages in the document
	int GetPages();

	/// Get the range of pages to print that was selected by the user
	bool GetPageRange
	(
		/// You'll get pairs of ints, each pair is the start and end page
		/// number of a range to print. i.e. [5, 10], [14, 16]
		GArray<int> &p
	);
	*/

	virtual int OnBeginPrint(GPrintDC *pDC) { return 1; }
	virtual bool OnPrintPage(GPrintDC *pDC, int PageIndex) = 0;
};

/// Class to connect to a printer and start printing pages.
class LgiClass GPrinter
{
	class GPrinterPrivate *d;

public:
	GPrinter();
	virtual ~GPrinter();

	/// Browse to a printer
	bool Browse(GView *Parent);

	/// Start a print job
	bool Print(GPrintEvents *Events, const char *PrintJobName, int Pages = -1, GView *Parent = NULL);
	
	/// Gets any available error message...
	GString GetErrorMsg();

	/// Write the user selected printer to a string for storage
	bool Serialize(char *&Str, bool Write);
};

#endif
