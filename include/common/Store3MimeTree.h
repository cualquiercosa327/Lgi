#ifndef _STORE3_MIME_TREE_H_
#define _STORE3_MIME_TREE_H_

/*
This class can build a mime type from segments.

Example valid mime trees:

	text/plain

	text/html

	multipart/alternative
		text/plain
		text/html

	multipart/mixed
		text/plain
		application/pdf
		application/octet-stream

	multipart/mixed
		multipart/alternative
			text/plain
			text/html
		application/octet-stream

	multipart/mixed
		multipart/alternative
			text/plain
			multipart/related
				text/html
				image/jpeg
				image/png
		application/octet-stream

	multipart/related
		text/html
		image/jpeg
		image/png

*/

template <typename TStore, typename TMail, typename TAttachment>
class Store3MimeTree
{
	bool InRelated;
	TMail *Mail;
	TStore *Store;

public:
	TAttachment *&Root;
	TAttachment *MsgText;
	TAttachment *MsgHtml;
	GArray<TAttachment*> MsgHtmlRelated;
	TAttachment *Related;
	TAttachment *Alternative;
	TAttachment *Mixed;
	GArray<TAttachment*> Attachments;
	GArray<TAttachment*> Unknown;
	
	Store3MimeTree(TMail *mail, TAttachment *&root) : Root(root)
	{
		Mail = mail;
		Store = dynamic_cast<TStore*>(Mail->GetStore());
		MsgText = NULL;
		MsgHtml = NULL;
		Related = NULL;
		Alternative = NULL;
		Mixed = NULL;
		InRelated = false;

		if (Root)
			Scan(Root);
	}
	
	void Scan(TAttachment *Seg)
	{
		if (!Root)
			Root = Seg;		
		
		bool PrevInRelated = InRelated;
		Add(Seg);
		
		GDataIt It = Seg->GetList(FIELD_MIME_SEG);
		if (It)
		{
			for (GDataPropI *i=It->First(); i; i=It->Next())
			{
				TAttachment *a = dynamic_cast<TAttachment*>(i);
				if (!a) continue;
				Scan(a);
			}
		}
		
		InRelated = PrevInRelated;
	}
	
	void Add(TAttachment *Seg)
	{
		if (Seg->IsMixed())
		{
			if (Mixed)
				Unknown.Add(Seg);
			else
				Mixed = Seg;
		}
		else if (Seg->IsAlternative())
		{
			if (Alternative)
				Unknown.Add(Seg);
			else
				Alternative = Seg;				
		}
		else if (Seg->IsRelated())
		{
			if (Related)
				Unknown.Add(Seg);
			else
				Related = Seg;
			InRelated = true;					
		}
		else if (Seg->IsPlainText())
		{
			if (MsgText)
				Attachments.Add(Seg);
			else
				MsgText = Seg;
		}
		else if (Seg->IsHtml())
		{
			if (MsgHtml)
				Attachments.Add(Seg);
			else
				MsgHtml = Seg;
		}
		else if (Seg->IsMultipart())
		{
			Unknown.Add(Seg);
		}
		else
		{
			if (InRelated)
				MsgHtmlRelated.Add(Seg);
			else
				Attachments.Add(Seg);
		}
	}

	bool Build()
	{
		if (Attachments.Length() > 0)
		{
			// We must have multipart/mixed at the root
			if (!Root || !Root->IsMixed())
			{
				if (Mixed)
				{
					// hmmmm, an existing mixed but not root?
					Mixed->Detach();
					Root = Mixed;
				}
				else
				{
					// Create new mixed root
					Root = new TAttachment(Store);
					if (!Root)
						return false;
					Mixed = Root;
					Root->SetStr(FIELD_MIME_TYPE, sMultipartMixed);
				}
			}
			
			if (Mixed)
			{
				LgiAssert(Mixed->IsMixed());
				
				// Add all our attachments now
				for (unsigned i=0; i<Attachments.Length(); i++)
					Attachments[i]->AttachTo(Mixed);
			}
		}
		
		TAttachment *Html = NULL;
		if (MsgHtml)
		{
			if (MsgHtmlRelated.Length() > 0)
			{
				// Create the HTML related tree if needed
				if (!Related)
				{
					Related = new TAttachment(Store);
					if (!Related)
						return false;
					Related->SetStr(FIELD_MIME_TYPE, sMultipartRelated);
				}

				MsgHtml->AttachTo(Related);
				for (unsigned i=0; i<MsgHtmlRelated.Length(); i++)
					MsgHtmlRelated[i]->AttachTo(Related);
				
				Html = Related;
			}
			else
			{
				Html = MsgHtml;
			}
		}		

		if (Html && MsgText)
		{
			// We need an alternative
			if (!Alternative)
			{
				Alternative = new TAttachment(Store);
				if (!Alternative)
					return false;
				Alternative->SetStr(FIELD_MIME_TYPE, sMultipartAlternative);
			}
			
			Html->AttachTo(Alternative);
			MsgText->AttachTo(Alternative);
			if (Root && Root->IsMultipart())
				Alternative->AttachTo(Root);
			else
				Root = Alternative;
		}
		else if (MsgText)
		{
			if (Root && Root->IsMultipart())
				MsgText->AttachTo(Root);
			else
				Root = MsgText;
		}
		else if (Html)
		{
			if (Root && Root->IsMultipart())
				Html->AttachTo(Root);
			else
				Root = Html;
		}
		
		if (Root)
			Root->AttachTo(Mail);
		else
			return false;
		
		// What to do with the Unknown segments?
		for (unsigned i=0; i<Unknown.Length(); i++)
		{
			// I know... kill them!!!
			Unknown[i]->Detach();
			DeleteObj(Unknown[i]);
		}
		
		return true;
	}	

	#ifdef _DEBUG
	bool UnitTests(TStore *Store, TMail *Mail)
	{
		const char *Tests[] = {
			"text/plain",
			
			"text/html",
			
			"multipart/alternative\n"
			"	text/plain\n"
			"	text/html\n",
			
			"multipart/mixed\n"
			"	text/plain\n"
			"	application/pdf\n"
			"	application/octet-stream\n",
			
			"multipart/mixed\n"
			"	multipart/alternative\n"
			"		text/plain\n"
			"		text/html\n"
			"	application/octet-stream\n",
			
			"multipart/mixed\n"
			"	multipart/alternative\n"
			"		text/plain\n"
			"		multipart/related\n"
			"			text/html\n"
			"			image/jpeg\n"
			"			image/png\n"
			"	application/octet-stream\n",
			
			"multipart/related\n"
			"	text/html\n"
			"	image/jpeg\n"
			"	image/png\n"
		};
		
		for (unsigned int i=0; i<CountOf(Tests); i++)
		{
			TAttachment *Root = NULL;

			Store3MimeTree<TStore, TMail, TAttachment> t(Mail, Root);
			
			GString Src = Tests[i];
			GString::Array Lines = Src.SplitDelimit(" \t\r\n");
			for (unsigned ln = 0; ln < Lines.Length(); ln++)
			{
				TAttachment *a = new TAttachment(Store);
				if (!a)
				{
					LgiTrace("%s:%i - Alloc err\n", _FL);
					return false;
				}
				a->SetStr(FIELD_MIME_TYPE, Lines[ln]);

				if (Lines[ln].Find("multipart") >= 0)
				{ DeleteObj(a); }// no-op
				else if (Lines[ln].Find("application/") >= 0)
					t.Attachments.Add(a);
				else if (Lines[ln].Find("html") >= 0)
					t.MsgHtml = a;
				else if (Lines[ln].Find("plain") >= 0)
					t.MsgText = a;
				else if (Lines[ln].Find("image") >= 0)
					t.MsgHtmlRelated.Add(a);
				else
					DeleteObj(a)
			}			
			
			if (!t.Build())
			{
				LgiTrace("%s:%i - Failed to build tree for test %i:\n%s\n", _FL, i, Tests[i]);
				return false;
			}

			for (unsigned n = 0; n < Lines.Length(); n++)
			{
				GArray<TAttachment*> Results;
				if (!Root->FindSegs(Lines[n], Results))
				{
					LgiTrace("%s:%i - Failed to find '%s' in test %i:\n%s\n", _FL, Lines[n].Get(), i, Tests[i]);
					return false;
				}
			}
		}

		return true;
	}
	#endif
};

#endif // _STORE3_MIME_TREE_H_