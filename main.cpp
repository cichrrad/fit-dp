#include <Kokkos_Core.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>

#include "src/graph.hpp"
#include "src/graph_builder.hpp"
#include "src/initialize_algorithm.hpp"
#include "src/preprocessing/csv_loader.hpp"

int main(int argc, char *argv[])
{
    int N;
    const auto raw_edges = parse_csv("./input/mock/mock_graph.csv", N);
    int s = 0;
    int t = N - 1;

    std::cout << "Parsed graph with " << N << " vertices. Source is " << s << " and sink is " << t << ".\n";

    Kokkos::initialize(argc, argv);
    {
        using Device = Kokkos::DefaultExecutionSpace;
        std::cout << "\nKokkos initialized on: " << typeid(Device).name() << "\n";

        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);
        initialize_algorithm(g,s,t,N);

        auto h_excess = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.excess);
        auto h_label = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.label);
        auto h_row_map = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.row_map);
        auto h_entries = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.entries);
        auto h_residual = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.residual_capacity);
        auto h_q_size = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.current_queue_size);
        auto h_active = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), g.current_active);

        // Print Nodes
        std::cout << "\n[NODE STATE]\n";
        std::cout << "Node | Label | Excess | Status\n";
        for(int i=0; i<N; ++i) {
            std::cout << std::setw(4) << i << " | " 
                      << std::setw(5) << h_label(i) << " | " 
                      << std::setw(6) << h_excess(i) << " | ";
            if(i == s) std::cout << "SOURCE";
            else if(i == t) std::cout << "SINK";
            else if(h_excess(i) > 0) std::cout << "ACTIVE";
            std::cout << "\n";
        }

        // Print Queue
        std::cout << "\n[INITIAL QUEUE]\n";
        std::cout << "Size: " << h_q_size() << "\n";
        std::cout << "Contents: [ ";
        for(size_t i=0; i<h_q_size(); ++i) std::cout << h_active(i) << " ";
        std::cout << "]\n";

        // Print Edges
        std::cout << "\n[EDGE STATE (Residual Capacities)]\n";
        for(int u=0; u<N; ++u) {
            for(int i=h_row_map(u); i<h_row_map(u+1); ++i) {
                int v = h_entries(i);
                std::cout << u << " -> " << v << " : " << h_residual(i) << "\n";
            }
        }

    }
};
