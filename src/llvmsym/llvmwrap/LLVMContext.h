#include <llvm/Config/llvm-config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 3 )
  #include <llvm/LLVMContext.h>
#else
  #include <llvm/IR/LLVMContext.h>
#endif
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_BUGREPORT
