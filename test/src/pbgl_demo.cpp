// src/pbgl_demo.cpp

// must be first for PBGL
#include <boost/graph/use_mpi.hpp>

#include <boost/mpi.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>   // <-- needed
#include <boost/graph/distributed/adjacency_list.hpp>
#include <iostream>

int main(int argc, char** argv) {
    boost::mpi::environment env(argc, argv);
    boost::mpi::communicator world;

    using Graph = boost::adjacency_list<
        boost::vecS,                                                         // OutEdgeList
        boost::distributedS<boost::graph::distributed::mpi_process_group,    // Process group
                            boost::vecS>,                                    // Local vertex container
        boost::undirectedS
    >;

    Graph g(static_cast<std::size_t>(2 * world.size())); // toy graph
    if (world.rank() == 0) {
        std::cout << "PBGL ok, ranks=" << world.size() << "\n";
    }
}

