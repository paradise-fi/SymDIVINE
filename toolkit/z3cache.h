#pragma once
#include <string>
#include "query_cache.h"
#include "utils.h"

namespace std {
    /**
     * Let's make z3::expr hashable!
     */
    template <>
    struct hash<z3::expr> {
        size_t operator()(const z3::expr& e) const {
            return e.hash();
        }
    };
}

/**
 * Statistical information for Z3 results
 */
struct Z3Info {
    Z3Info(size_t time = 0) : time(time), accessed(0) { }

    void access() { accessed++; }
    void dump(std::ostream& s) {
        s << "Accessed: " << accessed << "\n";
        s << "Query took: " << time << " us\n";
    }

    size_t time;
    size_t accessed;
};

/**
 * Item for caching subset calls
 */
struct Z3SubsetCall {
    std::vector<llvm_sym::Formula> pc_a;   // Path condition (including defs) for state a
    std::vector<llvm_sym::Formula> pc_b;   // Path condition (including defs) for state b
    std::vector<std::pair<llvm_sym::Formula::Ident, llvm_sym::Formula::Ident>> distinct; // distinct variables
};

namespace std {
    template <>
    struct hash<Z3SubsetCall> {
        size_t operator() (const Z3SubsetCall& c) const {
            auto res = hash_comb(
                hash<std::vector<llvm_sym::Formula>>()(c.pc_a),
                hash<std::vector<llvm_sym::Formula>>()(c.pc_b));
            return hash_comb(
                res,
                hash<std::vector<std::pair<llvm_sym::Formula::Ident, llvm_sym::Formula::Ident>>>()(c.distinct));
        }
    };

    template<>
    struct equal_to<std::vector<llvm_sym::Formula>> {
        bool operator()(const std::vector<llvm_sym::Formula>& a, const std::vector<llvm_sym::Formula>& b) const {
            size_t size = a.size();
            if (size != b.size())
                return false;

            for (size_t i = 0; i != size; i++)
                if (!equal_to<llvm_sym::Formula>() (a[i], b[i]))
                    return false;
            return true;
        }
    };

    template<>
    struct equal_to<Z3SubsetCall> {
        bool operator()(const Z3SubsetCall& a, const Z3SubsetCall& b) const {
            return
                equal_to<std::vector<llvm_sym::Formula>>() (a.pc_a, b.pc_a) &&
                equal_to<std::vector<llvm_sym::Formula>>() (a.pc_b, b.pc_b) &&
                a.distinct == b.distinct;
        }
    };
}


/**
 * Global instance of single z3 query cache
 */
extern QueryCache<Z3SubsetCall, z3::check_result, Z3Info> Z3cache;