#pragma once
#include <string>

/**
 * Reachability of bad states on given model
 */
template <class Store>
class Reachability {
public:
    Reachability(const std::string& model);
    void run();
private:
    std::string model_name;
};

#include "reachability.tpp"