#ifndef _GMIDI_H_
#define _GMIDI_H_

#if defined(WIN32)
	#if _MSC_VER >= 1400
	typedef DWORD_PTR MIDI_TYPE;
	#else
	typedef DWORD MIDI_TYPE;
	#endif
#endif

class GMidi : public GMutex
{
	GArray<GAutoString> In, Out;
	struct GMidiPriv *d;

protected:
	// Lock the object before accessing this
	GArray<uint8> MidiIn;

public:	
	GMidi();
	~GMidi();
	
	virtual GStream *GetLog() { return NULL; }

	bool IsMidiOpen();
	void Connect(int InIdx, int OutIdx);
	void SendMidi(uint8 *ptr, int len, bool quiet);
	void CloseMidi();

	virtual void OnMidiIn(uint8 *midi, int midi_len);
	virtual void OnMidiOut(uint8 *p, int len);

	#ifdef WIN32
	void StoreMidi(uint8 *ptr, int len);
	#endif
};

#endif