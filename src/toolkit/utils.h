#pragma once

#include <functional>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <chrono>
#include <vector>
#include <llvmsym/formula/rpn.h>

/**
 * Inserts newline char every n characters
 */
void break_string(size_t n, std::string& s);

enum class TriState { TRUE, FALSE, UNKNOWN };

/**
 * Provides debug output stream
 */
class DebugInterface {
public:
    static void add_stream(const std::string& name, std::unique_ptr<std::ostream>& s) {
        streams[name] = std::move(s);
    }

    static void add_file(const std::string& name, const std::string& file) {
        streams[name] = std::unique_ptr<std::ostream>(new std::ofstream(file));
    }

    static std::ostream& get_stream(const std::string& name) {
        auto res = streams.find(name);
        if (res == streams.end())
            throw std::runtime_error("DebugInterface: No such stream");
        return *(res->second);
    }

private:
    static std::map<std::string, std::unique_ptr<std::ostream>> streams;
};

/**
 * Provides interface for time measuremnts
 */
class StopWatch {
public:
    void start() { start_time = std::chrono::high_resolution_clock::now(); }
    void stop() { end_time = std::chrono::high_resolution_clock::now(); }
    size_t getUs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    }
    size_t getMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    }
    size_t getS() {
        return std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    }
private:
    typedef typename std::chrono::high_resolution_clock::time_point time_point;
    time_point start_time;
    time_point end_time;
};

// adapted from boost::hash_combine
std::size_t hash_comb(std::size_t h, std::size_t v);

namespace std {

    template <class A, class B>
    struct hash <pair<A, B>> {
        size_t operator() (const pair<A, B>& p) const {
            return hash_comb(hash<A>()(p.first), hash<B>()(p.second));
        }
    };

    template <class T>
    struct hash<vector<T>> {
        size_t operator() (const vector<T>& v) const {
            size_t h = 0;
            for (const T& t : v)
                h = hash_comb(h, hash<T>()(t));
            return h;
        }
    };
}

namespace std {
    template <>
    struct hash<llvm_sym::Formula::Ident> {
        size_t operator() (const llvm_sym::Formula::Ident& i) const {
            size_t h = 0;
            h = hash_comb(h, hash<short unsigned>()(i.seg));
            h = hash_comb(h, hash<short unsigned>()(i.off));
            h = hash_comb(h, hash<short unsigned>()(i.gen));
            h = hash_comb(h, hash<unsigned char>()(i.bw));
            return h;
        }
    };

    template <>
    struct hash<llvm_sym::Formula::Item> {
        size_t operator() (const llvm_sym::Formula::Item& i) const {
            switch (i.kind) {
            case llvm_sym::Formula::Item::Kind::Op:
                return hash<std::underlying_type<llvm_sym::Formula::Item::Operator>::type>()(i.op);
            case llvm_sym::Formula::Item::Kind::BoolVal:
            case llvm_sym::Formula::Item::Kind::Constant:
                return hash<int>()(i.value);
            case llvm_sym::Formula::Item::Kind::Identifier:
                return hash<llvm_sym::Formula::Ident>()(i.id);
            default:
                assert(false);
                return 0; // Suppress compiler warning
            }
        };
    };

    template <>
    struct hash<llvm_sym::Formula> {
        size_t operator() (const llvm_sym::Formula& f) const {
            return hash<vector<llvm_sym::Formula::Item>>()(f._rpn);
        }
    };

    template<>
    struct equal_to<llvm_sym::Formula> {
        bool operator()(const llvm_sym::Formula& a, const llvm_sym::Formula& b) const {
            return a._rpn == b._rpn;
        }
    };

    template<>
    struct equal_to<pair<llvm_sym::Formula, llvm_sym::Formula>> {
        bool operator()(const pair<llvm_sym::Formula, llvm_sym::Formula>& a,
            const pair<llvm_sym::Formula, llvm_sym::Formula>& b) const
        {
            return a.first._rpn == b.first._rpn
                && a.second._rpn == b.second._rpn;
        }
    };
}

// ToDo: Implement zip iterator
template <class A, class B>
class ZipIterator {
public:
private:
    typename A::iterator a_it;
    typename B::iterator b_it;
};

/**
 * Class for zipping two containers together for range based loops
 */
template <class A, class B>
class Zip {
public:

private:
    A& a;
    B& b;
};

template<class T>
class iter_less {
public:
    bool operator()(const T& a, const T& b) {
        return (*a) < (*b);
    }
};

template<class T>
class ref_less {
public:
    bool operator()(const T& a, const T& b) {
        return a.get() < b.get();
    }
};

// Expand STL algorithms to output items from both lists
template<class InputIt1, class InputIt2, class OutputIt, class Compare>
void set_intersection_diff(InputIt1 first1, InputIt1 last1,
                            InputIt2 first2, InputIt2 last2,
                            OutputIt d_first, OutputIt d_second, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            ++first1;
        }
        else {
            if (!comp(*first2, *first1)) {
                *d_first++ = *first1++;
                *d_second++ = *first2;
            }
            ++first2;
        }
    }
}

template<class InputIt1, class InputIt2,
         class OutputIt, class Compare>
void set_symmetric_difference_diff(InputIt1 first1, InputIt1 last1,
                                  InputIt2 first2, InputIt2 last2,
                                  OutputIt d_first, OutputIt d_second, Compare comp)
{
    while (first1 != last1) {
        if (first2 == last2) {
            std::copy(first1, last1, d_first);
            return;
        }

        if (comp(*first1, *first2)) {
            *d_first++ = *first1++;
        } else {
            if (comp(*first2, *first1)) {
                *d_second++ = *first2;
            } else {
                ++first1;
            }
            ++first2;
        }
    }
    std::copy(first2, last2, d_second);
}

template <class T, class Id = size_t>
class IdContainer {
public:
    using IdType = Id;
    IdContainer() :
        unused({ 1 })
    { }

    typename std::map<Id, T>::const_iterator begin() const {
        return values.begin();
    }
    typename std::map<Id, T>::iterator begin() {
        return values.begin();
    }
    typename std::map<Id, T>::const_iterator cbegin() const {
        return values.cbegin();
    }
    typename std::map<Id, T>::const_iterator end() const {
        return values.end();
    }
    typename std::map<Id, T>::iterator end() {
        return values.end();
    }
    typename std::map<Id, T>::const_iterator cend() const {
        return values.cend();
    }

    size_t size() const {
        return values.size();
    }

    T& insert(Id id, const T& item) {
        return values.insert({ id, item }).first->second;
    }

    Id insert(const T& item) {
        auto i = unused.back();
        unused.pop_back();
        if (unused.empty())
            unused.push_back(i + 1);
        values.insert( {i, item});
        return i;
    }

    void erase(typename std::map<Id, T>::iterator& it) {
        unused.push_back(it->first);
        values.erase(it);
    }

    void erase(Id i) {
        unused.push_back(i);
        values.erase(values.find(i));
    }

    const std::vector<Id>& free_idx() const {
        return unused;
    }

    std::vector<Id>& free_idx() {
        return unused;
    }

    const T& get(Id id) const {
        return values[id];
    }

    T& get(Id id) {
        return values[id];
    }

    void clear() {
        values.clear();
        unused = { 0 };
    }
private:
    std::map<Id, T> values;
    std::vector<Id> unused;
};

template <class T, class Info = std::tuple<>>
class UnionSet {
public:
    UnionSet(const T& data) : data(data), parent(nullptr), depth(0) { }

    bool is_root() const {
        return !parent;
    }

    UnionSet* get_set() {
        UnionSet* p = this;
        while (!p->is_root())
            p = p->parent;
        return p;
    }

    T data;
    Info tag;
private:
    UnionSet* parent;
    size_t depth;

    template <class U, class I, class Merge>
    friend void join(UnionSet<U, I>*, UnionSet<U, I>*, Merge);
};


struct TupleMergeOp {
    std::tuple<> operator()(std::tuple<>, std::tuple<>) {
        return {};
    }
};

template <class T, class Info, class Merge>
void join(UnionSet<T, Info>* a, UnionSet<T, Info>* b, Merge merge = TupleMergeOp()) {
    assert(a->is_root());
    assert(b->is_root());
    assert(a != b);

    UnionSet<T, Info>* top;
    UnionSet<T, Info>* node;
    if (b->depth < a->depth) {
        top = b; node = a;
    }
    else {
        top = a; node = b;
    }

    top->depth++;
    node->parent = top;
    top->tag = merge(std::move(top->tag), std::move(node->tag));
}
