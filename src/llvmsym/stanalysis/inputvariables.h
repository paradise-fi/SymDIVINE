#pragma once

#include <llvmsym/llvmwrap/Module.h>

#include <llvmsym/llvmdata.h>

#include <set>
#include <string>

class MultivalInfo {
    struct ValueMultivalInfo {
        std::vector< bool > fields;
        std::vector< ValueMultivalInfo > subtypes;

        ValueMultivalInfo( llvm::Type* ty )
        {
            int contained = ty->getNumContainedTypes();
            fields.resize( contained == 0 ? 1 : contained, false );
            if ( ty->isStructTy() ) {
                for ( auto it = ty->subtype_begin(), iE = ty->subtype_end(); it != iE; ++it ) {
                    subtypes.emplace_back( *it );
                }
            }
        }

        bool operator==( const ValueMultivalInfo &snd ) const
        {
            return std::tie( fields, subtypes ) == std::tie( snd.fields, snd.subtypes );
        }

        bool operator<( const ValueMultivalInfo &snd ) const
        {
            return std::tie( fields, subtypes ) < std::tie( snd.fields, snd.subtypes );
        }

        void insert( int field )
        {
            assert( (size_t)field < fields.size() );
            fields[ field ] = true;
        }

        bool present( const std::vector< int > indices ) const
        {
            const ValueMultivalInfo *current_frame = this;

            auto it = indices.begin();
            auto iE = indices.end() - 1;

            for ( ; it < iE; ++it ) {
                size_t subtype_no = *it;
                assert( subtype_no < current_frame->subtypes.size() );
                current_frame = &current_frame->subtypes[ subtype_no ];
            }

            return current_frame->present( indices.back() );
        }

        bool present( int field ) const
        {
            assert( (size_t)field < fields.size() );
            return fields[ field ];
        }

        bool isAllMultival() const
        {
            for ( bool f : fields ) {
                if ( !f ) {
                    return false;
                }
            }

            for ( const auto i : subtypes ) {
                if ( !i.isAllMultival( ) ) {
                    return false;
                }
            }
            return true;
        }

        bool isAllSingleval() const
        {
            for ( bool f : fields ) {
                if ( f ) {
                    return false;
                }
            }

            for ( const auto i : subtypes ) {
                if ( !i.isAllSingleval() ) {
                    return false;
                }
            }
            return true;
        }
    };

    std::map< const llvm::Value*, ValueMultivalInfo > variables;
    std::map< const llvm::Value*, ValueMultivalInfo > regions;

    bool visit_function( const llvm::Function &function );

    bool visit_value( const llvm::Value *val, const llvm::Function *parent );

    void initialize_function( const llvm::Function &function )
    {
        initialize_value( &function );

        auto values_to_initialize = llvm_sym::collectUsedValues( &function );

        for ( const llvm::Value *v : *values_to_initialize ) {
            initialize_value( v );
        }
    }

    void initialize_value( const llvm::Value *val )
    {
        variables.insert( decltype( variables )::value_type( val, val->getType() ) );

        if ( val->getType()->isPointerTy() ) {
            llvm::Type *base_type = val->getType();
            do {
                base_type = base_type->getPointerElementType();
            } while ( base_type->isPointerTy() );

            regions.insert( decltype( variables )::value_type(
                        val, base_type ) );
        }
    }

    public:

    bool isAllMultival( const llvm::Value* v, bool region = false ) const
    {
        if ( region ) {
            auto it = regions.find( v );
            assert( it != regions.end() );

            return it->second.isAllMultival();
        } else {
            auto it = variables.find( v );
            assert( it != variables.end() );

            return it->second.isAllMultival();
        }
    }

    bool isAllSingleval( const llvm::Value *v, bool region = false ) const
    {
        if ( region ) {
            auto it = regions.find( v );
            assert( it != regions.end() );

            return it->second.isAllSingleval();
        } else {
            auto it = variables.find( v );
            assert( it != variables.end() );

            return it->second.isAllSingleval();
        }
    }

    void initialize_module( const llvm::Module *m )
    {
        for ( const llvm::Value &v : m->getGlobalList() ) {
            initialize_value( &v );
        }

        for ( const llvm::Function &f: *m ) {
            initialize_function( f );
        }
    }

    void get_multivalues( const llvm::Module *module );

    MultivalInfo( const llvm::Module *m )
    {
        initialize_module( m );
    }

    void setMultival( const llvm::Value *v, int field = 0 )
    {
        assert( v );
        auto f = variables.find( v );
        assert( f != variables.end() );
        f->second.insert( field );
    }

    bool isMultival( const llvm::Value *v, int field = 0 ) const
    {
        assert( v );
        const auto found = variables.find( v );
        return found != variables.end() && found->second.present( field );
    }

    void setRegionMultival( const llvm::Value *ptr, int field = 0 )
    {
        assert( ptr );
        assert( ptr->getType()->isPointerTy() );
        auto f = regions.find( ptr );
        assert( f != regions.end() );
        f->second.insert( field );
    }

    bool mergeMultivalInfo( ValueMultivalInfo &info_a, ValueMultivalInfo &info_b )
    {
        if ( info_a == info_b )
            return false;
        
        for ( size_t f = 0; f < info_a.fields.size(); ++f ) {
            info_a.fields[ f ] = info_b.fields[ f ] = info_a.fields[ f ] || info_b.fields[ f ] ;
        }

        assert( info_a.subtypes.size() == info_b.subtypes.size() );
        for ( size_t subt = 0; subt < info_a.subtypes.size(); ++subt )
            mergeMultivalInfo( info_a.subtypes[ subt ], info_b.subtypes[ subt ] );

        return true;
    }

    bool mergeRegionMultivalInfoSubtype(
            const llvm::Value *ptra,
            const llvm::Value *ptr_substruct,
            std::vector< int > indices )
    {
        assert( ptra );
        assert( ptr_substruct );
        auto f = regions.find( ptra );
        auto f_sub = regions.find( ptr_substruct );

        assert( f != regions.end() && f_sub != regions.end());

        ValueMultivalInfo *substruct_info = &f->second;

        for ( size_t index : indices ) {
            assert( index < substruct_info->subtypes.size() );
            substruct_info = &substruct_info->subtypes[ index ];
        }
        return mergeMultivalInfo( *substruct_info, f_sub->second );
    }

    bool mergeRegionMultivalInfo( const llvm::Value *ptra, const llvm::Value *ptrb )
    {
        assert( ptra && ptrb );
        if ( (!llvm::isa< llvm::GlobalVariable >( ptra ) && llvm::isa< llvm::Constant >( ptra ))
                || (!llvm::isa< llvm::GlobalVariable >( ptrb ) && llvm::isa< llvm::Constant >( ptrb )) )
            return false;

        auto fa = regions.find( ptra );
        auto fb = regions.find( ptrb );

        assert( fa != regions.end() && fb != regions.end());
        return mergeMultivalInfo( fa->second, fb->second );
    }

    bool isRegionMultival( const llvm::Value *ptr, int field = 0 ) const
    {
        assert( ptr );
        assert( ptr->getType()->isPointerTy() );
        const auto found = regions.find( ptr );
        assert( found != regions.end() );
        return found->second.present( field );
    }
};
