#pragma once

#include <iostream>
#include <memory>
#include <vector>

template <class Store, class Hit>
Reachability<Store, Hit>::Reachability(const std::string& model_name)
    : eval(std::make_shared<BitCode>(model_name))
{}

template <class Store, class Hit>
void Reachability<Store, Hit>::run() {
    try {
        std::queue<StateId> to_do;

        Blob initial(eval.getSize(), eval.getExplicitSize());
        eval.write(initial.getExpl());

        StateId initial_id = knowns.insert(initial);
        graph.add_vertex(initial_id);
        to_do.push(initial_id);

        bool error_found = false;
        while (!to_do.empty() && !error_found) {
            StateId vertex = to_do.front(); to_do.pop();
            Blob b = knowns.getState(vertex);

            std::vector<StateId> successors;

            eval.read(b.getExpl());
            eval.advance([&]() {
                Blob newSucc(eval.getSize(), eval.getExplicitSize());
                eval.write(newSucc.getExpl());

                if (eval.is_error() && !eval.is_empty()) {
                    std::cout << "Error state:\n";
                    eval.dump();
                    std::cout << "is reachable." << std::endl;
                    error_found = true;
                }

                auto value = knowns.insertCheck(newSucc);
                if (value.first) {
                    if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
                        static int succs_total = 0;
                        std::cerr << ++succs_total << " states so far.\n";
                        /*std::cout << "New id: <" << value.second.exp_id
                            << ", " << value.second.sym_id << ">\n";*/
                    }
                    to_do.push(value.second);
                    
                }
                successors.push_back(value.second);
            });
            graph.add_successors(vertex, successors);
        }

        if (!error_found)
            std::cout << "Safe." << std::endl;

        if (Config.is_set("--statistics")) {
            std::cout << knowns.size() << " states generated" << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cout << "ERROR: uncaught exception: " << e.what() << "\n";
    }
    catch (z3::exception& e) {
        std::cout << "ERROR: uncaught exception: " << e.msg() << "\n";
    }
}

template <class Store, class Hit>
void Reachability<Store, Hit>::output_state_space(const std::string& filename) {
    auto id_format = [](const StateId& id) {
        return std::string("E") + std::to_string(id.exp_id) + "S" +
            std::to_string(id.sym_id);
    };

    auto label_format = [this](const StateId& vertex_id) {
        std::string ba_state;
        std::string control;
        try {
            Blob b = knowns.getState(vertex_id);
            eval.read(b.getExpl());
            std::stringstream s;
            auto* state = eval.getState();
            s << state->control << "\\n" << state->explicitData
                << "\\n" << state->data;
            control = s.str();
        }
        catch (const DatabaseException& e) {
            ba_state = e.what();
        }
        return "<" + std::to_string(vertex_id.exp_id) + ", "
            + std::to_string(vertex_id.sym_id) + ">\\n"
            + control;
    };

    auto style_format = [this](const StateId& vertex_id)->std::string {
        return "style=\"filled\" fillcolor=\"#FFC65D\"";
    };

    std::ofstream o(filename);
    graph.to_dot(o, id_format, label_format, style_format);
}