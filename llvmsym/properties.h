#pragma once
#include "llvmsym/blobutils.h"

namespace llvm_sym {

class Properties {
    bool _error = false;

public:

    bool isError() const
    {
        return _error;
    }

    void setError( bool val )
    {
        _error = val;
    }

    size_t getSize() const
    {
        return representation_size( _error );
    }

    void writeData( char * &mem ) const
    {
        blobWrite( mem, _error );
    }

    void readData( const char * &mem )
    {
        blobRead( mem, _error );
    }

};

}
