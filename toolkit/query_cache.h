#pragma once

/**
 * @author Jan Mrázek
 * Cache for results of queries e.g. to SMT solver
 */

#include <unordered_map>
#include <stdexcept>
#include <iostream>

struct ResInfo;

/**
* StatInfo hold statistics about the whole cache
*/
struct StatInfo {
    size_t hit_count;
    size_t miss_count;
    size_t replace_count;

    StatInfo() : hit_count(0), miss_count(0), replace_count(0) {}
};

/**
 * Query type has to have implementation of the std::hash and std::equal_to
 * There are no constraints to result type
 * SInfo has to implement set of statistic method - see ResInfo
 */
template <class Query, class Result, class SInfo = ResInfo>
class QueryCache {
public:
    QueryCache() {}

    /**
     * Checks if given query is cached
     * @return true if cached, false otherwise
     */
    bool is_cached(const Query& q) {
        auto r = cache.find(q);
        if (r == cache.end()) {
            s_info.miss_count++;
            last_cached = false;
        }
        else {
            s_info.hit_count++;
            last_cached = true;
            r->second.second.access();
            last_res = &r->second.first;
        }
        return last_cached;
    }

    /**
     * Returns result of the last query checked in is_cached
     * If the last query was not cached, std::runtime_error is thrown
     */
    const Result& result() const {
        if (last_cached)
            return *last_res;
        throw std::runtime_error("This query was not cached");
    }

    /**
     * Places query with its result to cache, if query is already cached,
     * Result and its statistics are rewritten.
     */
    template <typename... Args>
    void place(const Query& q, const Result& r, Args&&... args) {
        if (cache.find(q) != cache.end())
            s_info.replace_count++;
        cache[q] = std::make_pair(r, SInfo(args...));
    }

    /**
     * Provides statistical info
     */
    StatInfo get_stat() {
        return s_info;
    }

    /**
     * Dumps statistic info to given stream
     */
    void dump_stat(std::ostream& s) {
        s << "Query cache statistics" << std::endl;
        s << "----------------------" << std::endl;
        s << "Hit count: " << s_info.hit_count << std::endl;
        s << "Miss count: " << s_info.miss_count << std::endl;
        s << "Replace count: " << s_info.replace_count << std::endl;
    }

    /**
     * Calls f on every cached item. Passed are Query, Result and SInfo
     */
    template <typename F>
    void process(F f) {
        for (const auto& item : cache)
            f(item.first, item.second.first, item.second.second);
    }
private:
    bool last_cached;
    const Result* last_res;

    StatInfo s_info;

    std::unordered_map<Query, std::pair<Result, SInfo>> cache;
};

/**
 * Basic class for holding statistical information for given query
 */
struct ResInfo {
    ResInfo() : accessed(0) { }

    void access() { accessed++; }
    void dump(std::ostream& s) { s << "Accessed: " << accessed; }

    size_t accessed;
};