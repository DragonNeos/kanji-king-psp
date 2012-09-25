#ifndef UTILITY_H
#define UTILITY_H
#include <math.h>

//-------------------------------------------------------------------------------------------------
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&);


//-------------------------------------------------------------------------------------------------
	#define BUF_WIDTH		512
	#define SCR_WIDTH		480
	#define SCR_HEIGHT	272
	#define SCRATCHPAD	0x00010000		// Start of   16 KiB scratchpad memory
	#define VRAM			0x04000000		// Start of    2 MiB VRAM / Framebuffer
	#define UNCACHED		0x40000000		// Bypass the cache
//	#define TMP_VRAM		0x04088000		// Start of 1504 KiB of unused VRAM (assuming single display buffer and no depth buffer)
	#define TMP_VRAM		0x04110000		// Start of  960 KiB of unused VRAM (assuming double display buffer and no depth buffer)

//static char *const tmp = (char *) Scratchpad;

	#define BLACK			0xFF000000
	#define WHITE			0xFFFFFFFF
/*
	#deine	YELLOW		0xFF00FFFF;

enum colors
{
	RED =			0xFF0000FF,
	GREEN =		0xFF00FF00,
	BLUE =		0xFFFF0000,
	WHITE =		0xFFFFFFFF,
	LITEGRAY =	0xFFBFBFBF,
	GRAY =		0xFF7F7F7F,
	DARKGRAY =	0xFF3F3F3F,
	BLACK =		0xFF000000,
};
*/

//extern int debug1, debug2, debug3;

extern int subpixel;							// do subpixel rendering?

//-------------------------------------------------------------------------------------------------
typedef uint8_t	BYTE;
typedef uint16_t	WORD;

const static double	BYTE_TO_DOUBLE	= 1.0  / 255.0;
const static float	BYTE_TO_FLOAT	= 1.0f / 255.0f;
const static float	WORD_TO_FLOAT	= 1.0f / 65535.0f;
const static float	_1_3				= 1.0f / 3.0f;
const static float	_2_3				= 2.0f / 3.0f;

//-------------------------------------------------------------------------------------------------
template<typename T>
inline const T &min(const T &a, const T &b)
{
	return a < b ? a : b;
}

//-------------------------------------------------------------------------------------------------
template<typename T>
inline const T &max(const T &a, const T &b)
{
	return a < b ? b : a;
}

//-------------------------------------------------------------------------------------------------
template<typename T>
inline const T &clamp(const T &x, const T &a, const T &b)
{
	return min(max(x,a),b);
}

//-------------------------------------------------------------------------------------------------
template<typename T>
inline const T &wrap(T &x, const T &lim)
{
	while(x < 0)		x += lim;
	while(x >= lim)	x -= lim;
	return x;
}

//-------------------------------------------------------------------------------------------------
template<typename T>
inline void swap(T &a, T &b)
{
	T t = a;
	a   = b;
	b	 = t;
}


/*
//-------------------------------------------------------------------------------------------------
//	Allocates a new byte array, aligned on a 2^alignmentBits byte boundary.
inline char *AlignedAlloc(size_t bytes, int alignmentBits = 6)
{
//----
//	To make a 64 byte aligned pointer, we allocate 80 more bytes,
//	save the original pointer 8 bytes, and the size 16 bytes before the aligned pointer.
	size_t	extra			= 16 + (1<<alignmentBits);
	char		*unaligned	= (char*)malloc(bytes+extra),
				*aligned		= (char*)(((uintptr_t)(unaligned+extra) >> alignmentBits) << alignmentBits);

// save size of (aligned) memory
	*(size_t*)(aligned-16) = bytes;

// save original unaligned pointer
	*(char**)(aligned-8) = unaligned;

//	return aligned pointer
	return aligned;
}

//-------------------------------------------------------------------------------------------------
// Returns the size of the aligned memory
inline size_t AlignedSize(const void *alignedPtr)
{
	if(alignedPtr)		return *(size_t*) (((char*)(alignedPtr))-16);
	else					return 0;
}

//-------------------------------------------------------------------------------------------------
// Deletes aligned memory
inline void AlignedFree(void *alignedPtr)
{
// delete original unaligned pointer
	if(alignedPtr)
		free(*(char**) (((char*)(alignedPtr))-8));
}

*/



//-------------------------------------------------------------------------------------------------
inline void Blend(u32 *dst, float R, float G, float B, float A = 1.0f)
{
	const u32 rgba = *dst;
	float	r = BYTE_TO_FLOAT * ((rgba & 0x000000FF)),
			g = BYTE_TO_FLOAT * ((rgba & 0x0000FF00) >> 8),
			b = BYTE_TO_FLOAT * ((rgba & 0x00FF0000) >> 16);

	const float	d = 1.0f - A;

	r = r*d + R*A;
	g = g*d + G*A;
	b = b*d + B*A;

	*dst =		(min(int(255.0f*r+0.5f), 255))
				|	(min(int(255.0f*g+0.5f), 255) << 8)
				|	(min(int(255.0f*b+0.5f), 255) << 16);
}

//-------------------------------------------------------------------------------------------------
inline void BlendSubpixel(u32 *dst, float R, float G, float B, float RA, float GA, float BA)
{
	const u32 rgba = *dst;
	float	r = BYTE_TO_FLOAT * ((rgba & 0x000000FF)),
			g = BYTE_TO_FLOAT * ((rgba & 0x0000FF00) >> 8),
			b = BYTE_TO_FLOAT * ((rgba & 0x00FF0000) >> 16);

	r = r*(1.0f-RA) + R*RA;
	g = g*(1.0f-GA) + G*GA;
	b = b*(1.0f-BA) + B*BA;

	*dst =		(min(int(255.0f*r+0.5f), 255))
				|	(min(int(255.0f*g+0.5f), 255) << 8)
				|	(min(int(255.0f*b+0.5f), 255) << 16);
}

//-------------------------------------------------------------------------------------------------
inline void Blend(u32 *dst, u32 src = WHITE, float alpha = 1.0f)
{
	const float	R = BYTE_TO_FLOAT * ((src & 0x000000FF)),
					G = BYTE_TO_FLOAT * ((src & 0x0000FF00) >> 8),
					B = BYTE_TO_FLOAT * ((src & 0x00FF0000) >> 16),
					A = BYTE_TO_FLOAT * ((src)              >> 24) * alpha;
	Blend(dst, R, G, B, A);
}

//-------------------------------------------------------------------------------------------------
inline void BlendMax(u32 *dst, BYTE R, BYTE G, BYTE B)
{
	const u32 rgba = *dst;
	R = max(R, (BYTE)((rgba & 0x000000FF)      ));
	G = max(G, (BYTE)((rgba & 0x0000FF00) >>  8));
	B = max(B, (BYTE)((rgba & 0x00FF0000) >> 16));

	*dst = (R) | (G << 8) | (B << 16);
}


//-------------------------------------------------------------------------------------------------
inline void HSVtoRGB(float h, float s, float v, float* rgb)
{
	int i;
	float f,p,q,t,
			&r = rgb[0],
			&g = rgb[1],
			&b = rgb[2];

	if(s)
	{
		h *= 6.0f;			// sector 0 to 5
		i = (int)floor(h);
		f = h - i;			// factorial part of h
		p = v * (1.0f - s);
		q = v * (1.0f - s*f);
		t = v * (1.0f - s*(1.0f-f));

		switch(i)
		{
			case 0:		r = v;	g = t;	b = p;	break;
			case 1:		r = q;	g = v;	b = p;	break;
			case 2:		r = p;	g = v;	b = t;	break;
			case 3:		r = p;	g = q;	b = v;	break;
			case 4:		r = t;	g = p;	b = v;	break;
			default:		r = v;	g = p;	b = q;	break;
		}
	}
	else
	{
	//	achromatic (grey)
		r = v;
		g = v;
		b = v; 
	}
}


//-------------------------------------------------------------------------------------------------
inline void DrawBMPSubPixel(void *framebuffer, float x, float y, const BYTE *data, int w, int h, int pitch, u32 color)
{
	u32 *const buf = (u32*)((((u32)framebuffer)+VRAM) | UNCACHED);

	const
	float	a = BYTE_TO_FLOAT * ((color)              >> 24),
			r = BYTE_TO_FLOAT * ((color & 0x000000FF))		* a,
			g = BYTE_TO_FLOAT * ((color & 0x0000FF00) >> 8)	* a,
			b = BYTE_TO_FLOAT * ((color & 0x00FF0000) >> 16)* a;

//	The fractional part of x determines whether
//	the first column of pixels is considered red, green or blue
	const float fract = x - int(x);
	int								d =  0;
	if			(fract >= _2_3)	d = -2;
	else if	(fract >= _1_3)	d = -1;

	const int lim_w = w / 3 + 1,
					iX  = int(x),
					iY  = int(y);
	for(int j = 0; j < h; j++)
	for(int i = 0; i < lim_w; i++)
	{
		const	int	iR	= 3*i+d,
						iG	= 3*i+d+1,
						iB	= 3*i+d+2;
		const	BYTE	R	= iR < 0 || iR >= w ? 0 : data[j*pitch + iR],
						G	= iG < 0 || iG >= w ? 0 : data[j*pitch + iG],
						B	= iB < 0 || iB >= w ? 0 : data[j*pitch + iB];

		const int sum = int(R) + int(G) + int(B);

	// skip fully transparent pixel
		if(sum)
		{
			int	pixX = (iX+i),
					pixY = (iY+j);

			if(pixX >= 0 && pixX < BUF_WIDTH
			&&	pixY >= 0 && pixY < SCR_HEIGHT)
			{
				const	float	R_ = R * BYTE_TO_FLOAT * r,
								G_ = G * BYTE_TO_FLOAT * g,
								B_ = B * BYTE_TO_FLOAT * b;

				u32 *dst = buf + pixY*BUF_WIDTH + pixX;

				BlendMax(dst, int(R_*255.0f+0.5f), int(G_*255.0f+0.5f), int(B_*255.0f+0.5f));
	//			*dst = (R) | (G << 8) | (B << 16);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
inline void DrawBMPSubPixel3(void *framebuffer, float x, float y, const BYTE *data, int w, int h, int pitch, u32 color)
{
	u32 *const buf = (u32*)((((u32)framebuffer)+VRAM) | UNCACHED);

	const float	r = BYTE_TO_FLOAT * ((color & 0x000000FF)),
					g = BYTE_TO_FLOAT * ((color & 0x0000FF00) >> 8),
					b = BYTE_TO_FLOAT * ((color & 0x00FF0000) >> 16),
					a = BYTE_TO_FLOAT * ((color)              >> 24),
					f = 0.0013071895424837f * a;

//	The fractional part of x determines whether
//	the first column of pixels is considered red, green or blue;
//	and the vertical alignment for y.
	const float fX = x - int(x),
					fY = y - int(y);
	int						d, e;
	if			(fX < _1_3)	d =  0;
	else if	(fX < _2_3)	d = -1;
	else						d = -2;
	if			(fY < _1_3)	e =  0;
	else if	(fY < _2_3)	e = -1;
	else						e = -2;

	const int lim_w = w / 3 + 1,
				 lim_h = h / 3 + 1,
					iX  = int(x),
					iY  = int(y);
	for(int j = 0; j < lim_h; j++)
	for(int i = 0; i < lim_w; i++)
	{
		const	int	iR	= 3*i+d,
						iG	= 3*i+d+1,
						iB	= 3*i+d+2,
						j0 = 3*j+e,
						j1 = 3*j+e+1,
						j2 = 3*j+e+2;
		const	int	R0	= iR < 0 || iR >= w || j0 < 0 || j0 >= h ? 0 : data[j0*pitch + iR],
						G0	= iG < 0 || iG >= w || j0 < 0 || j0 >= h ? 0 : data[j0*pitch + iG],
						B0	= iB < 0 || iB >= w || j0 < 0 || j0 >= h ? 0 : data[j0*pitch + iB],
						R1	= iR < 0 || iR >= w || j1 < 0 || j1 >= h ? 0 : data[j1*pitch + iR],
						G1	= iG < 0 || iG >= w || j1 < 0 || j1 >= h ? 0 : data[j1*pitch + iG],
						B1	= iB < 0 || iB >= w || j1 < 0 || j1 >= h ? 0 : data[j1*pitch + iB],
						R2	= iR < 0 || iR >= w || j2 < 0 || j2 >= h ? 0 : data[j2*pitch + iR],
						G2	= iG < 0 || iG >= w || j2 < 0 || j2 >= h ? 0 : data[j2*pitch + iG],
						B2	= iB < 0 || iB >= w || j2 < 0 || j2 >= h ? 0 : data[j2*pitch + iB],
						sum = R0+G0+B0+R1+G1+B1+R2+G2+B2;

	// skip fully transparent pixel
		if(sum)
		{
			const	int	pixX = (iX+i),
							pixY = (iY+j);

			if(pixX >= 0 && pixX < BUF_WIDTH
			&&	pixY >= 0 && pixY < SCR_HEIGHT)
			{
				const	float	RA = f * (R0+R1+R2),
								GA = f * (G0+G1+G2),
								BA = f * (B0+B1+B2);

				u32 *dst = buf + pixY*BUF_WIDTH + pixX;

	//			BlendMax(dst, int(R_*255.0f+0.5f), int(G_*255.0f+0.5f), int(B_*255.0f+0.5f));
				BlendSubpixel(dst, r, g, b, RA, GA, BA);
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
inline void DrawBMP(void *framebuffer, int x, int y, const BYTE *data, int w, int h, int pitch, u32 color)
{
	const float	R = BYTE_TO_FLOAT * ((color & 0x000000FF)),
					G = BYTE_TO_FLOAT * ((color & 0x0000FF00) >> 8),
					B = BYTE_TO_FLOAT * ((color & 0x00FF0000) >> 16),
//					A = BYTE_TO_FLOAT * ((color)              >> 24);
					A = 1.537870049980777e-5f * (color        >> 24);

	u32 *const buf = (u32*)((((u32)framebuffer)+VRAM) | UNCACHED);

	for(int j = 0; j < h; j++)
	for(int i = 0; i < w; i++)
	{
		BYTE src	= data[j*pitch + i];

	// skip fully transparent pixel
		if(src)
		{
			int	pixX = (x+i),
					pixY = (y+j);
			if(pixX >= 0 && pixX < BUF_WIDTH
			&&	pixY >= 0 && pixY < SCR_HEIGHT)
			{
				u32 *dst = buf + pixY*BUF_WIDTH + pixX;
//				const float a = BYTE_TO_FLOAT * src;
				const float a = A * float(src);

	//			*dst = (R) | (G << 8) | (B << 16);
	//			BlendMax(dst, int(R*a*255.0f+0.5f), int(G*a*255.0f+0.5f), int(B*a*255.0f+0.5f));
	//			Blend(dst, color, BYTE_TO_FLOAT * src);
				Blend(dst, R, G, B, a);
			}
		}
	}

/*	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_T8, 1, 0, 0);
	sceGuTexFunc(GU_TFX_BLEND, GU_TCC_RGBA);
	sceGuTexImage(0, bmp.width, bmp.rows, 
*/
}


#endif//UTILITY_H

