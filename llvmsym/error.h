#pragma once

namespace llvm_sym {

enum class ErrorCause {
    SYMBOLIC_POINTER,
    SYMBOLIC_SELECT,
    VARIABLE_LEN_ARRAY,
};

void die( ErrorCause cause );

}

