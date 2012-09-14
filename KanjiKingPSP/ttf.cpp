#include "ttf.h"
#include "utility.h"

#include <freetype/freetype.h>
#include <freetype/fttypes.h>
#include <freetype/ftglyph.h>
#include <freetype/ftimage.h>

//-------------------------------------------------------------------------------------------------
FT_Library	FTLib = 0;

//-------------------------------------------------------------------------------------------------
const uint8_t UTF8::utf8d[] =
{
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12, 
};


//-------------------------------------------------------------------------------------------------
float Font::strPixel(const uint32_t code)
{
//	get 'width' of the char in 1/64 pixels (taking into account the current size!)
	FT_UInt k = findChar(code);
	if(k && FT_Load_Glyph(face, k, FT_LOAD_FORCE_AUTOHINT)==0)
		return 0.015625f * face->glyph->advance.x;				// 1/64

	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
float Font::strPixel(const char* ptr, uint32_t delimiter)
{
	UTF8	utf;
	u32	code	= 0,
			sum	= 0;
	int	tmpS	= s;

	if(tmpS != 42)
		setSize(42);

	FT_Set_Transform(face, NULL, NULL);

	for(;;)
	{
		code = utf.decode(&ptr);
		if(code < 0x20 || code == delimiter)
			break;

	// Use a single width, for all kanji, at the first hiragana codepoint (which is unused)
		u32 i = GlyphCacheIdx(code >= UNICODE_CJK ? UNICODE_HIRAGANA : code);

		if(i >= 512 || glyphCache[i].advance==0)
		{
		//	get 'width' of the char in 1/64 pixels (taking into account the current size!)
			FT_UInt k = findChar(code);
			FT_Pos x = 0;
//			if(k && FT_Load_Glyph(face, k, FT_LOAD_DEFAULT)==0)
//			if(k && FT_Load_Glyph(face, k, FT_LOAD_FORCE_AUTOHINT)==0)
			if(k && FT_Load_Glyph(face, k, FT_LOAD_TARGET_LIGHT)==0)
//			if(k && FT_Load_Glyph(face, k, FT_LOAD_NO_HINTING)==0)
//			if(k && FT_Load_Glyph(face, k, FT_LOAD_NO_SCALE|FT_LOAD_IGNORE_TRANSFORM)==0)
			{
				x = face->glyph->advance.x;
//				x = face->glyph->linearHoriAdvance;
				sum += x;
			}

			if(i < 512)
//				glyphCache[i].advance = x==0 ? 1 : x <= 0xFFFF ? x : 0xFFFF;
				glyphCache[i].advance = x==0 ? 1 : x;
		}
		else
			sum += glyphCache[i].advance;	// already cached!
	}

	if(tmpS != 42)
		setSize(tmpS);

	return float(sum) * 3.72023809524e-4 * float(s);			// ~ 1/(64*42)
//	return float(sum) * 0.5e-4 * float(s);
//	return float(sum) * x_factor * float(s);
}

//-------------------------------------------------------------------------------------------------
const char *Font::print(void *framebuffer, float x, float y, const char *text, u32 lim, int size, float linefeed, float xLim, int maxLines)
{
	if(size)
	{
	//	adjust size to fit on screen
		if(linefeed <= 0.0f && xLim <= 480.0f && maxLines==0)
		{
			setSize(42);
			float w = xLim - x - 5.0f,
					l = strPixel(text, lim),
					pt = float(size);

			if(l > 1.0f)
					pt = min(pt, w*42.0f / l);

			if(subpixel && pt < 13.0f)
				pt = min(pt+1.0f,13.0f);	// subpixel rendering has tighter tracking (no hinting), so we can afford a bigger font

			if(FT_Set_Char_Size(face, pt*64.0f, 0, 0, 0) == 0)
				s = pt;
		}
		else if(s!=size)
			setSize(size);
	}

//	special mode, for on and kun readings
	const bool	okurigana	= (linefeed <= -2.0f),
					colored		= (linefeed <= -3.0f);
	const float x0				= x;

	if(okurigana && maxLines)
		linefeed = 1.3f;

//	Decode UTF-8
	UTF8	utf8;
	u32	codepoint = 0,
			prevCode  = 0,
			colA = color,
			colB = ((color >> 25) << 24) | (0x00FFFFFF & color);	// halve alpha

	const uint8_t *ptr = (BYTE*)text;
//	const char *ptr = text;

	bool			dontPrint		= false;
	int			line				= 0;
	FT_UInt		left_glyph		= 0,
					right_glyph		= 0;
	FT_Vector	kerning;

	for(utf8.state = 0; *ptr; ++ptr)
	{
		if(!utf8.decode(&codepoint, *ptr))
		{
//		//	New character
			if(codepoint == lim				// Don't print delimiter
			||	codepoint < 0x20)				// control characters < space, incl. tab and newline
//			||	codepoint == '	'				// tab
//			||	codepoint == '\n')			// newline
				break;

			if(okurigana)
			{
				if(codepoint == ' ') {
					codepoint = 0x3000;		// ideographic space
					if(colored)
						color = colA;
				}
				else if(colored && codepoint == '.')
					color = colB;
			}

			right_glyph = findChar(codepoint);
			if(right_glyph == 0)
				right_glyph = findChar('?');

			if(left_glyph)
			{
				if(FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_UNFITTED, &kerning) == 0)
//				if(FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_DEFAULT,  &kerning) == 0)
					x += kerning.x * 0.015625f;

				left_glyph = 0;
			}

		//----
			if(x >= 512.0f)
				dontPrint = true;

			if(!(okurigana && codepoint == '.'))
			if(!dontPrint)
			{
				if(print(framebuffer, x, y, codepoint, &x) == 0)
					left_glyph = right_glyph;
			}

		//----
			if(codepoint==' ' || codepoint==0x3000)
			if(linefeed > 0.0f || xLim < 480.0f)
			{
				float l = strPixel((const char *)(ptr+1), ' ');

			//	line break?
				bool lineBreak = false;
				if(x + l >= xLim)
				{
					lineBreak = true;
				}
				else if(prevCode==',')		// prematurely break at ',' if there is enough space
				{
					l = strPixel((const char *)(ptr+1));
					if(x + l > xLim && x0 + l <= xLim && x0 + l < x)
						lineBreak = true;
				}

				if(lineBreak)
				{
					if(!((++line >= maxLines && maxLines) || (linefeed <= 0.0f)))
					{
						x = x0;
						y += linefeed * s;
						left_glyph = 0;
						prev_rsb_delta = 0;
					}
					else if(xLim < 480.0f)
						dontPrint = true;
				}
			}

			prevCode = codepoint;
		}

	}

	if(size && s!=size)
		setSize(size);
	color = colA;
	return (const char *) (ptr+1);
}


//-------------------------------------------------------------------------------------------------
FT_Error Font::print(void *framebuffer, float x, float y, u32 codepoint, float *newX)
{
	FT_Error err;

	u32 i = GlyphCacheIdx(codepoint);
	FT_UInt charIndex = findChar(codepoint);

	if(charIndex == 0)
	{
		charIndex = findChar('?');
		i = -1;
	}

//----
	const
	BYTE*	data = NULL;
	int	w = 0,
			h = 0,
			pitch = 0,
			lsb = 0,
			rsb = 0;
	float	advance = 0.0f;

//----
//	if(true)
	if(i >= 512 || s <= 12 || subpixel || glyphCache[i].advance==0 || glyphCache[i].bmp.buffer==NULL || glyphCache[i].bmp.size!=s)
	{
		if(subpixel)
		{
			float subX = x - int(x);
			x = int(x);
			if(newX)
				*newX = x;

			FT_Vector adv   = {int(subX*64.0f + 0.5f), 0};
			FT_Matrix scale = {3*65536, 0, 0, 65536};

			FT_Set_Transform(face, &scale, &adv);
		}
		else
			FT_Set_Transform(face, NULL, NULL);

	//	err = FT_Load_Glyph(face, charIndex, FT_LOAD_RENDER);
	//	err = FT_Load_Glyph(face, charIndex, FT_LOAD_DEFAULT);
	//	err = FT_Load_Glyph(face, charIndex, FT_LOAD_FORCE_AUTOHINT);
	//	err = FT_Load_Glyph(face, charIndex, FT_LOAD_TARGET_LIGHT);		// seems to produce better results (e.g., no missing lines in kanji)
		err = FT_Load_Glyph(face, charIndex, subpixel ? FT_LOAD_NO_HINTING : FT_LOAD_TARGET_LIGHT);

		if(err==0)
	//		err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD);		// unfortunately not available because of patents
			err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LIGHT);

		if(err)
			return err;

	//----
		const FT_GlyphSlot	&glyph	= face->glyph;
		const FT_Bitmap		&bmp		= glyph->bitmap;

		x			+= subpixel ? (glyph->bitmap_left / 3.0f) : glyph->bitmap_left;
		y			-= glyph->bitmap_top;
		w			= bmp.width;
		h			= bmp.rows;
		pitch		= bmp.pitch;
		data		= bmp.buffer;
		advance	= glyph->advance.x * 0.015625f;
		lsb		= glyph->lsb_delta;
		rsb		= glyph->rsb_delta;

//		if(lsb)	debug2 = lsb;
//		if(rsb)	debug3 = rsb;

	//----
//		if(false)
		if(i < 512 && s > 12 && !subpixel && glyphCache[i].advance)
		{
			BMP &dst = glyphCache[i].bmp;											// cached the bmp
			free(dst.buffer);
			dst.buffer = (BYTE*)malloc(h*pitch);
			if(dst.buffer)
			{
				dst.left		= glyph->bitmap_left;
				dst.top		= glyph->bitmap_top;
				dst.width	= w;
				dst.rows		= h;
				dst.pitch	= pitch;
				dst.size		= s;
				dst.lsb_delta = glyph->lsb_delta;
				dst.rsb_delta = glyph->rsb_delta;
				memcpy(dst.buffer, data, h*pitch);
			}

//			if(glyphCache[i].advance == 0)
//				glyphCache[i].advance = int(advance / (x_factor * s) + 0.5f);
		}

//		color |= 0x00FF0000;
	}
	else
	{
//		color &= 0xFF00FFFF;

		const BMP &bmp = glyphCache[i].bmp;										// already cached!

		x		+= bmp.left;
		y		-= bmp.top;
		w		=  bmp.width;
		h		=  bmp.rows;
		pitch	=  bmp.pitch;
		data	=  bmp.buffer;
//		advance= glyphCache[i].advance * x_factor * s;
		advance= glyphCache[i].advance * 3.72023809524e-4 * float(s);
		lsb	=  bmp.lsb_delta;
		rsb	=  bmp.rsb_delta;
	}


//----
	if(newX)
	{
		if(subpixel)
		{
			*newX += advance / 3.0f + 0.015625f * (lsb - rsb);
			x += 0.015625f * lsb;
		}
		else
		{
			*newX += advance;

			int d = prev_rsb_delta - lsb;
			if(d >= 32)
			{
				x -= 1.0f;
				*newX -= 1.0f;
			}
			else if (d < -32)
			{
				x += 1.0f;
				*newX += 1.0f;
			}
			
			prev_rsb_delta = rsb;
		}
	}

//----
	if(subpixel)
		DrawBMPSubPixel(framebuffer, x, y, data, w, h, pitch, color);
	else
		DrawBMP			(framebuffer, x, y, data, w, h, pitch, color);

	return 0;
}


