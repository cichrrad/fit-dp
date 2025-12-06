class MaxFlowOracle
  attr_accessor :graph, :num_nodes

  def initialize(filename)
    @graph = Hash.new { |h, k| h[k] = Hash.new(0) }
    nodes = Set.new

    # Read CSV
    File.foreach(filename) do |line|
      parts = line.split(' ')
      next if parts.empty?

      u = parts[0].to_i
      v = parts[1].to_i
      cap = parts[2].to_i

      # Add to residual graph
      @graph[u][v] += cap
      # NOTE: We do NOT add v->u capacity here unless the CSV line exists.
      # However, Edmonds-Karp requires the EXISTENCE of a reverse edge
      # initialized to 0 for the residual calculation.
      @graph[v][u] += 0 unless @graph[v].key?(u)

      nodes.add(u)
      nodes.add(v)
    end
    @num_nodes = nodes.max + 1
  end

  # BFS to find path from s to t
  def bfs(s, t, parent)
    visited = Array.new(@num_nodes, false)
    queue = []

    queue.push(s)
    visited[s] = true
    parent[s] = -1

    until queue.empty?
      u = queue.shift

      @graph[u].each do |v, capacity|
        # If not visited and capacity available
        next unless !visited[v] && capacity > 0

        queue.push(v)
        parent[v] = u
        visited[v] = true
        return true if v == t
      end
    end
    false
  end

  # Edmonds-Karp Algorithm
  def edmonds_karp(s, t)
    max_flow = 0
    parent = Array.new(@num_nodes)

    # While there is a path from source to sink
    while bfs(s, t, parent)
      path_flow = Float::INFINITY

      # Find bottleneck capacity in the path found by BFS
      v = t
      while v != s
        u = parent[v]
        path_flow = [path_flow, @graph[u][v]].min
        v = u
      end

      # Update residual capacities
      v = t
      while v != s
        u = parent[v]
        @graph[u][v] -= path_flow
        @graph[v][u] += path_flow
        v = u
      end

      max_flow += path_flow
    end

    max_flow
  end
end

# --- RUN THE CHECKER ---
filename = '../../input/mock/generated_graph.csv'

if File.exist?(filename)
  oracle = MaxFlowOracle.new(filename)
  source = 0
  sink = oracle.num_nodes - 1

  # puts "Calculating Max Flow..."
  result = oracle.edmonds_karp(source, sink)

  # puts "---------------------------"
  # puts "Source Node: #{source}"
  # puts "Sink Node:   #{sink}"
  puts "MAX FLOW:   #{result}"
  # puts "---------------------------"
else
  puts "File #{filename} not found. Run generator.rb first."
end
