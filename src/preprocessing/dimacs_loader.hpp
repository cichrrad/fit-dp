#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include "input_edge.hpp"


inline std::vector<InputEdge> parse_dimacs(
    const std::string& filename, 
    int& num_nodes, 
    int& source, 
    int& sink
) {
    std::vector<InputEdge> edges;
    std::ifstream file(filename);
    std::string line;
    
    num_nodes = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        char type;
        ss >> type;

        if (type == 'c') {
            continue; // Comment
        } 
        else if (type == 'p') {
            // p max NODES ARCS
            std::string format;
            int m;
            ss >> format >> num_nodes >> m;
        } 
        else if (type == 'n') {
            // n ID WHICH (s or t)
            int id;
            char which;
            ss >> id >> which;
            
            // DIMACS is 1-based, convert to 0-based
            if (which == 's') source = id - 1;
            else if (which == 't') sink = id - 1;
        } 
        else if (type == 'a') {
            // a SRC DST CAP
            int u, v;
            long long c;
            ss >> u >> v >> c;
            // DIMACS is 1-based, convert to 0-based
            edges.push_back({u - 1, v - 1, c}); 
        }
    }

 
    return edges;
}