
#include <boost/graph/use_mpi.hpp>

#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/property_map/parallel/parallel_property_maps.hpp>
#include <boost/property_map/parallel/vector_property_map.hpp>

using namespace boost;
using boost::graph::distributed::mpi_process_group;

/*
https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/distributed_property_map.html#consistency-models
*/
using boost::parallel::cm_backward;
using boost::parallel::cm_bidirectional;
using boost::parallel::cm_forward;

using e_static = property<edge_index_t, std::size_t>;

using ed = adjacency_list_traits<vecS, distributedS<mpi_process_group, vecS>, directedS>::edge_descriptor;

using Graph = adjacency_list<vecS, distributedS<mpi_process_group, vecS>,
                             directedS, no_property, property<edge_index_t, std::size_t, property<edge_reverse_t, ed>>>;

int main(int argc, char **argv)
{

  //===================================
  // [0] MPI & PROPERTIES SETUP
  //===================================

  mpi::environment env(argc, argv);
  mpi_process_group pg;
  auto pid = process_id(pg);
  auto np = num_processes(pg);

  const std::size_t N = 6;
  // TODO REMOVE THIS CRUX !!!!!
  if (N != 6 || np != 2)
  {
    return -1;
  }

  // split between pg processes
  Graph g(N, pg);

  // properties setup

  // vertex and edge indexing
  auto vid = get(vertex_index, g);
  auto eid = get(edge_index, g);

  // map indexing onto properties
  // VERTEX PROPERTIES
  vector_property_map<unsigned long, decltype(vid)> height(vid);
  vector_property_map<unsigned long, decltype(vid)> new_height(vid);
  vector_property_map<unsigned long, decltype(vid)> excess(vid);
  vector_property_map<unsigned long, decltype(vid)> added_excess(vid);
  vector_property_map<unsigned long, decltype(vid)> work(vid);
  vector_property_map<unsigned char, decltype(vid)> discovered(vid);
  // EDGE PROPERTIES
  vector_property_map<unsigned long, decltype(eid)> capacity(eid);
  vector_property_map<unsigned long, decltype(eid)> residual(eid);
  auto reverse_edge = get(edge_reverse, g);
  // CONSISTENCY MODELS
  // V
  height.set_consistency_model(cm_bidirectional);
  new_height.set_consistency_model(cm_forward);
  excess.set_consistency_model(cm_backward);
  added_excess.set_consistency_model(cm_forward);
  work.set_consistency_model(cm_backward);
  discovered.set_consistency_model(cm_forward);
  // E
  capacity.set_consistency_model(cm_backward);
  residual.set_consistency_model(cm_bidirectional);
  reverse_edge.set_consistency_model(cm_backward);

  //===================================
  // [1] INITIALIZATION
  //===================================

  // dummy input
  //                      <u            , v            , cap          >
  using eConf = std::tuple<unsigned long, unsigned long, unsigned long>;
  std::vector<eConf> edgeV;

  // each pid wires up their vertices
  // (DEMO ASSUME -np 2 and 6 vertices in graph!)
  // s = 0; t = 5;
  long sid = 0;
  long tid = 5;

  if (pid == 0)
  {
    edgeV.push_back({0ul, 1ul, 10ul});
    edgeV.push_back({0ul, 2ul, 7ul});
    edgeV.push_back({1ul, 2ul, 4ul});
    // Cross edges
    edgeV.push_back({1ul, 3ul, 8ul});
    edgeV.push_back({2ul, 4ul, 4ul});
  }
  if (pid == 1)
  {
    edgeV.push_back({3ul, 5ul, 2ul});
    edgeV.push_back({4ul, 5ul, 2ul});
  }

  // GRAPH BUILDING
  // VERTICES
  graph_traits<Graph>::vertex_iterator v, vend;
  for (tie(v, vend) = vertices(g); v != vend; v++)
  {
    height[*v] = (vid[*v] == sid ? N : 0);
    new_height[*v] = (height[*v]);
    excess[*v] = 0;
    added_excess[*v] = 0;
    work[*v] = 0;
    discovered[*v] = false;
    std::cout << "vertex id is " << get(vid, *v) << "\n";
  }
  // EDGES
  struct AddedEdge
  {
    ed uv, vu;
    unsigned long cap;
  };
  std::vector<AddedEdge> added;
  for (std::size_t i = 0; i < edgeV.size(); ++i)
  {
    const auto &e = edgeV[i];
    const auto &uId = std::get<0>(e);
    const auto &vId = std::get<1>(e);
    const auto &cap = std::get<2>(e);

    auto u = vertex(uId, g);
    if (u.owner != pid)
      return 1;

    auto v = vertex(vId, g);
    auto uv = add_edge(u, v, g).commit().first;
    auto vu = add_edge(v, u, g).commit().first;

    put(reverse_edge, uv, vu);
    put(reverse_edge, vu, uv);

    added.push_back({uv, vu, cap});
  }

  std::size_t eid_l = 0;
  graph_traits<Graph>::edge_iterator ei, ei_end;
  for (tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
  {
    put(eid, *ei, eid_l++); // 0..m_local-1
  }

  for (const auto &a : added)
  {
    put(capacity, a.uv, a.cap);
    put(residual, a.uv, a.cap);

    put(capacity, a.vu, 0ul);
    put(residual, a.vu, 0ul);
  }

  synchronize(height);
  synchronize(new_height);
  synchronize(excess);
  synchronize(added_excess);
  synchronize(work);
  synchronize(discovered);
  synchronize(capacity);
  synchronize(residual);
  synchronize(reverse_edge);
  synchronize(pg);

  auto vlabel = [&](auto v) -> std::string
  {
    if (v.owner == pid)
    {
      return "V" + std::to_string(vid[v]); // safe to read local vertex_index
    }
    else
    {
      // remote: don't touch vertex_index; show owner:local descriptor
      return "V(" + std::to_string(v.owner) + ":" + std::to_string(v.local) + ")";
    }
  };

  for (int turn = 0; turn < np; ++turn)
  {
    if (pid == turn)
    {
      std::cout << "=============================\n";
      std::cout << "Rank " << pid << " local view\n";
      std::cout << "=============================\n";

      graph_traits<Graph>::vertex_iterator v_it, v_end;
      for (tie(v_it, v_end) = vertices(g); v_it != v_end; ++v_it)
      {
        auto x = *v_it;
        if (x.owner != pid)
          continue;

        std::cout << vlabel(x)
                  << " (owner=" << x.owner << " local=" << x.local << "): "
                  << "h=" << height[x]
                  << " nh=" << new_height[x]
                  << " ex=" << excess[x]
                  << " add_ex=" << added_excess[x]
                  << " work=" << work[x]
                  << " disc=" << static_cast<int>(discovered[x])
                  << "\n";

        graph_traits<Graph>::out_edge_iterator ei, ei_end;
        for (tie(ei, ei_end) = out_edges(x, g); ei != ei_end; ++ei)
        {
          auto e = *ei;          // local edge (owner is this rank)
          auto a = source(e, g); // local
          auto b = target(e, g); // may be remote
          auto re = get(reverse_edge, e);

          std::cout << "  e[" << get(eid, e) << "]: "
                    << vlabel(a) << " -> " << vlabel(b)
                    << " | cap=" << get(capacity, e)
                    << " res=" << get(residual, e);

          std::cout << " | rev ";
          if (re.owner() == pid)
          {
            std::cout << "idx=" << get(eid, re);
            bool rr_ok = (get(reverse_edge, re) == e);
            bool endpoints_ok = (source(re, g) == b) && (target(re, g) == a);
            std::cout << " | rr_ok=" << (rr_ok ? "Y" : "N")
                      << " | endpoints_ok=" << (endpoints_ok ? "Y" : "N");
          }
          else
          {
            std::cout << "owner=" << re.owner();
          }
          std::cout << "\n";
        }
      }
      std::cout.flush();
    }
    boost::graph::distributed::synchronize(pg); // print rank-by-rank
  }

  if (pid == 0)
    std::cout << "done\n";
  return 0;
}

// TODO REVERSE EDGE ID IS GARBAGE