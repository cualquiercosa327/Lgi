#ifndef _GMIDI_H_
#define _GMIDI_H_

#if defined(WINDOWS)
	#if _MSC_VER >= 1400
	typedef DWORD_PTR MIDI_TYPE;
	#else
	typedef DWORD MIDI_TYPE;
	#endif
    #include "mmsystem.h"
#elif defined(MAC)
    #include <CoreMidi/MIDIServices.h>
#endif

#define MIDI_SYSEX_START	0xf0
#define MIDI_SYSEX_END		0xf7

class GMidi : public GMutex
{
	struct GMidiPriv *d;

	void OnError(char *Func, GAutoString *Error, uint32 Code, char *File, int Line);

	#ifdef WIN32
	friend class GMidiNotifyWnd;
	friend void CALLBACK MidiInProc(HMIDIIN hmi, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE dwParam1, MIDI_TYPE dwParam2);
	void StoreMidi(uint8 *ptr, int len);
	void ParseMidi();
	#endif

protected:
	// Lock the object before accessing this
	GArray<uint8> MidiIn;

	// Arrays of device names
	GArray<GAutoString> In, Out;
    
    #ifdef MAC
    friend void MidiNotify(const MIDINotification *message, void *refCon);
    virtual void OnMidiNotify(const MIDINotification *message);
    #endif

public:	
	GMidi();
	~GMidi();

	static int GetMidiPacketSize(uint8 *ptr, int len);
	
	virtual GStream *GetLog() { return NULL; }

	bool IsMidiOpen();
	bool Connect(int InIdx, int OutIdx, GAutoString *ErrorMsg = NULL);
	void SendMidi(uint8 *ptr, int len, bool quiet);
	void CloseMidi();

	virtual void OnMidiIn(uint8 *midi, int len);
	virtual void OnMidiOut(uint8 *midi, int len);
};

#endif