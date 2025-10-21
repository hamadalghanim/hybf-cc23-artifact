#include <bitset>
#include <sstream>
#include "llvm/Support/raw_ostream.h"
#include "LSH.h"

using namespace llvm;

template <typename T>
static std::string toBinaryString(const T &x)
{
	std::stringstream ss;
	ss << std::bitset<sizeof(T) * 8>(x);
	return ss.str();
}

// Returns a number from 0 -> band_cnt, where 0 means exact match
int LSH_distance(size_t sig1, size_t sig2, int band_cnt)
{
	if (band_cnt <= 0)
		band_cnt = 2;

	int sigBits = sizeof(sig1) * 8;
	int bandBits = sigBits / band_cnt;

	int bandMask = 1;
	for (int i = 0; i < bandBits - 1; i++)
	{
		bandMask <<= 1;
		bandMask |= 1; // generate the mask
	}

	// errs() << bandBits << " " << toBinaryString(bandMask) << "\n";

	int hits = 0;
	for (int i = 0; i < band_cnt; i++)
	{
		size_t sub1 = (sig1 >> (i * bandBits)) & bandMask;
		size_t sub2 = (sig2 >> (i * bandBits)) & bandMask;
		// errs() << toBinaryString(sub1) << "\n";

		if (sub1 == sub2)
			hits++;
	}

	// errs() << hits << "\n";
	return band_cnt - hits;
}

// TODO: Pedro please check my work here
int LSH_VecDistance(std::vector<size_t> h1, std::vector<size_t> h2, int band_cnt)
{
	int dist = 0;
	for (size_t i = 0; i < h1.size(); i++)
	{
		if (i >= h2.size())
			break;
		dist += LSH_distance(h1[i], h2[i], band_cnt);
	}

	return dist;
}
