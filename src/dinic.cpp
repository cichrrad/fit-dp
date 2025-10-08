#include "dinic.hpp"

#include <algorithm>
#include <limits>
#include <queue>

namespace my_dinic {

Edge add_edge_with_reverse(Graph &g, Vertex u, Vertex v, long c, CapMap &cap,
                           ResMap &res, RevMap &rev) {
  bool ok;
  Edge e, er;
  tie(e, ok) = add_edge(u, v, g);
  tie(er, ok) = add_edge(v, u, g);

  cap[e] = c;
  cap[er] = 0;
  res[e] = c;
  res[er] = 0; // residuals start equal to capacities
  rev[e] = er;
  rev[er] = e; // cross-link

  return e;
}

Dinic::Dinic(Graph &g_)
    : g(g_), cap(get(boost::edge_capacity, g)),
      res(get(boost::edge_residual_capacity, g)),
      rev(get(boost::edge_reverse, g)) {
  const std::size_t n = num_vertices(g);
  level.assign(n, -1);
  cur.resize(n);
  cur_end.resize(n);
}

bool Dinic::bfs(Vertex s, Vertex t) {
  std::fill(level.begin(), level.end(), -1);
  std::queue<Vertex> q;
  level[s] = 0;
  q.push(s);

  while (!q.empty()) {
    Vertex u = q.front();
    q.pop();

    boost::graph_traits<Graph>::out_edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = out_edges(u, g); ei != ei_end; ++ei) {
      Edge e = *ei;
      if (res[e] <= 0)
        continue; // only edges with residual > 0
      Vertex v = target(e, g);
      if (level[v] == -1) {
        level[v] = level[u] + 1;
        q.push(v);
      }
    }
  }
  return level[t] != -1;
}

long Dinic::dfs(Vertex u, Vertex t, long pushed) {
  if (u == t || pushed == 0)
    return pushed;

  for (auto &it = cur[u]; it != cur_end[u]; ++it) {
    Edge e = *it;
    if (res[e] <= 0)
      continue;
    Vertex v = target(e, g);
    if (level[v] != level[u] + 1)
      continue;

    long can = (res[e] < pushed ? res[e] : pushed);
    long aug = dfs(v, t, can);
    if (aug > 0) {
      res[e] -= aug;
      Edge er = rev[e];
      res[er] += aug;
      return aug;
    }
  }
  return 0;
}

long Dinic::max_flow(Vertex s, Vertex t) {
  const std::size_t n = num_vertices(g);
  long flow = 0;
  const long INF = std::numeric_limits<long>::max();

  while (bfs(s, t)) {
    // initialize current arcs for this phase
    for (std::size_t u = 0; u < n; ++u) {
      auto p = out_edges(Vertex(u), g);
      cur[u] = p.first;
      cur_end[u] = p.second;
    }
    // push blocking flow
    while (true) {
      long aug = dfs(s, t, INF);
      if (aug == 0)
        break;
      flow += aug;
    }
  }
  return flow;
}

} // namespace my_dinic
