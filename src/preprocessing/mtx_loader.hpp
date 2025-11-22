#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>
#include "input_edge.hpp"

inline std::vector<InputEdge> parse_mtx(const std::string& filename, int& num_nodes) {
    std::vector<InputEdge> edges;
    std::ifstream file(filename);
    std::string line;
    num_nodes = 0;

    bool is_symmetric = false;
    bool is_pattern = false;
    bool header_found = false;

    // 1. Parse Header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // Check for Header Signature
        if (line.find("%%MatrixMarket") == 0) {
            std::stringstream ss(line);
            std::string object, format, field, symmetry;
            // Skip "%%MatrixMarket"
            std::string dummy; 
            ss >> dummy >> object >> format >> field >> symmetry;

            if (symmetry == "symmetric" || symmetry == "hermitian") {
                is_symmetric = true;
            }
            if (field == "pattern") {
                is_pattern = true;
            }
            header_found = true;
            continue; // Done with this line
        }

        // Skip comments
        if (line[0] == '%') continue;

        // If we hit a non-comment, non-header line, it's the Size Line
        break; 
    }

    if (!header_found) {
        std::cerr << "Warning: MTX file missing standard header. Assuming general/real.\n";
    }

    // 2. Parse Size Line: Rows Cols Entries
    int rows = 0, cols = 0, lines = 0;
    std::stringstream ss_size(line);
    ss_size >> rows >> cols >> lines;
    
    num_nodes = std::max(rows, cols);

    // 3. Parse Data Lines
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '%') continue;
        
        std::stringstream ss(line);
        int u, v;
        double w = 1.0; 
        
        ss >> u >> v;
        
        // MTX is 1-based -> convert to 0-based
        u -= 1; 
        v -= 1;

        if (!is_pattern) {
            ss >> w; // Read weight if not a pattern
        }

        long long cap = static_cast<long long>(w);

        // Add Forward Edge
        edges.push_back({u, v, cap});

        // If symmetric and not a self-loop, explicitly add the reverse edge
        if (is_symmetric && u != v) {
            edges.push_back({v, u, cap});
        }
    }
    return edges;
}