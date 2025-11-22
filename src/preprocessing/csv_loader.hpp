#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include "input_edge.hpp"

/*
u v cap
u1 v1 cap1
...  
un vn capn
 */
// number of nodes implicit by max id

inline std::vector<InputEdge> parse_csv(const std::string& filename, int& num_nodes) {
    std::vector<InputEdge> edges;
    std::ifstream file(filename);
    std::string line;
    num_nodes = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments
        
        // Replace commas with spaces for easier parsing
        std::replace(line.begin(), line.end(), ',', ' ');
        
        std::stringstream ss(line);
        int u, v;
        long long c = 1LL; // Default capacity
        
        if (!(ss >> u >> v)) continue; // Skip malformed lines
        if (ss >> c) { /* capacity read */ }

        edges.push_back({u, v, c});
        
        // Dynamically update num_nodes based on max index seen
        // (Assuming 0-based index in CSVs usually)
        num_nodes = std::max(num_nodes, std::max(u, v) + 1);
    }
    return edges;
}