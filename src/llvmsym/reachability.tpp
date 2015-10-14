#pragma once

#include <iostream>
#include <memory>
#include <vector>

template <class Store, class Hit>
Reachability<Store, Hit>::Reachability(const std::string& name) : model_name(name) {
    // ToDo: Check if the file is valid
}

template <class Store, class Hit>
void Reachability<Store, Hit>::run() {
    /*try*/ {
        std::shared_ptr<BitCode> bc = std::make_shared<BitCode>(model_name);
        Evaluator<Store> eval(bc);

        std::vector<Blob> to_do;

        Blob initial(eval.getSize(), eval.getExplicitSize());
        eval.write(initial.getExpl());

        knowns.insert(initial);
        to_do.push_back(initial);

        bool error_found = false;
        while (!to_do.empty() && !error_found) {
            Blob b = to_do.back();
            to_do.pop_back();

            std::vector< Blob > successors;

            eval.read(b.getExpl());
            eval.advance([&]() {
                Blob newSucc(eval.getSize(), eval.getExplicitSize());
                eval.write(newSucc.getExpl());

                if (eval.isError()) {
                    std::cout << "Error state:\n" << eval.toString() << "is reachable."
                        << std::endl;
                    error_found = true;
                }

                if (knowns.insertCheck(newSucc).first) {
                    if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
                        static int succs_total = 0;
                        std::cerr << ++succs_total << " states so far.\n";
                    }
                    successors.push_back(newSucc);
                }
            });
            std::random_shuffle(successors.begin(), successors.end());
            for (const Blob &succ : successors)
                to_do.push_back(succ);
        }

        if (!error_found)
            std::cout << "Safe." << std::endl;

        if (Config.is_set("--statistics")) {
            std::cout << knowns.size() << " states generated" << std::endl;
        }
    }
    /*catch (std::exception& e) {
        std::cout << "ERROR: uncaught exception: " << e.what() << "\n";
    }
    catch (z3::exception& e) {
        std::cout << "ERROR: uncaught exception: " << e.msg() << "\n";
    }*/
}