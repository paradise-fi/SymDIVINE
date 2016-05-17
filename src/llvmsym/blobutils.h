#pragma once

#include <vector>
#include <stddef.h>

// TODO: specializace na vector<is_pod<type>::value> s pouzitim memcpy...

template<class T> struct is_not_vector : public std::true_type {};

template<class T, class Alloc> 
struct is_not_vector<std::vector<T, Alloc>> : public std::false_type {};

template< typename T >
typename std::enable_if<is_not_vector<T>::value,void>::type blobWrite( char *& mem, const T &e )
{
    *reinterpret_cast< T* >( mem ) = e;
    mem += sizeof( T );
}
template< typename T >
void blobWrite( char *& mem, const std::vector< T > &e )
{
    blobWrite( mem, e.size() );
    for ( const auto &c : e )
        blobWrite( mem, c );
}
template< typename T, typename ...Tail >
void blobWrite( char *& mem, const T &e, const Tail&...tail)
{
    blobWrite( mem, e );
    blobWrite( mem, tail...);
}

template< typename T >
typename std::enable_if<is_not_vector<T>::value,void>::type blobRead( const char *& mem, T& dest )
{
    dest = *reinterpret_cast< const T* >( mem );
    mem += sizeof( T );
}
template< typename T >
void blobRead( const char *& mem, std::vector< T > &e )
{
    size_t size;
    blobRead( mem, size );
    e.resize( size );
    for ( size_t i = 0; i < size; i++ )
        blobRead( mem, e[i] );
}
template< typename T, typename ...Tail >
void blobRead( const char *& mem, T& e, Tail&...tail)
{
    blobRead( mem, e );
    blobRead( mem, tail...);
}

template< typename T >
constexpr typename std::enable_if<is_not_vector<T>::value,size_t>::type representation_size( const T& )
{
    return sizeof( T );
}

template< typename T >
size_t representation_size( const std::vector< T > &e )
{
    constexpr size_t e_size = sizeof( decltype( e.size() ) );
    size_t s = e_size;

    for (const auto &a : e)
        s += representation_size(a);

    return s;
}

template< typename T, typename... Tail >
size_t representation_size( const T &e, const Tail&...tail )
{
    return representation_size( e ) + representation_size( tail... );
}

