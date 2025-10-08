#ifndef DINIC_BGL_HPP
#define DINIC_BGL_HPP

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <vector>

namespace my_dinic {

using Traits =
    boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::directedS>;
using EdgeDesc = Traits::edge_descriptor;

using Graph = boost::adjacency_list<
    boost::vecS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<
        boost::edge_capacity_t, long,
        boost::property<boost::edge_residual_capacity_t, long,
                        boost::property<boost::edge_reverse_t, EdgeDesc>>>>;

using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using Edge = boost::graph_traits<Graph>::edge_descriptor;

using CapMap = boost::property_map<Graph, boost::edge_capacity_t>::type;
using ResMap =
    boost::property_map<Graph, boost::edge_residual_capacity_t>::type;
using RevMap = boost::property_map<Graph, boost::edge_reverse_t>::type;

Edge add_edge_with_reverse(Graph &g, Vertex u, Vertex v, long c, CapMap &cap,
                           ResMap &res, RevMap &rev);

inline Edge add_edge_with_capacity(Graph &g, Vertex u, Vertex v, long c) {
  CapMap cap = get(boost::edge_capacity, g);
  ResMap res = get(boost::edge_residual_capacity, g);
  RevMap rev = get(boost::edge_reverse, g);
  return add_edge_with_reverse(g, u, v, c, cap, res, rev);
}

struct Dinic {
  Graph &g;
  CapMap cap;
  ResMap res;
  RevMap rev;

  std::vector<int> level;

  using OEI = boost::graph_traits<Graph>::out_edge_iterator;
  std::vector<OEI> cur, cur_end;

  explicit Dinic(Graph &g);

  // Returns max-flow value; updates residual capacities in-place.
  long max_flow(Vertex s, Vertex t);

private:
  bool bfs(Vertex s, Vertex t);
  long dfs(Vertex u, Vertex t, long pushed);
};

} // namespace my_dinic

#endif // DINIC_BGL_HPP
