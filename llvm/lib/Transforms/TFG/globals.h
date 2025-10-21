#ifndef __GLOBALS_H__
#define __GLOBALS_H__

const int N_HASHES = 100;
const int N_SHINGLES = 2;

namespace llvm
{
	// Define your enum
	enum MyHashMode
	{
		HASH_NORMAL,
		HASH_VECTOR,
	};

	extern MyHashMode hashmode;
}; // namespace llvm

#endif
