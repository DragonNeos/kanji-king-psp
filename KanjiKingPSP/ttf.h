#include <stdint.h>

#include <pspkernel.h>

#include <ft2build.h>
//#include FT_FREETYPE_H
//#include FT_GLYPH_H
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>


#include <malloc.h>

#include "utility.h"


//-------------------------------------------------------------------------------------------------
class file
{
	DISALLOW_COPY_AND_ASSIGN(file)

public:
//+------------------------------------------------------------------+
	const SceUID id;

//+------------------------------------------------------------------+
	file(const char *filename, int flags = PSP_O_RDONLY)
	:	id(sceIoOpen(filename, flags, 0777))
	{

	}

//+------------------------------------------------------------------+
	~file()
	{
		sceIoClose(id);
	}

//+------------------------------------------------------------------+
//	Returns the file length in bytes. Warning: seeks to the end.
	SceOff length() const
	{
		return sceIoLseek(id, 0, SEEK_END);
	}

//+------------------------------------------------------------------+
//	Read data from a certain offset.
//	@return The number of bytes read
	int read(void *data, SceSize size, SceOff start = 0) const
	{
		sceIoLseek(id, start, SEEK_SET);
		return sceIoRead(id, data, size);
	}

//+------------------------------------------------------------------+
//	Write data to memory stick.
//	@return The number of bytes written
	int write(void *data, SceSize size)
	{
		return sceIoWrite(id, data, size);
	}

};


//-------------------------------------------------------------------------------------------------
template<typename T>
class vec
{
	DISALLOW_COPY_AND_ASSIGN(vec)

public:
//+------------------------------------------------------------------+
	const size_t length;
	T *const data;

//+------------------------------------------------------------------+
	vec(size_t l, T *src = NULL)
	:	length	(l),
		data		((T*)malloc((length+1)*sizeof(T)))
	{
		if(data)
		{
			data[length] = 0;					// terminating NULL character
			if(src)
				memcpy(data, src, length*sizeof(T));
		}
	}

//+------------------------------------------------------------------+
	vec(const file &f)
	:	length	(f.length()+1),
		data		((T*)malloc((length+1)*sizeof(T)))
	{
		if(data)
		{
			f.read(data, length);
			data[length] = 0;					// terminating NULL character
		}
	}

//+------------------------------------------------------------------+
	~vec()
	{
		free(data);
	}

//+------------------------------------------------------------------+
	operator	const	T*	()	const				{ return data; }
	operator			T*	()						{ return data; }
	operator	bool		()	const				{ return data != NULL && length != 0; }

//+------------------------------------------------------------------+
	void zero()
	{
		memset(data, 0, length*sizeof(T));
	}
};

typedef vec<char> string;


//-------------------------------------------------------------------------------------------------
// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

class UTF8
{
	static const uint8_t utf8d[];

public:
	uint32_t state;
//+------------------------------------------------------------------+
	UTF8() : state(0)
	{}

//+------------------------------------------------------------------+
	inline uint32_t decode(uint32_t* codep, uint32_t byte)
	{
		uint32_t type = utf8d[byte];

		*codep = (state != UTF8_ACCEPT) ?
			(byte & 0x3fu) | (*codep << 6) :
			(0xff >> type) & (byte);

		state = utf8d[256 + state + type];
		return state;
	}

//+------------------------------------------------------------------+
	inline uint32_t decode(const char** strPtr)
	{
		uint32_t codepoint = 0;
		const char *s = *strPtr;
		while(*s)
			if(!decode(&codepoint, (BYTE)*s++))
				break;

		*strPtr = s;
		return codepoint;
	}

//+------------------------------------------------------------------+
	inline int strlen(const char* s, uint32_t delimiter = 0x0)
	{
		state = 0;
		uint32_t code = 0;
		int count = 0;
		while((code = decode(&s)))
			if(code == delimiter)
				break;
			else
				count++;
		return count;
	}

};



extern FT_Library FTLib;

//-------------------------------------------------------------------------------------------------
//	Must call FT_Init_FreeType(&FTLib) before first use, and FT_Done_FreeType(FTLib); for cleanup.
class Font
{
	DISALLOW_COPY_AND_ASSIGN(Font)

public:
//+------------------------------------------------------------------+
	FT_Face face;
	u32 color;

//+------------------------------------------------------------------+
	Font(const char *filename, int size = 0) : color(WHITE)
	{
		FT_New_Face(FTLib, filename, 0, &face);

		if(size)
			setSize(size);
	}

//+------------------------------------------------------------------+
	~Font()
	{
		FT_Done_Face(face);
	}

//+------------------------------------------------------------------+
//	Sets face->size->metrics.y_scale
	FT_Error setSize(int pt)
	{
		return FT_Set_Char_Size(face, pt*64, 0, 0, 0);
	}

//+------------------------------------------------------------------+
	FT_GlyphSlot &render(FT_ULong char_code)
	{
		FT_Load_Char(face, char_code, FT_LOAD_RENDER);
		return face->glyph;
	}

//+------------------------------------------------------------------+
//	Prints a UTF-8 encoded string to the screen.
//	@return a pointer to the next character after the delimiter.
	const char *print(void *framebuffer, float x, float y, const char *text, u32 delimiter = 0x0, int pt = 0, float linefeed = 1.5f);
	FT_Error    print(void *framebuffer, float x, float y, u32 codepoint);
};

