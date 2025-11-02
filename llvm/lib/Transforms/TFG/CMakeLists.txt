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

# --- Force C++17 for this component ---
set_target_properties(LLVMTFG PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# --- Link filesystem if using older libstdc++ (e.g., GCC < 9) ---
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(LLVMTFG PRIVATE stdc++fs)
endif()
