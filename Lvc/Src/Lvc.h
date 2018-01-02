#ifndef _Lvc_h_
#define _Lvc_h_

#include "Lgi.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"
#include "GOptionsFile.h"

#define OPT_Folders	"Folders"
#define OPT_Folder	"Folder"

extern const char *AppName;

enum VersionCtrl
{
	VcNone,
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
};

struct AppPriv
{
	GTree *Tree;
	LList *Lst;
	GOptionsFile Opts;

	AppPriv()  : Opts(GOptionsFile::PortableMode, AppName)
	{
		Lst = NULL;
		Tree = NULL;
	}
};

extern VersionCtrl DetectVcs(const char *Path);

#include "VcCommit.h"
#include "VcFolder.h"

#endif