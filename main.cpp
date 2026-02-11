#include <Kokkos_Core.hpp>
#include <vector>
#include <iostream>
#include <chrono>
#include <string>

#include "src/graph.hpp"
#include "src/graph_builder.hpp"
#include "src/initialize_algorithm.hpp"
#include "src/preprocessing/dimacs_par_loader.hpp"
#include "src/push_relabel.hpp"

int main(int argc, char *argv[])
{

    std::string graph_path = "./input/mock/generated_graph.dimacs";
    unsigned int tc = 0;
    if (argc > 1)
    {
        graph_path = argv[1];
    }
    if (argc > 2)
    {
        tc = (unsigned int)std::stoi(argv[2]);
    }

    Kokkos::initialize(argc, argv);
    {
        using Device = Kokkos::DefaultExecutionSpace;
        std::cout << "Running on: " << typeid(Device).name() << "\n";
        std::cout << "Loading graph from: " << graph_path << "\n";
        // IO & Setup
        int N = 0, s = -1, t = -1;
        Kokkos::Timer timer;
        const auto raw_edges = parallel_load_dimacs(graph_path, N, s, t, tc);
        auto timerIO = timer.seconds();
        std::cout << "IO time   :       " << timerIO << " [s]\n";

        if (s == -1)
            s = 0; // Default assumption
        if (t == -1)
            t = N - 1; // Default assumption
        if (N == 0)
            throw std::runtime_error("Header parse failed or empty graph");

        // Build Graph
        timer.reset();
        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);
        auto timerBUILD = timer.seconds();
        std::cout << "BUILD time:       " << timerBUILD << " [s]\n";

        // Init (Saturate S)
        timer.reset();
        initialize_algorithm(g, s, t, N);
        auto timerINIT = timer.seconds();
        std::cout << "INIT time :       " << timerINIT << " [s]\n";

        // Solve
        long long max_flow = 0;
        timer.reset();
        PushRelabelSolver<Device>::solve(g, s, t, N, max_flow);
        auto timerSOLVE = timer.seconds();
        std::cout << "SOLVE time:       " << timerSOLVE << " [s]\n";
        std::cout << "---------------------------------" << "\n";
        std::cout << "TOTAL TIME IS     " << timerIO + timerBUILD + timerINIT + timerSOLVE << " [s]\n";
        std::cout << "MAX FLOW IS       " << max_flow << "\n";
    }
    Kokkos::finalize();
    return 0;
}