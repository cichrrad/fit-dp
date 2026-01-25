#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>
#include "input_edge.hpp"

/*
FORMAT:
line 1: source sink -1
line 2+: u v cap
*/

inline std::vector<InputEdge> parse_csv(const std::string& filename, int& num_nodes, int& source, int& sink) {
    std::vector<InputEdge> edges;
    std::ifstream file(filename);
    std::string line;
    
    num_nodes = 0;
    source = 0;
    sink = 0;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return edges;
    }

    // READ HEADER
    int header_s = -1, header_t = -1, header_dummy = -1;
    if (std::getline(file, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::stringstream ss(line);
        ss >> header_s >> header_t >> header_dummy;
    } else {
        return edges; // Empty file
    }

    // READ EDGES
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue; // Skip comments

        std::replace(line.begin(), line.end(), ',', ' ');
        std::stringstream ss(line);
        int u, v;
        long long c = 1LL; 
        
        if (!(ss >> u >> v)) continue; 
        if (ss >> c) { /* capacity read */ }

        edges.push_back({u, v, c});
        
        // Dynamically track max ID
        num_nodes = std::max(num_nodes, std::max(u, v) + 1);
    }

    // CONFIGURE SOURCE/SINK
    if (header_s == header_t) {
        // "Unspecified" case -> use defaults based on graph size
        source = 0;
        sink = num_nodes - 1;
    } else {
        // Specified case
        source = header_s;
        sink = header_t;
    }

    return edges;
}