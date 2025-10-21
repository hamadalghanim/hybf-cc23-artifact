#include "globals.h"

namespace llvm
{
    // pass was failing to load
    // ran ldd -r build/basicblocks/BlocksPass.so  # shows undefined symbols
    // turns out undefined symbol: _ZN4llvm8hashmodeE (build/basicblocks/BlocksPass.so)
    // thus we needed a concrete value for the enum
    MyHashMode hashmode = HASH_NORMAL;
}