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

		if (MsgHtml && MsgText)
		{
			// We need an alternative
			if (!Alternative)
			{
				Alternative = new TAttachment(Store);
				if (!Alternative)
					return false;
				Alternative->SetStr(FIELD_MIME_TYPE, sMultipartAlternative);
			}
			/*
			if (Alternative != Root)
			{
				if (Root && Root->IsMultipart())
					Alternative->AttachTo(Root);
				else
					Root = Alternative;
			}
			*/
			
			MsgHtml->AttachTo(Alternative);
			MsgText->AttachTo(Alternative);
		}
		else if (MsgText)
		{
			if (Root && Root->IsMultipart())
				MsgText->AttachTo(Root);
			else
				Root = MsgText;
		}
		
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

				if (Alternative)
					// MsgHtml is already attach to the alternative seg
					Alternative->AttachTo(Related);
				else
					MsgHtml->AttachTo(Related);
					
				if (Root && Root->IsMultipart())
					Related->AttachTo(Root);
				else
					Root = Related;
				
				
				for (unsigned i=0; i<MsgHtmlRelated.Length(); i++)
					MsgHtmlRelated[i]->AttachTo(Related);
			}
			else
			{
				if (Alternative)
					Root = Alternative; // MsgHtml is already attach to the alternative seg
				else if (Root && Root->IsMultipart())
					MsgHtml->AttachTo(Root);
				else
					Root = MsgHtml;					
			}
		}
		
		if (Root)
			Root->AttachTo(Mail);
		
		// What to do with the Unknown segments?
		for (unsigned i=0; i<Unknown.Length(); i++)
		{
			// I know... kill them!!!
			Unknown[i]->Detach();
			DeleteObj(Unknown[i]);
		}
		
		return true;
	}	
};

#endif // _STORE3_MIME_TREE_H_