# Disable the coverage instrumentation for the fuzzer itself.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fsanitize-coverage=0")
if( LLVM_USE_SANITIZE_COVERAGE  )
  add_library(LLVMFuzzer STATIC
    EXCLUDE_FROM_ALL  # Do not build if you are not building fuzzers.
    FuzzerCrossOver.cpp
    FuzzerIO.cpp
    FuzzerLoop.cpp
    FuzzerMain.cpp
    FuzzerMutate.cpp
    FuzzerUtil.cpp
    )

  if( LLVM_INCLUDE_TESTS )
    add_subdirectory(test)
  endif()
endif()
