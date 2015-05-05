#pragma once

#include "toolkit/hash.h"
#include "llvmsym/smtdatastore.h"
#include "toolkit/utils.h"

#include <unordered_map>
#include <set>
#include <unordered_set>
#include <vector>
#include <memory>
#include <stdexcept>

using namespace llvm_sym;

// reference counting blobs
struct Blob {
    char* getSymb() const { return mem + explicit_size + user_size; }
    char* getExpl() const { return mem + user_size; }
    char* getUser() const { return mem; }

    size_t getSymbSize() const { return size - user_size - explicit_size; }
    size_t getExplSize() const { return explicit_size; }
    size_t getUserSize() const { return user_size; }

    template <class T>
    T& user_as() {
        return *((T*)getUser());
    }

    Blob(size_t s, size_t e_s, size_t user_s = 0) : size(s), mem(new char[s]),
        refcount(new size_t(1)), explicit_size(e_s), user_size(user_s) {}

    Blob(const Blob &snd) : size(snd.size), mem(snd.mem),
        refcount(snd.refcount), explicit_size(snd.explicit_size), user_size(snd.user_size)
    {
        ++*refcount;
    }

    Blob &operator=(const Blob snd) {
        if (this != &snd) {
            if (mem && --*refcount == 0) {
                delete[] mem;
            }

            mem = snd.mem;
            refcount = snd.refcount;
            explicit_size = snd.explicit_size;
            user_size = snd.user_size;
            size = snd.size;
            ++*refcount;
        }
        return *this;
    }

    ~Blob() {
        assert(*refcount >= 1);
        if (--*refcount == 0) {
            delete[] mem;
            delete refcount;
        }
    }

    /*  bool operator<( const Blob &snd ) const {
      if ( explicit_size != snd.explicit_size )
      return explicit_size < snd.explicit_size;
      return memcmp( mem, snd.mem, explicit_size ) < 0; // and data part is less than...
      }

      bool operator==( const Blob &snd ) const {
      if ( explicit_size != snd.explicit_size )
      return false;
      if ( memcmp( mem, snd.mem, explicit_size ) )
      return false;
      return SMTStore::equal( mem + explicit_size, snd.mem + explicit_size );
      }*/
private:
    char *mem;
    size_t *refcount;
    size_t size;
    size_t user_size;
    size_t explicit_size;
};

struct blobHashExplicitPart {
    size_t operator()(const Blob &b) const {
        hash128_t h = spookyHash(reinterpret_cast<void*>(b.getExpl()),
            b.getExplSize(), 0, 0);
        return h.first ^ h.second;
    }
};

struct blobEqualExplicitPart {
    bool operator()(const Blob &b1, const Blob &b2) const {
        if (b1.getExplSize() != b2.getExplSize())
            return false;
        return memcmp(b1.getExpl(), b2.getExpl(), b1.getExplSize()) == 0;
    }
};

struct blobHashExplicitUserPart {
    size_t operator()(const Blob &b) const {
        hash128_t h = spookyHash(reinterpret_cast<void*>(b.getUser()),
            b.getExplSize() + b.getUserSize(), 0, 0);
        return h.first ^ h.second;
    }
};

struct blobEqualExplicitUserPart {
    bool operator()(const Blob &b1, const Blob &b2) const {
        if (b1.getExplSize() != b2.getExplSize()
         || b1.getUserSize() != b2.getUserSize())
            return false;
        return memcmp(b1.getUser(), b2.getUser(), b1.getExplSize() + b2.getUserSize()) == 0;
    }
};

/**
 * Identifier for states
 */
struct StateId {
    typedef size_t IdType;

    IdType exp_id; // Identifies id of the explicit part
    IdType sym_id; // Identifies id of the symbolic part

    StateId(IdType exp_id = 0, IdType sym_id = 0) : exp_id(exp_id), sym_id(sym_id) { }
    bool operator==(const StateId& id) const {
        return exp_id == id.exp_id && sym_id == id.sym_id;
    }
    
    bool operator!=(const StateId& id) const {
        return exp_id != id.exp_id || sym_id != id.sym_id;
    }
};

static inline std::ostream& operator<<(std::ostream& o, const StateId& i) {
    return (o << "<" << i.exp_id << ", " << i.sym_id << ">");
}

namespace std {
    /**
     * Support hashing for StateId in the general way
     */
    template<>
    struct hash<StateId> {
        typedef typename StateId::IdType IdType;
        size_t operator()(const StateId& id) const {
            return hash_comb(hash<IdType>()(id.exp_id), hash<IdType>()(id.sym_id));
        }
    };

    /**
     * Support StateId equality in the general way
     */
    template<>
    struct equal_to<StateId> {
        bool operator()(const StateId& a, const StateId& b) const {
            return a == b;
        }
    };
}

class DatabaseException : public std::runtime_error {
public:
    DatabaseException(const std::string& msg) : std::runtime_error(msg) { }
};

template<typename ExplState, typename SymbState,  typename SymbContainer,
    typename ExplHasher, typename ExplEqual, typename Id = size_t>
class Database {
public:
    typedef typename StateId::IdType IdType;

    Database() : id_counter(1) {};

    bool seen(const ExplState &st) {
        auto got = si_table.find(st);
        if (got == si_table.end())
            return false;
        else {
            SymbState sst;
            fillSym(sst, st);
            return got->second.second.seen(sst);
        }
    }

    bool seen(StateId id) {
        auto got = is_table.find(id);
        if (got == is_table.end())
            return false;
        return true;
    }

    //assume st is not yet stored
    StateId insert(const ExplState &st) {
        auto got = si_table.find(st);
        if (got == si_table.end()) {
            SymbContainer sc;
            SymbState sst;
            fillSym(sst, st);
            StateId id;
            id.sym_id = sc.insert(sst);
            si_table.insert(std::make_pair(st, std::make_pair(++id_counter, sc)));
            id.exp_id = id_counter - 1;
            is_table.insert(std::make_pair(id, st));
            return id;
        }
        else {
            SymbState sst;
            fillSym(sst, st);
            StateId id;
            id.exp_id = got->second.first;
            id.sym_id = got->second.second.insert(sst);
            is_table.insert(std::make_pair(id, st));
            return id;
        }
    }

    std::pair<bool, StateId> insertCheck(const ExplState &st) {
        auto got = si_table.find(st);
        if (got == si_table.end()) {
            SymbContainer sc;
            SymbState sst;
            fillSym(sst, st);
            StateId id;
            id.sym_id = sc.insert(sst);
            si_table.insert(std::make_pair(st, std::make_pair(++id_counter, sc)));
            id.exp_id = id_counter - 1;
            is_table.insert(std::make_pair(id, st));
            return std::make_pair(true, id);
        }
        else {
            SymbState sst;
            fillSym(sst, st);
            StateId id;
            id.exp_id = got->second.first;
            std::pair<bool, IdType> ret = got->second.second.insertCheck(sst);
            id.sym_id = ret.second;
            if (ret.first)
                is_table.insert(std::make_pair(id, st));
            return std::make_pair(ret.first, id);
        }
    }

    void fillSym(SymbState &sst, const ExplState &st) {
        const char * temp = st.getSymb();
        sst.readData(temp);
    }

    size_t size() {
        size_t retVal = 0;
        for (auto e : si_table) {
            retVal += e.second.second.size();
        }
        return retVal;
    }

    Blob getState(StateId id) {
        auto res = is_table.find(id);
        if (res == is_table.end()) {
            std::cout << "Table content: ";
            for(const auto& item : is_table)
                std::cout << item.first;
            std::cout << "\n";
            throw DatabaseException("Cannot find state <"
                    + std::to_string(id.exp_id) + ", "
                    + std::to_string(id.sym_id) + ">");
        }
        return res->second;
    }

private:
    // Table from explicit state -> <id, Symbolic>
    std::unordered_map<
        ExplState, std::pair<IdType, SymbContainer>, ExplHasher, ExplEqual> si_table;
    // Table from id to explicit state
    std::unordered_map<StateId, ExplState> is_table;

    IdType id_counter; // Holds next free id
};

struct EmptyValue {
    bool nothing;
    void readData(char *_) {}
};

struct EmptyCandidate {
    bool nothing;
    bool seen() { return false; }
    void insert() {}
    bool insertCheck() { return false; }
    size_t size() { return 0; }
};

struct ObliviousCandidate {
    bool nothing;
    bool seen() { return true; }
    void insert() {}
    bool insertCheck() { return true; }
    size_t size() { return 0; }
};

struct SMTEqual {
    bool operator()(const SMTStore &a, const SMTStore &b) const {
        //assert( a.segments_mapping.size() == b.segments_mapping.size() );
        return a.subseteq(a, b) && a.subseteq(b, a);
    }
};

struct SMTSubseteq {
    bool operator()(const SMTStore &a, const SMTStore &b) const {
        //assert( a.segments_mapping.size() == b.segments_mapping.size() );
        return a.subseteq(a, b);
    }
};

template< typename State, typename Hit >
class LinearCandidate {
public:
    typedef typename StateId::IdType IdType;

    bool seen(const State &st) {
        for (auto e : data) {
            if (hit(e, st))
                return true;
        }
        return false;
    }

    bool seen(IdType id) {
        return id != 0 && id <= data.size();
    }

    IdType insert(const State &st) {
        data.push_back(st);
        return data.size();
    }

    std::pair<bool, IdType> insertCheck(const State &st) {
        IdType id = 1;
        for (auto e : data) {
            if (hit(st, e))
                return std::make_pair(false, id);
            id++;
        }
        data.push_back(st);
        return std::make_pair(true, data.size());
    }

    size_t size() {
        return data.size();
    }
private:
    std::vector<State> data;
    Hit hit;
};

//State implements operator<
template< typename State >
struct OrderedCandidate {
    std::set< State > data;

    bool seen(const State &st) {
        return data.count(st) != 0;
    }

    void insert(const State &st) {
        data.insert(st);
    }

    bool insertCheck(const State &st) {
        return data.insert(st).second;
    }

    size_t size() {
        return data.size();
    }
};

template< typename State, typename Hasher, typename Equal >
struct HashedCandidate {
    std::unordered_set< State, Hasher, Equal > data;

    bool seen(const State &st) {
        return data.count(st) != 0;
    }

    void insert(const State &st) {
        State temp = st;
        temp.keep = true;
        data.insert(temp);
    }

    bool insertCheck(const State &st) {
        if (data.find(st) != data.end())
            return false;
        insert(st);
        return true;
    }

    size_t size() {
        return data.size();
    }
};
