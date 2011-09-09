#include "ttf.h"

//-------------------------------------------------------------------------------------------------
FT_Library FTLib = 0;


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
const char *Font::print(void *framebuffer, float x, float y, const char *text, u32 lim, int size, float linefeed)
{
//	special mode, for on and kun readings
	const bool on_kun = (linefeed == -2.0f);

	if(size)
		setSize(size);

	const float x0 = x;

//	Decode UTF-8
	UTF8	utf8;
	u32	codepoint = 0,
			colA = color,
			colB = 0x7FFFFFFF & color;

	const uint8_t *s = (BYTE*)text;

	FT_UInt		left_glyph  = 0,
					right_glyph = 0;
	FT_Vector	kerning;

	for(; *s; ++s)
	{
		if(!utf8.decode(&codepoint, *s))
		{
		//	New character
			if(codepoint == lim				// Don't print delimiter
			||	codepoint == '\n')			// newline
				break;
			if(on_kun)
			{
				if(codepoint == '	')			// tab
					break;
				if(codepoint == ' ')	{
					codepoint = 0x3000;		// ideographic space
					color = colA;
				}
				if(codepoint == '.')
					color = colB;
			}

			right_glyph = FT_Get_Char_Index(face, codepoint);
			if(right_glyph == 0)
				right_glyph = FT_Get_Char_Index(face, '?');

			if(left_glyph)
			{
				if(FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_UNFITTED, &kerning) == 0)
					x += kerning.x * 0.015625f;

				left_glyph = 0;
			}

		//	line break?
			if(x >= 480.0f-0.7f*size)
			{
				x = x0;
			//	y += size;
			//	y += double(face->height) * double(face->size->metrics.y_scale) / double(face->units_per_EM);
				y += linefeed * size;// float(face->size->metrics.y_scale);
				if(linefeed <= 0.0f)
					linefeed = -1.0f;
			}


			if(!(on_kun && codepoint == '.'))
			if(!(linefeed == -1.0f))
			{
				if(print(framebuffer, x, y, codepoint) == 0)
				{
					x += face->glyph->advance.x * 0.015625f;

					left_glyph = right_glyph;
				}
			}
		}

	}

	color = colA;
	return (const char *) (s+1);

}


// max array size = Scratchpad size(16384) / sizeof(FT_Glyph)
//	first 128 ~
//static FT_Glyph *const tmp = (FT_Glyph *) SCRATCHPAD;
//#define tmpSize 256							// array size
//#include <pspgu.h>

//-------------------------------------------------------------------------------------------------
FT_Error Font::print(void *framebuffer, float x, float y, u32 codepoint)
{
/*	for(int i = 0; i < tmpSize; i++)
	{
		if(tmp[i])
			tmp[i]->
	}
*/
	FT_Error err;
//	err = FT_Load_Char (face, codepoint, FT_LOAD_RENDER);
	FT_UInt charIndex = FT_Get_Char_Index(face, codepoint);

	if(charIndex == 0)
		charIndex = FT_Get_Char_Index(face, '?');

	err = FT_Load_Glyph(face, charIndex, FT_LOAD_RENDER);
//	err = FT_Load_Glyph(face, charIndex, FT_LOAD_DEFAULT);

//	FT_Glyph g;
//	err = FT_Get_Glyph(face->glyph, &glyph );
//	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
//	FT_Done_Glyph(glyph);

	if(err)
		return err;

	u32 *const buf  = (u32*)((((u32)framebuffer)+VRAM) | UNCACHED);

	const FT_GlyphSlot	&glyph	= face->glyph;
	const FT_Bitmap		&bmp		= glyph->bitmap;

	const int	X = (int)(x+0.5f),
					Y = (int)(y+0.5f),
					w = bmp.width,
					h = bmp.rows,
					l = glyph->bitmap_left,
					t = glyph->bitmap_top;

	for(int j = 0; j < h; j++)
	for(int i = 0; i < w; i++)
	{
		BYTE src = bmp.buffer[j*bmp.pitch + i];
	
	// skip fully transparent pixel
		if(src)
		{
			int	pixX = (i+X+l) & (BUF_WIDTH-1),
					pixY = (j+Y-t);

			if(pixY < 0 || pixY >= SCR_HEIGHT)
				continue;

			u32 *dst = buf + pixY*BUF_WIDTH + pixX;

			Blend(dst, color, BYTE_TO_FLOAT * src);
		}
	}

/*	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_T8, 1, 0, 0);
	sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGBA);
	sceGuTexImage(0, bmp.width, bmp.rows, 
*/
	return 0;
}


