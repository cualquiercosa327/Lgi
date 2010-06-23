/// \file
/// \author Mathew Allen (fret@memecode.com)

#ifndef __GABOUT_H
#define __GABOUT_H

#include "GDocView.h"

/// A simple about dialog
class GAbout : public GDialog, public GDefaultDocumentEnv
{
public:
	/// Constructor
	GAbout
	(
		/// The parent window
		GView *parent,
		/// The application name
		char *AppName,
		/// The version
		double Ver,
		/// The description of the application
		char *Text,
		/// The filename of a graphic to display
		char *AboutGraphic,
		/// URL for the app
		char *Url,
		/// Support email addr for the app
		char *Email
	);

	int OnNotify(GViewI *Ctrl, int Flags);
};

#endif
