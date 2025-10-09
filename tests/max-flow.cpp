#include "../src/dinic.hpp"
#include "../src/ff.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>
#include <boost/graph/plod_generator.hpp>
#include <boost/graph/push_relabel_max_flow.hpp>

#include <boost/random/linear_congruential.hpp> // minstd_rand
#include <boost/random/mersenne_twister.hpp>    // mt19937 / mt19937_64
#include <boost/random/uniform_int_distribution.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random> // only for std::random_device
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace boost;

// ------------------------- Graph typedefs -------------------------
using Traits = adjacency_list_traits<vecS, vecS, directedS>;
using EdgeDesc = Traits::edge_descriptor;

using Graph =
    adjacency_list<vecS, vecS, directedS, no_property,
                   property<edge_capacity_t, long,
                            property<edge_residual_capacity_t, long,
                                     property<edge_reverse_t, EdgeDesc>>>>;

using Vertex = graph_traits<Graph>::vertex_descriptor;

// ------------------------- Helpers -------------------------
static EdgeDesc add_edge_with_capacity(Vertex u, Vertex v, long c, Graph &g) {
  auto cap = get(edge_capacity, g);
  auto res = get(edge_residual_capacity, g);
  auto rev = get(edge_reverse, g);

  bool ok;
  EdgeDesc e, er;
  tie(e, ok) = add_edge(u, v, g);
  tie(er, ok) = add_edge(v, u, g);

  cap[e] = c;
  cap[er] = 0;
  res[e] = c;
  res[er] = 0;
  rev[e] = er;
  rev[er] = e;
  return e;
}

struct Config {
  // graph size & capacity range
  int V = 10000;
  int cap_min = 1;
  int cap_max = 10000;

  // runs & RNG
  int runs = 10;
  uint64_t seed = 0;        // 0 => random_device
  bool fixed_graph = false; // reuse exact same edge list across runs

  // source/sink
  bool have_fixed_st = false;
  int s_fixed = 0, t_fixed = 1;

  // algorithms
  bool run_ff = true, run_ek = true, run_dn = true, run_pr = true;

  // generator selection
  enum class Gen { ER, PLOD };
  Gen gen = Gen::ER; // default: ER-like
  double er_edges_per_v = 5.0;

  // PLOD params
  double plod_alpha = 2.5;
  double plod_beta = 1000.0;
  bool plod_self_loops = false; // typically false for flow networks
};

// ------------------------- CLI parsing -------------------------
static bool starts_with(const std::string &s, const std::string &pref) {
  return s.size() >= pref.size() &&
         std::equal(pref.begin(), pref.end(), s.begin());
}

static std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, sep)) {
    size_t b = tok.find_first_not_of(" \t");
    size_t e = tok.find_last_not_of(" \t");
    if (b != std::string::npos)
      out.push_back(tok.substr(b, e - b + 1));
  }
  return out;
}

static void enable_algos_from_csv(Config &c, const std::string &csv) {
  c.run_ff = c.run_ek = c.run_dn = c.run_pr = false;
  for (auto &a : split(csv, ',')) {
    if (a == "all") {
      c.run_ff = c.run_ek = c.run_dn = c.run_pr = true;
    } else if (a == "ff")
      c.run_ff = true;
    else if (a == "ek")
      c.run_ek = true;
    else if (a == "dn" || a == "dinic")
      c.run_dn = true;
    else if (a == "pr" || a == "push-relabel")
      c.run_pr = true;
    else
      std::cerr << "[WARN] Unknown algo token: " << a << "\n";
  }
}

static void parse_graph_gen(Config &c, const std::string &spec) {
  // format:
  //   er[:edges_per_v=5.0]
  //   plod[:alpha=2.5,beta=1000,self_loops=0]
  auto parts = split(spec, ':');
  if (parts.empty())
    return;

  auto kind = parts[0];
  if (kind == "er")
    c.gen = Config::Gen::ER;
  else if (kind == "plod")
    c.gen = Config::Gen::PLOD;
  else {
    std::cerr << "[WARN] Unknown --graph-gen kind: " << kind << "\n";
    return;
  }

  if (parts.size() == 1)
    return;
  for (auto &kv : split(parts[1], ',')) {
    auto eq = kv.find('=');
    if (eq == std::string::npos)
      continue;
    auto k = kv.substr(0, eq);
    auto v = kv.substr(eq + 1);
    if (c.gen == Config::Gen::ER) {
      if (k == "edges_per_v")
        c.er_edges_per_v = std::stod(v);
    } else { // PLOD
      if (k == "alpha")
        c.plod_alpha = std::stod(v);
      else if (k == "beta")
        c.plod_beta = std::stod(v);
      else if (k == "self_loops")
        c.plod_self_loops = (v != "0");
    }
  }
}

static void print_usage(const char *prog) {
  std::cout
      <<
      R"(Usage:
  )" << prog
      << R"( [flags]

Generators:
  --graph-gen=er[:edges_per_v=5.0]
  --graph-gen=plod[:alpha=2.5,beta=1000,self_loops=0]

Common flags:
  --algos=ek,pr,dn,ff     Which algorithms to run (csv). 'all' for everything.
  --vertices=10000        Number of vertices (used by all generators)
  --cap-min=1 --cap-max=10000
  --runs=10
  --seed=42               0 => random_device
  --fixed-graph           Reuse exact same edge list for all runs
  --s=ID --t=ID           Fix source/sink (otherwise deterministic per-run)
  --help
Examples:
  )" << prog
      << R"( --graph-gen=plod:alpha=2.7,beta=1200 --algos=ek,pr,dn --vertices=20000 --runs=5 --seed=7
  )" << prog
      << R"( --graph-gen=er:edges_per_v=6 --algos=dn,pr --vertices=50000 --runs=3 --seed=1 --fixed-graph
)";
}

static Config parse_args(int argc, char **argv) {
  Config c;
  for (int i = 1; i < argc; ++i) {
    std::string a(argv[i]);
    auto val = [&](const char *key) -> std::string {
      std::string k(key);
      if (starts_with(a, k))
        return a.substr(k.size());
      return "";
    };
    if (a == "--help" || a == "-h") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (auto v = val("--algos="); !v.empty())
      enable_algos_from_csv(c, v);
    else if (auto v = val("--graph-gen="); !v.empty())
      parse_graph_gen(c, v);
    else if (auto v = val("--vertices="); !v.empty())
      c.V = std::stoi(v);
    else if (auto v = val("--cap-min="); !v.empty())
      c.cap_min = std::stoi(v);
    else if (auto v = val("--cap-max="); !v.empty())
      c.cap_max = std::stoi(v);
    else if (auto v = val("--runs="); !v.empty())
      c.runs = std::stoi(v);
    else if (auto v = val("--seed="); !v.empty())
      c.seed = static_cast<uint64_t>(std::stoull(v));
    else if (a == "--fixed-graph")
      c.fixed_graph = true;
    else if (auto v = val("--s="); !v.empty()) {
      c.have_fixed_st = true;
      c.s_fixed = std::stoi(v);
    } else if (auto v = val("--t="); !v.empty()) {
      c.have_fixed_st = true;
      c.t_fixed = std::stoi(v);
    } else
      std::cerr << "[WARN] Unrecognized flag: " << a << "\n";
  }
  if (c.V < 2) {
    std::cerr << "[ERR] Need at least 2 vertices.\n";
    std::exit(2);
  }
  if (c.cap_min < 0 || c.cap_max < c.cap_min) {
    std::cerr << "[ERR] Bad capacity range.\n";
    std::exit(2);
  }
  if (c.have_fixed_st) {
    if (c.s_fixed < 0 || c.s_fixed >= c.V || c.t_fixed < 0 ||
        c.t_fixed >= c.V || c.s_fixed == c.t_fixed) {
      std::cerr << "[ERR] Invalid fixed s,t.\n";
      std::exit(2);
    }
  }
  return c;
}

// ------------------------- Edge generation -------------------------
using Edge = std::tuple<int, int, long>;

static std::vector<Edge> gen_edges_er(int V, int E, int cap_min, int cap_max,
                                      uint64_t seed) {
  // vertices + capacities both from Boost.Random
  boost::random::mt19937_64 rng_v(static_cast<std::uint64_t>(seed));
  boost::random::uniform_int_distribution<int> vdist(0, V - 1);

  boost::random::mt19937_64 rng_c(
      static_cast<std::uint64_t>(seed ^ 0x9E3779B97F4A7C15ULL));
  boost::random::uniform_int_distribution<long> cdist(cap_min, cap_max);

  std::vector<Edge> edges;
  edges.reserve(E);
  for (int i = 0; i < E; ++i) {
    int u = vdist(rng_v), v = vdist(rng_v);
    if (u == v) {
      --i;
      continue;
    } // avoid self-loops for flow networks
    edges.emplace_back(u, v, cdist(rng_c));
  }
  return edges;
}

static std::vector<Edge> gen_edges_plod(int V, double alpha, double beta,
                                        bool self_loops, int cap_min,
                                        int cap_max, uint64_t seed) {
  using SFGen =
      plod_iterator<minstd_rand, adjacency_list<vecS, vecS, directedS>>;

  // Boost RNG for PLOD
  minstd_rand edge_rng(static_cast<std::uint64_t>(seed));

  // Boost RNG for capacities
  boost::random::mt19937_64 cap_rng(
      static_cast<std::uint64_t>(seed ^ 0x9E3779B97F4A7C15ULL));
  boost::random::uniform_int_distribution<long> cap(cap_min, cap_max);

  std::vector<Edge> edges;
  for (SFGen it(edge_rng, V, alpha, beta, self_loops), end; it != end; ++it) {
    int u = static_cast<int>(it->first);
    int v = static_cast<int>(it->second);
    if (!self_loops && u == v)
      continue; // just in case
    edges.emplace_back(u, v, cap(cap_rng));
  }
  return edges;
}

static void build_graph_from_edges(Graph &g, const std::vector<Edge> &edges) {
  for (const auto &e : edges) {
    int u, v;
    long c;
    std::tie(u, v, c) = e;
    add_edge_with_capacity(u, v, c, g);
  }
}

// ------------------------- Main -------------------------
int main(int argc, char **argv) {
  using Clock = std::chrono::steady_clock;
  using ns = std::chrono::nanoseconds;

  Config cfg = parse_args(argc, argv);
  uint64_t base_seed = cfg.seed ? cfg.seed : std::random_device{}();

  long long total_ff_ns = 0, total_ek_ns = 0, total_dn_ns = 0, total_pr_ns = 0;

  const int approx_E = (cfg.gen == Config::Gen::ER)
                           ? static_cast<int>(cfg.V * cfg.er_edges_per_v)
                           : 0; // PLOD decides its own E

  std::cout << "[INFO] gen=" << (cfg.gen == Config::Gen::ER ? "ER" : "PLOD")
            << "  V=" << cfg.V
            << (cfg.gen == Config::Gen::ER
                    ? (std::string("  E≈") + std::to_string(approx_E))
                    : std::string())
            << "  caps=[" << cfg.cap_min << "," << cfg.cap_max << "]"
            << "  runs=" << cfg.runs << "  seed=" << base_seed
            << (cfg.fixed_graph ? "  (fixed-graph)" : "") << "\n";

  if (cfg.gen == Config::Gen::PLOD)
    std::cout << "       PLOD: alpha=" << cfg.plod_alpha
              << " beta=" << cfg.plod_beta
              << " self_loops=" << (cfg.plod_self_loops ? "1" : "0") << "\n";

  // optionally pre-generate fixed edge list
  std::vector<Edge> fixed_edges;
  if (cfg.fixed_graph) {
    if (cfg.gen == Config::Gen::ER)
      fixed_edges =
          gen_edges_er(cfg.V, approx_E, cfg.cap_min, cfg.cap_max, base_seed);
    else
      fixed_edges = gen_edges_plod(cfg.V, cfg.plod_alpha, cfg.plod_beta,
                                   cfg.plod_self_loops, cfg.cap_min,
                                   cfg.cap_max, base_seed);
    std::cout << "[INFO] fixed-graph | edges=" << fixed_edges.size() << "\n";
  }

  for (int run_id = 0; run_id < cfg.runs; ++run_id) {
    uint64_t run_seed = base_seed + static_cast<std::uint64_t>(run_id);

    // s,t selection
    int s = cfg.have_fixed_st ? cfg.s_fixed : 0;
    int t = cfg.have_fixed_st ? cfg.t_fixed : 1;
    if (!cfg.have_fixed_st) {
      // choose deterministically per run
      boost::random::mt19937_64 rng_st(
          static_cast<std::uint64_t>(run_seed ^ 0x9E3779B97F4A7C15ULL));
      boost::random::uniform_int_distribution<int> vdist(0, cfg.V - 1);
      do {
        s = vdist(rng_st);
        t = vdist(rng_st);
      } while (s == t);
    }
    std::cout << "[RUN " << run_id << "] s=" << s << " t=" << t << "\n";

    // edge list for this run
    const std::vector<Edge> &edges =
        cfg.fixed_graph
            ? fixed_edges
            : (cfg.gen == Config::Gen::ER
                   ? gen_edges_er(cfg.V, approx_E, cfg.cap_min, cfg.cap_max,
                                  run_seed)
                   : gen_edges_plod(cfg.V, cfg.plod_alpha, cfg.plod_beta,
                                    cfg.plod_self_loops, cfg.cap_min,
                                    cfg.cap_max, run_seed));

    std::cout << "       edges=" << edges.size() << "\n";

    // per-algo graphs
    Graph g_ff(cfg.V), g_ek(cfg.V), g_dn(cfg.V), g_pr(cfg.V);
    if (cfg.run_ff)
      build_graph_from_edges(g_ff, edges);
    if (cfg.run_ek)
      build_graph_from_edges(g_ek, edges);
    if (cfg.run_dn)
      build_graph_from_edges(g_dn, edges);
    if (cfg.run_pr)
      build_graph_from_edges(g_pr, edges);

    std::vector<std::pair<std::string, long>> flows;

    if (cfg.run_ek) {
      auto st = Clock::now();
      long f = edmonds_karp_max_flow(g_ek, s, t);
      auto en = Clock::now();
      auto dt = std::chrono::duration_cast<ns>(en - st).count();
      total_ek_ns += dt;
      flows.push_back({"EK", f});
      std::cout << "> EK  flow=" << f << "  (" << dt << " ns, " << (dt / 1000)
                << " us)\n";
    }
    if (cfg.run_pr) {
      auto st = Clock::now();
      long f = push_relabel_max_flow(g_pr, s, t);
      auto en = Clock::now();
      auto dt = std::chrono::duration_cast<ns>(en - st).count();
      total_pr_ns += dt;
      flows.push_back({"PR", f});
      std::cout << "> PR  flow=" << f << "  (" << dt << " ns, " << (dt / 1000)
                << " us)\n";
    }
    if (cfg.run_ff) {
      auto st = Clock::now();
      long f = my_ff::ford_fulkerson_max_flow(g_ff, s, t);
      auto en = Clock::now();
      auto dt = std::chrono::duration_cast<ns>(en - st).count();
      total_ff_ns += dt;
      flows.push_back({"FF", f});
      std::cout << "> FF  flow=" << f << "  (" << dt << " ns, " << (dt / 1000)
                << " us)\n";
    }
    if (cfg.run_dn) {
      auto st = Clock::now();
      long f = my_dinic::Dinic(g_dn).max_flow(s, t);
      auto en = Clock::now();
      auto dt = std::chrono::duration_cast<ns>(en - st).count();
      total_dn_ns += dt;
      flows.push_back({"DN", f});
      std::cout << "> DN  flow=" << f << "  (" << dt << " ns, " << (dt / 1000)
                << " us)\n";
    }

    if (flows.size() >= 2) {
      long ref = flows.front().second;
      bool ok = true;
      for (auto &p : flows)
        if (p.second != ref)
          ok = false;
      if (!ok) {
        std::cout << "[" << run_id << "][WRONG] Results differ:\n";
        for (auto &p : flows)
          std::cout << "  " << p.first << ": " << p.second << "\n";
        return -1;
      }
    }
    std::cout << "\n";
  }

  auto avg = [&](long long total_ns) {
    return total_ns / static_cast<double>(cfg.runs);
  };

  std::cout << "[OK] All " << cfg.runs << " runs passed.\n";
  std::cout << "Totals over " << cfg.runs << " runs:\n\n";
  if (cfg.run_ek)
    std::cout << "> EK total: " << total_ek_ns
              << " ns | avg: " << avg(total_ek_ns) << " ns  (~"
              << (avg(total_ek_ns) / 1e6) << " ms)\n";
  if (cfg.run_pr)
    std::cout << "> PR total: " << total_pr_ns
              << " ns | avg: " << avg(total_pr_ns) << " ns  (~"
              << (avg(total_pr_ns) / 1e6) << " ms)\n";
  if (cfg.run_ff)
    std::cout << "> FF total: " << total_ff_ns
              << " ns | avg: " << avg(total_ff_ns) << " ns  (~"
              << (avg(total_ff_ns) / 1e6) << " ms)\n";
  if (cfg.run_dn)
    std::cout << "> DN total: " << total_dn_ns
              << " ns | avg: " << avg(total_dn_ns) << " ns  (~"
              << (avg(total_dn_ns) / 1e6) << " ms)\n";

  return 0;
}
