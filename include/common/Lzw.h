#ifndef _LZW_H_
#define _LZW_H_

class Lzw
{
	class LzwPrivate *d;

	int find_match(int hash_prefix, uint hash_character);
	void output_code(GStream *output, uint code);
	int input_code(GStream *input);
	uchar *decode_string(unsigned char *buffer, unsigned int code);
	
public:
	int MeterBlock;
	Progress *Meter;

	Lzw();
	virtual ~Lzw();

	void SetBufSize(int i);
	bool Decompress(GStream *out, GStream *in);
	bool Compress(GStream *out, GStream *in);
};

#endif