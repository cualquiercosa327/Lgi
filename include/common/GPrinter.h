/**
	\file
	\author Matthew Allen
*/

#ifndef _GPRINTER_H_
#define _GPRINTER_H_

/// Class to connect to a printer and start printing pages.
class LgiClass GPrinter
{
	class GPrinterPrivate *d;

public:
	GPrinter();
	virtual ~GPrinter();

	/// Set number of pages in document (if known)
	void SetPages(int p);

	/// Get the number of pages in the document
	int GetPages();

	/// Get the range of pages to print that was selected by the user
	bool GetPageRange
	(
		/// You'll get pairs of ints, each pair is the start and end page
		/// number of a range to print. i.e. [5, 10], [14, 16]
		GArray<int> &p
	);

	/// Start a print job
	GPrintDC *StartDC(const char *PrintJobName, GView *Parent = 0);

	/// Browse to a printer
	bool Browse(GView *Parent);
	
	/// Write the user selected printer to a string for storage
	bool Serialize(char *&Str, bool Write);
};

#endif
