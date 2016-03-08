#pragma once

template <class Store, class Hit>
Ltl<Store, Hit>::Ltl(const std::string& model_name, const std::string& prop,
    bool depth_bounded)
	: eval(std::make_shared<BitCode>(model_name)), depth_bounded(depth_bounded),
      ba("! (" + prop + ")")
{
}

template <class Store, class Hit>
void Ltl<Store, Hit>::run() {
	// Init blob and reserve space for info about BA state in user section
	Blob initial(eval.getSize()
		+ sizeof(index_type), eval.getExplicitSize(), sizeof(index_type));
	// Read out the initial state from evaluator
	eval.write(initial.getExpl());

	index_type ba_start = ba.get_init_vert();
	auto ba_succ = ba.get_ba().get_successors(ba_start);
	assert(ba_succ.size() == 1);
	ba_start = ba_succ.front();

	initial.user_as<index_type>() = ba_start;

	// Insert initial state to database and start point of the buchi automaton
	StateId initial_id = knowns.insert(initial);
	graph.add_vertex(initial_id);

	run_nested_dfs(initial_id);
}

template <class Store, class Hit>
void Ltl<Store, Hit>::reset_dfs() {
    graph.transform_vertices([&](VertexInfo& info) {
        info.reset();
    });
}

template <class Store, class Hit>
std::vector<StateId> Ltl<Store, Hit>::generate_successors(StateId vertex_id)
{
    Blob state = knowns.getState(vertex_id);

    // Get successors of current BA states
    auto ba_graph = ba.get_ba();
    index_type ba_state = state.user_as<index_type>();
    auto ba_succ = ba_graph.get_successors(ba_state);
    auto ba_pc = ba_graph.get_successors_edges(ba_state);

    std::vector<StateId> successors;

    // Make product with the BA
    for (size_t i = 0; i != ba_succ.size(); i++) {
        eval.read(state.getExpl());
        // Push proposition guard 
        eval.getState()->data.pushPropGuard(ba_pc[i].ap);
        eval.getState()->properties.empty |= eval.getState()->data.empty();

        eval.advance([&]() {
            if (eval.is_empty())
                return;

            Blob newSucc(eval.getSize() + sizeof(index_type),
                eval.getExplicitSize(), sizeof(index_type));
            eval.write(newSucc.getExpl());
            newSucc.user_as<index_type>() = ba_succ[i]; // Update BA state

            if (Config.is_set("--verbose")) {
                std::cout << "New succ produced\n";
            }
            auto successor_id = knowns.insertCheck(newSucc).second;
            successors.push_back(successor_id);
        });
    }
    
    return successors;
}

template <class Store, class Hit>
void Ltl<Store, Hit>::run_nested_dfs(StateId start_vertex) {
	if (!graph.exists(start_vertex))
		throw LtlException("Initial vertex not found!");

	std::stack<std::vector<StateId>> to_process;
    to_process.push({ start_vertex });

	bool accepting_found     = false;
    bool depth_bound_reached = false;
    size_t depth_bound       = 32;

	while (!accepting_found && !to_process.empty()) {
        auto& top = to_process.top();
        if (top.empty()) { // No successors left
            to_process.pop();
            continue;
        }
		auto vertex_id = top.back();
		VertexInfo& info = graph.get_vertex_info(vertex_id);	

        // Check depth bound
        if (depth_bounded && info.depth >= depth_bound) {
            to_process.pop();
            depth_bound_reached = true;
            continue;
        }      
        
		if (info.outer_color == VertexColor::WHITE) {
			// New vertex, generate successors & change color
			auto successors = graph.get_successors(vertex_id);
            if (Config.is_set("--verbose")) {
                std::cout << "\nEntering vertex: <" << vertex_id.exp_id << ", "
                    << vertex_id.sym_id << ">\n";
            }
			if (successors.empty()) {
                // There are no known successors, so we generate them
                successors = generate_successors(vertex_id);
                // Default item info
                std::vector<VertexInfo> successors_info(successors.size(),
                    { VertexColor::WHITE, VertexColor::WHITE, info.depth + 1 });
                graph.add_successors(vertex_id, successors,
                    successors_info, std::vector<NoInfo>(successors_info.size()));
			}
            to_process.push(successors);

            info.outer_color = VertexColor::GRAY;
		}
		else if (info.outer_color == VertexColor::GRAY) {
            Blob b = knowns.getState(vertex_id);
            if (Config.is_set("--verbose")) {
                std::cout << "Backtracking vertex: <" << vertex_id.exp_id << ", "
                    << vertex_id.sym_id << ", " << b.user_as<index_type>() << ">\n";
            }
			info.outer_color = VertexColor::BLACK;

            // Remove vertex
            top.pop_back();

			// Backtrack and check if the BA state is accepting
			index_type ba_vertex = b.user_as<index_type>();
			if (ba.get_ba().get_vertex_info(ba_vertex))
				accepting_found |= run_inner_dfs(vertex_id);
			if (accepting_found)
				break;
		}
		else if (info.outer_color == VertexColor::BLACK) {
			// Vertex was put multiple times onto stack
			top.pop_back();
		}

        // Check if the DFS is ending
        if (depth_bounded && to_process.empty() && depth_bound_reached) {
            reset_dfs();
            depth_bound *= 2;
            depth_bound_reached = false;
            to_process.push({ start_vertex });
            std::cout << "   Starting new depth " << depth_bound << "\n"
                         "============================\n";
        }
	}

	if (accepting_found)
		std::cout << "Property violated!\n";
	else
		std::cout << "Property holds!\n";
}

template <class Store, class Hit>
bool Ltl<Store, Hit>::run_inner_dfs(StateId start_vertex) {
	if (!graph.exists(start_vertex))
		throw LtlException("Initial vertex not found!");

    if (Config.is_set("--verbose")) {
        std::cout << "Running inner DFS for vertex " << start_vertex << "\n";
    }

	std::stack<std::vector<StateId>> to_process;
    to_process.push({ start_vertex });
	bool accepting_found = false;

	while (!accepting_found && !to_process.empty()) {
        auto& top = to_process.top();
        if (top.empty()) {
            to_process.pop();
            continue;
        }
		auto vertex_id = top.back();
		VertexInfo& info = graph.get_vertex_info(vertex_id);
		if (info.inner_color == VertexColor::WHITE) {
			auto succ = graph.get_successors(vertex_id);
			auto succ_info = graph.get_successors_info(vertex_id);

			for (size_t i = 0; i != succ.size(); i++) {
                if (Config.is_set("--verbose")) {
                    std::cout << "<" << succ[i].exp_id << ", "
                        << succ[i].sym_id << ">\n";
                }
				if (/*succ_info[i].inner_color == VertexColor::GRAY
					|| */start_vertex == succ[i])
				{
					std::cout << "Cycle from accepting state found!\n"
                        "Original vertex found!\n";
					accepting_found = true;
				}
			}
            to_process.push(succ);
			info.inner_color = VertexColor::GRAY;
		}
		else if (info.inner_color == VertexColor::GRAY) {
			info.inner_color = VertexColor::BLACK;
			top.pop_back();
		}
		else if (info.inner_color == VertexColor::BLACK) {
			top.pop_back();
		}
	}
	return accepting_found;
}

template <class Store, class Hit>
void Ltl<Store, Hit>::output_state_space(const std::string& filename) {
    std::ofstream o(filename);

    static const std::vector<std::string> colors = { 
        "F16745", // red
        "FFC65D", // yellow
        "7BC8A4", // green
        "4CC3D9", // blue
        "93648D"  // violet
    };

    auto id_format = [](const StateId& id) {
        return std::string("E") + std::to_string(id.exp_id) + "S" +
            std::to_string(id.sym_id);
    };

    auto label_format = [this](const StateId& vertex_id) {
        std::string ba_state;
        std::string control;
        try {
            Blob b = knowns.getState(vertex_id);
            ba_state = std::to_string(b.user_as<index_type>());
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
            + std::to_string(vertex_id.sym_id) + ", " + ba_state + ">\\n"
            + control;
    };
    
    auto style_format =[this](const StateId& vertex_id)->std::string {
        Blob b = knowns.getState(vertex_id) ;
        index_type ba_state = b.user_as<index_type>() ;
        if(ba_state < colors.size())
            return "style=\"filled\" fillcolor=\"#" + colors[ba_state] + "\"" ;
        return "";
    };

    graph.to_dot(o, id_format, label_format, style_format);
}