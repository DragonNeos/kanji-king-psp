/// <summary>
/// SimpleRNG is a simple random number generator based on 
/// George Marsaglia's MWC (multiply with carry) generator.
/// Although it is very simple, it passes Marsaglia's DIEHARD
/// series of random number generator tests.
/// 
/// Written by John D. Cook 
/// http://www.johndcook.com
/// </summary>


typedef unsigned int uint;


//+------------------------------------------------------------------+
class SimpleRNG
{
	uint	m_w,
			m_z;

public:
//+------------------------------------------------------------------+
// These values are not magical, just the default values Marsaglia used.
// Any pair of unsigned integers should be fine.
	SimpleRNG(uint u = 521288629, uint v = 362436069)
	:	m_w	(u),
		m_z	(v)
	{
	}


//+------------------------------------------------------------------+
	void seed(uint u, uint v = 0)
	{
		if(u) m_w = u; 
		if(v) m_z = v;
	}


//+------------------------------------------------------------------+
// This is the heart of the generator.
// It uses George Marsaglia's MWC algorithm to produce an unsigned integer.
// See http://www.bobwheeler.com/statistics/Password/MarsagliaPost.txt
	uint randUint()
	{
		m_z = 36969 * (m_z & 65535) + (m_z >> 16);
		m_w = 18000 * (m_w & 65535) + (m_w >> 16);
		return (m_z << 16) + m_w;
	}

//+------------------------------------------------------------------+
// Produce a uniform random sample from the open interval (0, 1).
	double randOpen()
	{
		uint u = randUint();										// 0 <= u < 2^32
		return (u + 1.0) * 2.328306435454494e-10;			// 1/(2^32 + 2).
	}


//+------------------------------------------------------------------+
// Produce a uniform random sample from the half open interval [0, 1).
	double rand()
	{
		uint u = randUint();										// 0 <= u < 2^32
		return u * 2.328306436538696e-10;					// 1/(2^32 + 1)
	}

//+------------------------------------------------------------------+
// Produce a uniform random sample from the half open interval [0, 1).
	float randf()
	{
		uint u = randUint();										// 0 <= u < 2^32
		u &= 0x007fffff;											// random mantissa, zero exponent and sign
		u |= 0x3F800000;											// exponent = 127 -> [1,2)

		float *f = (float*)&u;
		*f -= 1.0f;													// random float in   [0,1)
		return *f;
	}

};

