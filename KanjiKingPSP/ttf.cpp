#include "ttf.h"
#include "utility.h"

#include <freetype/freetype.h>
#include <freetype/fttypes.h>
#include <freetype/ftglyph.h>
#include <freetype/ftimage.h>

//-------------------------------------------------------------------------------------------------
SuperFastBlur	SFB;
vec<BYTE>		SFBTMP(272*272*3*3);

FT_Library		FTLib = 0;

uint32_t      *const	tmp_str = (uint32_t*)(SCRATCHPAD+8*1024),			// decoded string (max 2048 chars)
							tmp_str_len = 0;

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
void SuperFastBlur::blur(BYTE *pix, int w, int h)
{
	const int	wm = w-1,
					hm = h-1;

//----
//	use unused video-ram as temporary memory
	BYTE	*L		= (BYTE*)TMP_VRAM;		// "luminosity" as opposed to red, green or blue
	int	*vMIN	= (int*) (L + ((w*h+0x3F)&~0x3F)),
			*vMAX	= vMIN + max(w,h);

	int sum,x,y,i,p,p1,p2,yp,yi,yw;

	yw = yi = 0;

	for(y = 0; y < h; y++, yw += w)
	{
		sum = 0;
		for(i = -1; i <= 1; i++)
		{
			p = yi + clamp(i,0,wm);
			sum += pix[p];
		}

		for(x = 0; x < w; x++, yi++)
		{
			L[yi] = dv[sum];

			if(y==0)
			{
				vMIN[x] = min(x+1+1,wm);
				vMAX[x] = max(x-1,0);
			}

			p1 = yw + vMIN[x];
			p2 = yw + vMAX[x];

			sum += pix[p1] - pix[p2];
		}
	}

	for(x = 0; x < w; x++)
	{
		sum = 0;
		yp = -1*w;
		for(i = -1; i <= 1; i++, yp += w)
		{
			yi = max(0,yp) + x;
			sum += L[yi];
		}

		yi = x;
		for(y=0; y < h; y++, yi += w)
		{
			pix[yi] = dv[sum];
			if(x==0)
			{
				vMIN[y]=min(y+1+1,hm)*w;
				vMAX[y]=max(y-1,0)*w;
			}

			p1=x+vMIN[y];
			p2=x+vMAX[y];

			sum += L[p1] - L[p2];
		}
	}

}//blur



//-------------------------------------------------------------------------------------------------
const char* DecodeUTF8(const char* ptr, uint32_t delimiter)
{
	UTF8	utf8;
	u32	code = 0;
	for(tmp_str_len = 0; (tmp_str[tmp_str_len] = code = utf8.decode(&ptr)); tmp_str_len++)
	{
	// break on all control characters < space, incl. tab and newline
		if(code == delimiter || code < 0x20)
			break;
	}

//	tmp_str[tmp_str_len+1] = 0;

	return ptr;
}

//-------------------------------------------------------------------------------------------------
float Font::strPixel(const uint32_t code)
{
//	get 'width' of the char in 1/64 pixels (taking into account the current size!)
	FT_UInt k = findChar(code);
	if(k && FT_Load_Glyph(face, k, FT_LOAD_TARGET_LIGHT)==0)
		return 0.015625f * face->glyph->advance.x;				// 1/64

	return 0.0f;
}

//-------------------------------------------------------------------------------------------------
float Font::strPixel(const char* ptr, uint32_t delimiter)
{
	DecodeUTF8(ptr,delimiter);

	return strPixel(tmp_str, delimiter);
}

//-------------------------------------------------------------------------------------------------
float Font::strPixel(const u32* ptr, uint32_t delimiter)
{
	u32	code	= 0,
			sum	= 0;
	int	tmpS	= s;

	FT_UInt		left_glyph	= 0,
					right_glyph	= 0;
	FT_Vector	kern;

	if(tmpS != 42)
		setSize(42);

	FT_Set_Transform(face, NULL, NULL);

	for(;;)
	{
		code = *ptr++;
		if(code < 0x20 || code == delimiter)
			break;

	// Use a single width, for all kanji, at the first hiragana codepoint (which is unused)
		u32 i = GlyphCacheIdx(code >= UNICODE_CJK ? UNICODE_HIRAGANA : code);

		if(i >= 512 || glyphCache[i].advance==0)
		{
		//	get 'width' of the char in 1/64 pixels (taking into account the current size!)
			right_glyph = findChar(code);
			FT_Pos x = 0;
			if(right_glyph
//			&& FT_Load_Glyph(face, right_glyph, FT_LOAD_DEFAULT)==0)
//			&& FT_Load_Glyph(face, right_glyph, FT_LOAD_FORCE_AUTOHINT)==0)
//			&& FT_Load_Glyph(face, right_glyph, FT_LOAD_TARGET_LIGHT)==0)
			&& FT_Load_Glyph(face, right_glyph, FT_LOAD_NO_HINTING)==0)
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
		{
			sum += glyphCache[i].advance;	// already cached!
			right_glyph = glyphCache[i].index;
		}

		if(left_glyph && right_glyph
		&&	FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_UNFITTED, &kern) == 0)
			sum += kern.x;

		left_glyph = right_glyph;

	}

	if(tmpS != 42)
		setSize(tmpS);

	return float(sum) * 3.72023809524e-4f * float(s);			// ~ 1/(64*42)
//	return float(sum) * 0.5e-4 * float(s);
//	return float(sum) * x_factor * float(s);
}

//-------------------------------------------------------------------------------------------------
int Font::strLines(float size, float width)
{
	u32	spc[]	= {' ',0};
	const
	float	scale	= size / float(s),
			space	= scale * strPixel(spc);
	float	x		= 0.0f;
	int	lines	= 1;


	for(u32 n = 0; n < tmp_str_len; )
	{
		while(tmp_str[n]==' ')
			n++;

	//	length of a single word
		float word = scale * strPixel(tmp_str+n,' ');

		if(word > width)
			return 0x7FFFFFFF;				// Not even a single word fits on a line

	//	line break?
		if(x + word >= width)
		{
			x = word + space;
			lines++;
		}
		else
			x += word + space;

	//	advance
		while(tmp_str[n]!=' ' && n < tmp_str_len)
			n++;
	}

	return lines;
}


//-------------------------------------------------------------------------------------------------
const char *Font::print(void *framebuffer, float x, float y, const char *text, u32 lim, int size, float linefeed, float xLim, int maxLines, int *lines)
{
//----
//	pre-decode
	const char *ret = DecodeUTF8(text,lim);

//	special mode, for on and kun readings
	const bool	okurigana	= (linefeed <= -2.0f),
					colored		= (linefeed <= -3.0f);
	const float x0				= x;

	if(okurigana && maxLines)
		linefeed = 1.2f;

//----		
	bool fitSize = false;
	if(size)
	{
	//	adjust size to fit on screen
		if(xLim < 480.0f && maxLines > 0)
		{
			fitSize = true;
			setSize(42);
			const
			float w = xLim - x;
			float l = strPixel(text, lim);

			float	pt = float(size);

			if(linefeed <= 0.0f)
			{
				if(l > 1.0f)
					pt = min(pt, w*42.0f / l);
			}
			else if(l * size > w*42.0f)
			{
			//	reduce size, until the text fits on the required number of lines
				for(pt = size - 1.0f; pt >= 8.0f; pt -= 1.0f)
				{
					if(strLines(pt,w) <= maxLines)
						break;
				}
			}

			if(subpixel && pt <= 15.0f)
				pt = min(pt+1.0f, 15.0f);	// subpixel rendering has tighter tracking (no hinting), so we can afford a bigger font

			if(FT_Set_Char_Size(face, pt*64.0f, 0, 0, 0) == 0)
				s = pt;
		}
		else if(s!=size)
			setSize(size);
	}

	int			line				= 0;
	u32			codepoint		= 0,
					prevCode			= 0,
					colA				= color,
					colB				= ((color >> 25) << 24) | (0x00FFFFFF & color);	// halve alpha
	FT_UInt		left_glyph		= 0,
					right_glyph		= 0;
	FT_Vector	kerning;

	for(u32 c = 0; c < tmp_str_len; c++)
	{
		codepoint = tmp_str[c];

	//----
	//	tmp_str_len should not include the delimiter,
	//	so the fact that this check is necessary seems to indicate some bug...
		if(codepoint < 0x20 || codepoint == lim)
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
			if(FT_Get_Kerning(face, left_glyph, right_glyph, subpixel ? FT_KERNING_DEFAULT : FT_KERNING_UNFITTED, &kerning) == 0)
				x += kerning.x * 0.015625f;

			left_glyph = 0;
		}

	//----
		if(x >= 512.0f || y > 272.0f)
			break;

		if(!(okurigana && codepoint == '.'))
		{
			if(print(framebuffer, x, y, codepoint, &x) == 0)
				left_glyph = right_glyph;
		}

	//----
		if(codepoint==' ' || codepoint==0x3000)
		if(linefeed > 0.0f || xLim < 480.0f)
		{
			float l = strPixel(tmp_str+c+1, ' ');

		//	line break?
			bool	lineBreak	= false,
					earlyBreak	= false;
			if(x + l >= xLim)
			{
				lineBreak = true;
			}
			else if(prevCode==',' || prevCode==';')
			{
			// "prematurely" break at ',' or ';' if there is enough space, anyway
				l = strPixel(tmp_str+c+1);
				if(x + l > xLim && x0 + l <= xLim && x0 + l < x)
					earlyBreak = true;
			}

			if(lineBreak||earlyBreak)
			{
				if(!((line+1 >= maxLines && maxLines) || (linefeed <= 0.0f)))
				{
					line++;
					x = x0;
					y += linefeed * s;
					left_glyph = 0;
					prev_rsb_delta = 0;
				}
				else if(xLim < 470.0f && !earlyBreak && !fitSize)
					break;
			}
		}

		prevCode = codepoint;
	}

	if(size && s!=size)
		setSize(size);
	color = colA;

	if(lines)
		*lines = line+1;

	return ret;
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
	if(i >= 512 || subpixel || glyphCache[i].advance==0 || glyphCache[i].bmp.buffer==NULL || glyphCache[i].bmp.size!=s)
	{
		if(subpixel)
		{
			float subX = x - int(x);
			x = int(x);
			if(newX)
				*newX = x;

			FT_Vector adv   = {int(subX*64.0f + 0.5f), 0};
//			FT_Matrix scale = {3*65536, 0, 0, 65536};
			FT_Matrix scale = {3*65536, 0, 0, subpixel==2 ? 3*65536 : 65536};

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
			err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

		if(err)
			return err;

	//----
		const FT_GlyphSlot	&glyph	= face->glyph;
		const FT_Bitmap		&bmp		= glyph->bitmap;

		x			+= subpixel==0 ? glyph->bitmap_left : (glyph->bitmap_left * _1_3);
		y			-= subpixel==0 ? glyph->bitmap_top  : (glyph->bitmap_top  * (subpixel==2 ? _1_3 : 1.0f));
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
			*newX += advance * _1_3 + 0.015625f * (lsb - rsb);
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
	{
		if(subpixel==2)
		{
			const int W = w+12,
						 H = h+12;
			for(int i = 0; i < W; i++)
			for(int j = 0; j < H; j++)
				SFBTMP[i+j*W] = (i>=6 && j>=6 && i-6<w && j-6<h) ? data[(i-6)+(j-6)*pitch] : 0;

			SFB.blur(SFBTMP, W, H);
			SFB.blur(SFBTMP, W, H);
			DrawBMPSubPixel3(framebuffer, x-2.0f, y-2.0f, SFBTMP, W, H, W, color);
		}
		else
			DrawBMPSubPixel(framebuffer, x, y, data, w, h, pitch, color);
	}
	else
		DrawBMP				(framebuffer, x, y, data, w, h, pitch, color);

	return 0;
}


