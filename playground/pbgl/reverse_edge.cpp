#include <boost/graph/use_mpi.hpp>
#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

using ED = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::edge_descriptor;
using Graph = adjacency_list<
    vecS,
    distributedS<mpi_process_group, vecS>,
    directedS,
    no_property,
    property<edge_capacity_t, int, property<edge_reverse_t, ED>>>;

int main(int argc, char **argv)
{
    mpi::environment env(argc, argv);
    mpi_process_group pg;
    int pid = process_id(pg), np = num_processes(pg);
    if (np < 2 || np > 3)
        return 1;

    Graph g(4, pg);

    auto u = vertex(0, g);
    auto v = vertex(3, g);

    ED e_uv{}, e_vu{};

    if (pid == 0)
    {
        e_uv = add_edge(u, v, g).commit().first;
        e_vu = add_edge(v, u, g).commit().first;
    }

    // 1) Flush graph structure first
    synchronize(g);

    auto rev = get(edge_reverse, g);
    auto cap = get(edge_capacity, g);

    if (pid == 0)
    {
        // fill both sides (PBGL will route remote puts)
        put(rev, e_uv, e_vu);
        put(rev, e_vu, e_uv);
        put(cap, e_uv, 10);
        put(cap, e_vu, 5);
    }

    // 2) Now flush map updates that had remote puts
    synchronize(rev);
    synchronize(cap);

    if (pid == 0)
    {
        int reverse_cap = get(cap, get(rev, e_uv)); // capacity of v->u
        std::cout << "reverse edge cap is " << reverse_cap << "\n";
    }
}
