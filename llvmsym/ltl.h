#pragma once
#include <string>

/**
 * Checks if given LTL property of a model holds or not
 */
template <class T>
class Ltl {
public:
    Ltl(const std::string& model, const std::string& prop);
    void run();
private:

};

#include "ltl.tpp"