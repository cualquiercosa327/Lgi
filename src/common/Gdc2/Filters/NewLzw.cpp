#include <stdio.h>
#include "Gdc2.h"
#include "Lzw.h"

#define MAX_TABLE_SIZE			18041

class LzwPrivate
{
public:
	int Bits;
	int HashingShift()
	{
		return Bits - 8;
	}
	
	int MaxValue()
	{
		return (1 << Bits) - 1;
	}
	
	int MaxCode()
	{
		return MaxValue() - 1;
	}
	
	int ClearCode()
	{
		return 256;
	}
	
	int EndOfFile()
	{
		return ClearCode() + 1;
	}
	
	int StartCode()
	{
		return ClearCode() + 2;
	}
	
	int TableSize()
	{
		switch (Bits)
		{
			case 8:
				return 293;
			case 9:
				return 599;
			case 10:
				return 1097;
			case 11:
				return 2099;
			case 12:
				return 5021;
			case 13:
				return 9029;
			case 14:
				return 18041;
		}

		LgiAssert(0);
		return 0;
	}

	int *code_value;
	uint *prefix_code;
	uchar *append_character;
	uchar decode_stack[4000];
	int input_bit_count;
	ulong input_bit_buffer;
	int output_bit_count;
	ulong output_bit_buffer;
	int next_code;

	LzwPrivate()
	{
		code_value = new int[MAX_TABLE_SIZE];
		prefix_code = new uint[MAX_TABLE_SIZE];
		append_character = new uchar[MAX_TABLE_SIZE];

		SetBits(9);
		Reset();
	}

	~LzwPrivate()
	{
		DeleteArray(code_value);
		DeleteArray(prefix_code);
		DeleteArray(append_character);
	}

	void SetBits(int b)
	{
		Bits = b;

		for (int i=0; i<256; i++)
		{
			prefix_code[i] = i;
		}
	}

	void Reset()
	{
		input_bit_count = 0;
		input_bit_buffer = 0;
		output_bit_count = 0;
		output_bit_buffer = 0;
		next_code = StartCode();
	}
};

Lzw::Lzw()
{
	d = new LzwPrivate;
}

Lzw::~Lzw()
{
	DeleteObj(d);
}

inline int Get1(GStream *i)
{
	uchar c = 0;
	i->Read(&c, sizeof(c));
	return c;
}

inline void Put1(char i, GStream *o)
{
	o->Write(&i, 1);
}

bool Lzw::Compress(GStream *output, GStream *input)
{
	uint next_code = d->StartCode();  /* Next code is the next available string code*/
	uint character;
	uint string_code;
	uint index;
	int i;

	d->SetBits(d->Bits);

	for (i = 0; i < d->TableSize(); i++)  /* Clear out the string table before starting */
	{
		d->code_value[i] = -1;
	}

	string_code = Get1(input);    /* Get the first code                         */

	/*
	** This is the main loop where it all happens.  This loop runs util all of
	** the input has been exhausted.  Note that it stops adding codes to the
	** table after all of the possible codes have been defined.
	*/
	while ((character = Get1(input)) != (unsigned)EOF)
	{
		index = find_match(string_code,character);/* See if the string is in */
		if (d->code_value[index] != -1)            /* the table.  If it is,   */
			string_code = d->code_value[index];        /* get the code value.  If */
		else                                    /* the string is not in the*/
		{                                       /* table, try to add it.   */
			if (next_code <= d->MaxCode())
			{
				d->code_value[index] = next_code++;
				d->prefix_code[index] = string_code;
				d->append_character[index] = character;
			}
			output_code(output,string_code);  /* When a string is found  */
			string_code=character;            /* that is not in the table*/
		}                                   /* I output the last string*/
	}                                     /* after adding the new one*/
	/*
	** End of the main loop.
	*/
	output_code(output, string_code); /* Output the last code               */
	output_code(output, d->MaxValue());   /* Output the end of buffer code      */
	output_code(output, 0);           /* This code flushes the output buffer*/

	return true;
}

/*
** This is the hashing routine.  It tries to find a match for the prefix+char
** string in the string table.  If it finds it, the index is returned.  If
** the string is not found, the first available index in the string table is
** returned instead.
*/
int Lzw::find_match(int hash_prefix, uint hash_character)
{
	int index;
	int offset;

	index = (hash_character << d->HashingShift()) ^ hash_prefix;
	if (index == 0)
		offset = 1;
	else
		offset = d->TableSize() - index;
	
	while (1)
	{
		if (d->code_value[index] == -1)
			return(index);
		
		if (d->prefix_code[index] == hash_prefix && d->append_character[index] == hash_character)
			return(index);
		
		index -= offset;
		
		if (index < 0)
			index += d->TableSize();
	}
}

/*
**  This is the expansion routine.  It takes an LZW format file, and expands
**  it to an output file.  The code here should be a fairly close match to
**  the algorithm in the accompanying article.
*/
bool Lzw::Decompress(GStream *output, GStream *input)
{
	uint new_code;
	uint old_code = 0;
	int character = 0; // first character of the last string
	uchar *string;

	d->SetBits(d->Bits);
	d->Reset();

	/*
	**  This is the main expansion loop.  It reads in characters from the LZW file
	**  until it sees the special code used to inidicate the end of the data.
	*/
	while ((new_code = input_code(input)) != d->EndOfFile())
	{
		if (new_code == d->ClearCode())
		{
			d->SetBits(9);
			new_code = input_code(input);
			if (new_code == d->EndOfFile())
			{
				break;
			}
			string = decode_string(d->decode_stack, new_code);
			if (!string)
			{
				return false;
			}

			character = *string;
			while (string >= d->decode_stack)
			{
				Put1(*string--, output);
			}
		}
		else
		{
			if (new_code >= d->next_code)
			{
				/*
				** This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING
				** case which generates an undefined code.  It handles it by decoding
				** the last code, and adding a single character to the end of the decode string.
				*/
				d->decode_stack[0] = character;
				string = decode_string(d->decode_stack+1, old_code);
			}
			else
			{
				/*
				** Otherwise we do a straight decode of the new code.
				*/
				string = decode_string(d->decode_stack, new_code);
			}

			if (!string)
			{
				return false;
			}
			
			// Now we output the decoded string in reverse order.
			character = *string;
			while (string >= d->decode_stack)
			{
				Put1(*string--, output);
			}

			// Finally, if possible, add a new code to the string table.
			if (d->next_code <= d->MaxCode())
			{
				d->prefix_code[d->next_code] = old_code;
				d->append_character[d->next_code] = character;

				d->next_code++;

				switch (d->next_code)
				{
					case 511:
					case 1023:
					case 2047:
					{
						d->SetBits(d->Bits+1);
						break;
					}
					case 4095:
					{
						d->SetBits(9);
						d->next_code = d->StartCode();
						break;
					}
				}
			}
		}

		old_code = new_code;
	}

	if (new_code != d->EndOfFile())
	{
		return false;
	}

	return true;
}

/*
** This routine simply decodes a string from the string table, storing
** it in a buffer.  The buffer can then be output in reverse order by
** the expansion program.
*/

uchar *Lzw::decode_string(unsigned char *buffer, unsigned int code)
{
	int i;

	i=0;
	while (code > 255)
	{
		*buffer++ = d->append_character[code];
		code = d->prefix_code[code];
		if (code < 0 ||
			code >= d->next_code ||
			code == 256 ||
			code == 257 ||
			i++ >= 4094)
		{
			_asm int 3
			printf("Fatal error during code expansion.\n");
			return 0;
		}
	}
	
	*buffer = code;
	
	return buffer;
}

/*
** The following two routines are used to output variable length
** codes.  They are written strictly for clarity, and are not
** particularly efficient.
*/
int Lzw::input_code(GStream *input)
{
	while (d->input_bit_count <= 24)
	{
		ulong Byte = Get1(input);
		d->input_bit_buffer |= Byte << (24 - d->input_bit_count);
		d->input_bit_count += 8;
	}
	
	uint return_value = d->input_bit_buffer >> (32-d->Bits);
	
	d->input_bit_buffer <<= d->Bits;
	d->input_bit_count -= d->Bits;
	
	return return_value;
}

void Lzw::output_code(GStream *output, uint code)
{
	d->output_bit_buffer |= (unsigned long) code << (32-d->Bits-d->output_bit_count);
	d->output_bit_count += d->Bits;
	while (d->output_bit_count >= 8)
	{
		Put1(d->output_bit_buffer >> 24,output);
		d->output_bit_buffer <<= 8;
		d->output_bit_count -= 8;
	}
}


void Lzw::SetBufSize(int i)
{
	d->SetBits(i + 1);
}
