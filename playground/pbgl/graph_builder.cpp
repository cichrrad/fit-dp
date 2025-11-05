#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/property_map/parallel/parallel_property_maps.hpp>
#include <boost/property_map/parallel/vector_property_map.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

using ED = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::edge_descriptor;
using VD = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::vertex_descriptor;

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, no_property, property<edge_capacity_t, int, property<edge_reverse_t, ED>>>;

int main(int argc, char **argv)
{
    mpi::environment env(argc, argv);
    mpi_process_group pg;
    int pid = process_id(pg), np = num_processes(pg);

    Graph g(6, pg);

    // assume edges list {u,v} from file etc.
    std::vector<std::pair<int, int>> edge_list =
        {
            {0, 1},
            {0, 2},
            {1, 2},
            {1, 3},
            {2, 4},
            {3, 5},
            {4, 5},

        };

    // note -- we could split edge_list
    // between processes ?
    if (pid == 0)
    {
        // https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/distributed_adjacency_list.html#building-a-distributed-graph
        // building a dist. graph 2]
        for (const auto &p : edge_list)
        {
            auto u = vertex(p.first, g);
            auto v = vertex(p.second, g);
            add_edge(u, v, g);
            // add reverse too
            add_edge(v, u, g);
        }
    }

    synchronize(g);

    auto rev_map = get(edge_reverse, g);
    auto cap_map = get(edge_capacity, g);
    // set up edge properties (each process sets up his own)
    // if each process got just their
    // part of edge_list
    // no checks would be needed
    for (const auto &p : edge_list)
    {
        auto u = vertex(p.first, g);
        if (u.owner == pid)
        {
            auto v = vertex(p.second, g);
            // .second is ok flag
            auto e = edge(u, v, g).first;

            // this blows up -- cannot access ED
            // of remote edge

            // auto erev = edge(v, u, g).first;
            // put(rev_map, e, erev);
            //  reverse is handled by
            //  owner of erev

            // set my capacity
            // pid to see ownership matters
            put(cap_map, e, (pid));
        }
    }

    synchronize(rev_map);
    synchronize(cap_map);

    // cout to verify
}