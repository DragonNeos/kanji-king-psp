#ifndef TTF_H
#define TTF_H
#include <stdint.h>

#include <pspkernel.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/fttypes.h>
#include <freetype/ftglyph.h>
#include <freetype/ftimage.h>


#include <malloc.h>

#include "utility.h"


//-------------------------------------------------------------------------------------------------
#define	UNICODE_RAD_SUPP	0x2E80		// first codepoint of CJK Radicals Supplement
#define	UNICODE_RADICALS	0x2F00		// first codepoint of 214 Kangxi Radicals
#define	UNICODE_HIRAGANA	0x3040		// first codepoint of Hiragana
#define	UNICODE_KATAKANA	0x30A0		// first codepoint of Katakana
#define	UNICODE_BOPOMOFO	0x3100		// first codepoint of Bopomofo
#define	UNICODE_CJK			0x4E00		// first codepoint of CJK Unified Ideographs

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
	operator bool()
	{
		return id > 0;
	}

//+------------------------------------------------------------------+
//	Returns the file length in bytes. Warning: seeks to the end.
	SceOff length() const
	{
		return id > 0 ? sceIoLseek(id, 0, SEEK_END) : 0;
	}

//+------------------------------------------------------------------+
//	Read data from a certain offset.
//	@return The number of bytes read
	int read(void *data, SceSize size, SceOff start = 0) const
	{
		if(id <= 0)
			return 0;
		sceIoLseek(id, start, SEEK_SET);
		return sceIoRead(id, data, size);
	}

//+------------------------------------------------------------------+
//	Write data to memory stick.
//	@return The number of bytes written
	int write(void *data, SceSize size)
	{
		return id > 0 ? sceIoWrite(id, data, size) : 0;
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
			if(src)
				memcpy(data, src, length*sizeof(T));

			*((BYTE*)&data[length]) = 0;	// terminating NULL character
		}
	}

//+------------------------------------------------------------------+
	vec(const file &f)
	:	length	(f.length()/sizeof(T)),
		data		((T*)malloc((length+1)*sizeof(T)))
	{
		if(data)
		{
			f.read(data, length*sizeof(T));
			*((BYTE*)&data[length]) = 0;	// terminating NULL character
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
/*	inline int strlen(const char* s, uint32_t delimiter = 0x0)
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
*/
};



extern FT_Library	FTLib;

//-------------------------------------------------------------------------------------------------
struct BMP
{
	short	left,
			top,
			rows,
			width,
			pitch,
			size,									// corresponding font size
			lsb_delta,
			rsb_delta;
	BYTE*	buffer;

	BMP() : buffer(NULL) {}
	~BMP() { free(buffer); }
};

struct GlyphData
{
	u32	index,								// FT_Get_Char_Index
			advance;								// face->glyph->advance.x
	BMP	bmp;

	GlyphData(int i = 0) : index(i), advance(0) {}
};


//-------------------------------------------------------------------------------------------------
//	Must call FT_Init_FreeType(&FTLib) before first use, and FT_Done_FreeType(FTLib); for cleanup.
class Font
{
	DISALLOW_COPY_AND_ASSIGN(Font)

public:
	u32 color;

private:
	int s;
	FT_Face face;

// , for 512 selected codepoints:
//   0 - 319 = 0x0020 - 0x015F	(latin+)
//	320       = 0x4E00+				(kanji, with a single value assumed applicable for all kanji)
//	321 - 511 = 0x3040 - 0x30FF	(hiragana + katakana)
//	vec<WORD> advanceCache;
	vec<GlyphData> glyphCache;
//	float advance, x_factor;				// x_factor = 1 / (64 * face->size->metrics.x_scale)
	FT_Pos prev_rsb_delta;

//+------------------------------------------------------------------+
	static inline u32 GlyphCacheIdx(u32 code)
	{
		if(code < 0x160)							// first 320 characters, from 'space'
			code -= 0x20;
		else
		if(code >= UNICODE_HIRAGANA && code < UNICODE_BOPOMOFO)
			code -= UNICODE_HIRAGANA-320;		// hiragana + katakana
		else
			code = -1;								// not cacheable

		return code;
	}

public:
//+------------------------------------------------------------------+
	Font(const char *filename, int size = 18)
	:	color			(WHITE),
		s				(size),
		face			(NULL),
		glyphCache	(512)
	{
		open(filename, size);
		glyphCache.zero();
	}

//+------------------------------------------------------------------+
	~Font()
	{
		close();
	}

//+------------------------------------------------------------------+
	void close()
	{
		FT_Done_Face(face);
	}


//+------------------------------------------------------------------+
	void open(const char *filename, int size = 18)
	{
		FT_New_Face(FTLib, filename, 0, &face);

		if(size)
			setSize(size);

//		x_factor = 48.0 / (double(face->size->metrics.x_scale) * double(face->units_per_EM));
		debug1 = sizeof(GlyphData);
	}

//+------------------------------------------------------------------+
//	Sets face->size->metrics.y_scale
	FT_Error setSize(int pt)
	{
		FT_Error e = FT_Set_Char_Size(face, pt*64, 0, 0, 0);
		if(e == 0)
			s = pt;
		return e;
	}

//+------------------------------------------------------------------+
	int getSize() const
	{
		return s;
	}

//+------------------------------------------------------------------+
//	Returns the length of the string in pixel, if rendered at the current size.
	float strPixel(const char* s, uint32_t delimiter = 0x0);
	float strPixel(uint32_t code);

//+------------------------------------------------------------------+
//	Returns the character index, or 0 if the char is not found.
	FT_UInt findChar(u32 c)
	{
//		return FT_Get_Char_Index(face, codepoint);
		u32 i = GlyphCacheIdx(c);

		if(i >= 512 || glyphCache[i].index==0)
		{
			FT_UInt idx = FT_Get_Char_Index(face,c);
			if(i < 512 && idx <= 0xFFFF)
				glyphCache[i].index = idx;
			return idx;
		}
		else
			return glyphCache[i].index;	// already cached!
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
//	All control characters < 0x20, incl. NULL, tab and newline are
//	always delimiter.
	const char *print(void *framebuffer, float x, float y, const char *text, u32 delimiter = 0x0, int pt = 0, float linefeed = 0.0f, float xLim = 480.0f, int maxLines = 0);
	FT_Error    print(void *framebuffer, float x, float y, u32 codepoint, float *newX = NULL);
};



#endif//TTF_H

