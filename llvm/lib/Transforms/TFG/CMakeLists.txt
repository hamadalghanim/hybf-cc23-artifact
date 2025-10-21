add_llvm_component_library(LLVMTFG
    Blocks.cpp
    MinHash.cpp
    LSH.cpp
    instToInt.cpp
	Tiles.cpp
    globals.cpp
    TileReorder.cpp
  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/TFG



  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  Core
  Support
  )

