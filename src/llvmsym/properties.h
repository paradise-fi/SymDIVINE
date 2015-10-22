#pragma once
#include "llvmsym/blobutils.h"

namespace llvm_sym {

struct Properties {
    bool error = false;
    bool empty = false;

    size_t getSize() const
    {
        return representation_size(error, empty);
    }

    void writeData(char * &mem) const
    {
        blobWrite(mem, error, empty);
    }

    void readData(const char * &mem)
    {
        blobRead(mem, error, empty);
    }

};

}
