#include <Kokkos_Core.hpp>
#include <vector>
#include <iostream>
#include <chrono>

#include "src/graph.hpp"
#include "src/graph_builder.hpp"
#include "src/initialize_algorithm.hpp"
#include "src/preprocessing/dimacs_par_loader.hpp"
#include "src/push_relabel.hpp"

int main(int argc, char *argv[])
{
    Kokkos::initialize(argc, argv);
    {
        using Device = Kokkos::DefaultExecutionSpace;
        std::cout << "Running on: " << typeid(Device).name() << "\n";
        // IO & Setup
        int N, s, t;
        Kokkos::Timer timer;
        const auto raw_edges = parallel_load_dimacs("./input/mock/generated_graph.dimacs", N, s, t);
        auto timerIO = timer.seconds();
        
        // Build Graph
        timer.reset();
        auto g = GraphBuilder::build_graph<Device>(raw_edges, N, s, t);
        auto timerBUILD = timer.seconds();


        // Init (Saturate S)
        timer.reset();
        initialize_algorithm(g, s, t, N);
        auto timerINIT = timer.seconds();
        
        // Solve
        long long max_flow = 0;
        timer.reset();
        PushRelabelSolver<Device>::solve(g, s, t, N, max_flow);
        auto timerSOLVE = timer.seconds();

        std::cout << "IO time   :       " << timerIO        <<  " [s]\n";
        std::cout << "BUILD time:       " << timerBUILD     <<  " [s]\n";
        std::cout << "INIT time :       " << timerINIT      <<  " [s]\n";
        std::cout << "SOLVE time:       " << timerSOLVE     <<  " [s]\n";
        std::cout << "---------------------------------"    <<  "\n";
        std::cout << "TOTAL TIME IS     " << timerIO + timerBUILD + timerINIT + timerSOLVE << " [s]\n";
        std::cout << "MAX FLOW IS       " << max_flow << "\n";
    }
    Kokkos::finalize();
    return 0;
}