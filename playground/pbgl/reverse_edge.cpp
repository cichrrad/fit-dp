#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/property_map/parallel/parallel_property_maps.hpp>
#include <boost/property_map/parallel/vector_property_map.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

using ed = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::edge_descriptor;
using vd = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::vertex_descriptor;

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, no_property, property<edge_index_t, std::size_t, property<edge_reverse_t, ed>>>;

int main(int argc, char **argv)
{
    mpi::environment env(argc, argv);
    mpi_process_group pg;

    auto pid = process_id(pg);
    auto np = num_processes(pg);

    if (np < 2 || np > 3)
    {
        return 1;
    }

    Graph g(4, pg);

    auto u = vertex(0, g);
    auto v = vertex(3, g);
    auto rev_edges = get(edge_reverse, g);
    // make 0 do it so we know v is remote
    if (pid == 0)
    {
        // get just edge descriptors
        // (.second is 'ok' bool flag)
        ed e1 = add_edge(u, v, g).commit().first;
        ed e2 = add_edge(v, u, g).commit().first;
        put(rev_edges, e1, e2);
    }
    synchronize(rev_edges);
    synchronize(g);
    // std::cout << e1_rev.local << ":" << e1_rev.owner();
}