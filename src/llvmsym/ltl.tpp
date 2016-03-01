#pragma once

template <class Store, class Hit>
Ltl<Store, Hit>::Ltl(const std::string& model, const std::string& prop)
	: model_name(model), ba("! (" + prop + ")")
{
	// ToDo: Check if the file is valid
}

template <class Store, class Hit>
void Ltl<Store, Hit>::run() {
	std::shared_ptr<BitCode> bc = std::make_shared<BitCode>(model_name);
	Evaluator<Store> eval(bc);

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

	run_nested_dfs(eval, initial_id);
}

template <class Store, class Hit>
void Ltl<Store, Hit>::run_nested_dfs(Evaluator<Store>& eval, StateId start_vertex) {
	if (!graph.exists(start_vertex))
		throw LtlException("Initial vertex not found!");

	std::stack<StateId> to_process;
	to_process.push(start_vertex);
	bool accepting_found = false;

	while (!accepting_found && !to_process.empty()) {
		auto vertex_id = to_process.top();
		VertexInfo& info = graph.get_vertex_info(vertex_id);
		Blob b = knowns.getState(vertex_id);
		if (info.outer_color == VertexColor::WHITE) {
			// New vertex, generate successors & change color
			const auto& succs = graph.get_successors(vertex_id);
			std::cout << "\nEntering vertex: <" << vertex_id.exp_id << ", "
				<< vertex_id.sym_id << ", " << b.user_as<index_type>() << ">\n";
			if (succs.empty()) {
				// We have to generate new successors
				std::vector<StateId> successors_ids;
                std::vector<StateId> successors_to_process;

				// Generate product with BA
				auto ba_graph = ba.get_ba();
				index_type ba_state = b.user_as<index_type>();
				auto ba_succ = ba_graph.get_successors(ba_state);
				auto ba_pc = ba_graph.get_successors_edges(ba_state);

				for (size_t i = 0; i != ba_succ.size(); i++) {
					eval.read(b.getExpl());
					// Push proposition guard
                    std::cout << "Pushing: " << ba_succ[i] << ": " << ba_pc[i].ap << "\n";
					eval.getState()->data.pushPropGuard(ba_pc[i].ap);
                    eval.getState()->properties.empty |= eval.getState()->data.empty();
					eval.advance([&]() {
                        if (eval.is_empty())
                            return;

						Blob newSucc(eval.getSize() + sizeof(index_type),
							eval.getExplicitSize(), sizeof(index_type));
						eval.write(newSucc.getExpl());
    					newSucc.user_as<index_type>() = ba_succ[i]; // Update BA state

						std::cout << "New succ produced\n";
						auto result = knowns.insertCheck(newSucc);
						if (result.first) {
							if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
								static int succs_total = 0;
								std::cerr << ++succs_total << " states so far.\n";
							}
                            successors_to_process.push_back(result.second);
						}
                        successors_ids.push_back(result.second);
					});
				}

                /*std::random_shuffle(successors_to_process.begin(),
                    successors_to_process.end());*/
                graph.add_successors(vertex_id, successors_ids);
    			/*if (successors_ids.empty())
        			graph.add_edge(vertex_id, vertex_id);*/

                std::cout << "Successors: ";
                for (const auto& id : successors_to_process) {
                    to_process.push(id);
                    std::cout << "<" << id.exp_id << ", " << id.sym_id << "> ";
                }
                std::cout << "\n";

                std::cout << successors_ids.size() << " successors generated\n";
			}
			info.outer_color = VertexColor::GRAY;
		}
		else if (info.outer_color == VertexColor::GRAY) {
			std::cout << "Backtracking vertex: <" << vertex_id.exp_id << ", "
				<< vertex_id.sym_id << ", " << b.user_as<index_type>() << ">\n";
			info.outer_color = VertexColor::BLACK;
			to_process.pop();

			// Backtrack
			// Check if state is accepting
			index_type ba_vertex = b.user_as<index_type>();
			if (ba.get_ba().get_vertex_info(ba_vertex))
				accepting_found |= run_inner_dfs(vertex_id);
			/*if (accepting_found)
				break;*/
		}
		else if (info.outer_color == VertexColor::BLACK) {
			// Vertex was put multiple times onto stack
			to_process.pop();
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

	std::cout << "Running inner DFS for vertex " << start_vertex << "\n";

	std::stack<StateId> to_process;
	to_process.push(start_vertex);
	//to_process.push(start_vertex);
	bool accepting_found = false;

	while (!to_process.empty()) {
		auto vertex_id = to_process.top();
		VertexInfo& info = graph.get_vertex_info(vertex_id);
		if (info.inner_color == VertexColor::WHITE) {
			auto succ = graph.get_successors(vertex_id);
			auto succ_info = graph.get_successors_info(vertex_id);

			for (size_t i = 0; i != succ.size(); i++) {
                std::cout << "<" << succ[i].exp_id << ", " << succ[i].sym_id << ">\n";
				if (/*succ_info[i].inner_color == VertexColor::GRAY
					|| */start_vertex == succ[i])
				{
					std::cout << "Cycle from accepting state found!\n";
					if (start_vertex == succ[i])
						std::cout << "Original vertex found!\n";
					accepting_found = true;
				}
				if (succ_info[i].inner_color == VertexColor::WHITE)
					to_process.push(succ[i]);
			}

			info.inner_color = VertexColor::GRAY;
		}
		else if (info.inner_color == VertexColor::GRAY) {
			info.inner_color = VertexColor::BLACK;
			to_process.pop();
		}
		else if (info.inner_color == VertexColor::BLACK) {
			to_process.pop();
		}
	}
	return accepting_found;
}

template <class Store, class Hit>
void Ltl<Store, Hit>::output_state_space(const std::string& filename) {
    std::ofstream o(filename);

    std::shared_ptr<BitCode> bc = std::make_shared<BitCode>(model_name);
    Evaluator<Store> eval(bc);
    
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

    auto label_format = [this, &eval](const StateId& vertex_id) {
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