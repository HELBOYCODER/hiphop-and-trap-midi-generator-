# Cross-compile the Windows x64 VST3 from Linux with clang-cl + lld-link against the Windows SDK/CRT that
# `xwin` downloads (https://github.com/Jake-Shadle/xwin; running it means accepting Microsoft's SDK licence).
#
#   xwin --accept-license splat --output $XWIN        # once
#   cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-x86_64-windows.cmake -DXWIN=$XWIN ...
# See scripts/build-windows-cross.sh
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
if(NOT XWIN)
  set(XWIN "$ENV{XWIN}")
endif()
if(NOT LLVM_BIN)
  set(LLVM_BIN /usr/lib/llvm-18/bin)
endif()
set(CMAKE_C_COMPILER   ${LLVM_BIN}/clang-cl)
set(CMAKE_CXX_COMPILER ${LLVM_BIN}/clang-cl)
set(CMAKE_LINKER       ${LLVM_BIN}/lld-link)
set(CMAKE_AR           ${LLVM_BIN}/llvm-lib)
set(CMAKE_RC_COMPILER  ${LLVM_BIN}/llvm-rc)
set(CMAKE_MT           ${LLVM_BIN}/llvm-mt)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_inc "/imsvc ${XWIN}/crt/include /imsvc ${XWIN}/sdk/include/ucrt /imsvc ${XWIN}/sdk/include/um /imsvc ${XWIN}/sdk/include/shared")
set(CMAKE_C_FLAGS_INIT   "--target=x86_64-pc-windows-msvc ${_inc} -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-pc-windows-msvc ${_inc} -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH")
set(CMAKE_RC_FLAGS_INIT  "-I${XWIN}/sdk/include/um -I${XWIN}/sdk/include/shared -I${XWIN}/crt/include")
set(_lib "/libpath:${XWIN}/crt/lib/x86_64 /libpath:${XWIN}/sdk/lib/um/x86_64 /libpath:${XWIN}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT: no redistributable needed
