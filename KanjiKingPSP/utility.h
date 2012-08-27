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

//-------------------------------------------------------------------------------------------------
typedef uint8_t BYTE;

const static double	BYTE_TO_DOUBLE	= 1.0  / 255.0;
const static float	BYTE_TO_FLOAT	= 1.0f / 255.0f;

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
inline void Blend(u32 *dst, u32 src = WHITE, float alpha = 1.0f)
{
	const u32 rgba = *dst;
	float	r = BYTE_TO_FLOAT * ((rgba & 0x000000FF)),
			g = BYTE_TO_FLOAT * ((rgba & 0x0000FF00) >> 8),
			b = BYTE_TO_FLOAT * ((rgba & 0x00FF0000) >> 16);
//			a = BYTE_TO_FLOAT * ((rgba & 0xFF000000) >> 24);

	if(src!=WHITE)
	{
		const float	R = BYTE_TO_FLOAT * ((src & 0x000000FF)),
						G = BYTE_TO_FLOAT * ((src & 0x0000FF00) >> 8),
						B = BYTE_TO_FLOAT * ((src & 0x00FF0000) >> 16),
						A = BYTE_TO_FLOAT * ((src)              >> 24) * alpha,
						d = 1.0f - A;

		r = r*d + R*A;
		g = g*d + G*A;
		b = b*d + B*A;
	}
	else
	{
	//	Small optimization for white, not sure if its worth the branch.
		const float	d = 1.0f - alpha;

		r = r*d + alpha;
		g = g*d + alpha;
		b = b*d + alpha;
	}

	*dst =	  (rgba & 0xFF000000)		// keep dst alpha
				| (clamp(int(255.0f*r+0.5f), 0, 255))
				| (clamp(int(255.0f*g+0.5f), 0, 255) << 8)
				| (clamp(int(255.0f*b+0.5f), 0, 255) << 16);
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
