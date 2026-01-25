# checker.rb
require 'set'

class DinicSolver
  attr_reader :num_nodes, :source, :sink, :graph

  Edge = Struct.new(:to, :rev, :cap, :flow)

  def initialize(filename)
    @graph = []
    @source = 0
    @sink = 0
    load_graph(filename)
  end

  def add_edge(u, v, cap)
    # Forward edge: u -> v
    # rev is the index of the reverse edge in graph[v]
    a = Edge.new(v, @graph[v].size, cap, 0)
    # Backward edge: v -> u (capacity 0 for residual)
    b = Edge.new(u, @graph[u].size, 0, 0)

    @graph[u] << a
    @graph[v] << b
  end

  def load_graph(filename)
    max_id = 0
    lines = File.readlines(filename)

    return if lines.empty?

    # Parse Header
    header = lines[0].split(' ')
    s_in = header[0].to_i
    t_in = header[1].to_i

    # Track if we need to set defaults later
    use_defaults = (s_in == t_in)

    unless use_defaults
      @source = s_in
      @sink = t_in
    end

    # Parse Edges (Lines 1..End) to find Max ID
    lines[1..-1].each do |line|
      next if line.strip.empty? || line.start_with?('#')

      parts = line.split(' ')
      u = parts[0].to_i
      v = parts[1].to_i
      max_id = [max_id, u, v].max
    end

    @num_nodes = max_id + 1
    @graph = Array.new(@num_nodes) { [] }

    # Apply defaults if header was unspecified (s == t)
    if use_defaults
      @source = 0
      @sink = @num_nodes - 1
    end

    # 4. Build Graph
    lines[1..-1].each do |line|
      next if line.strip.empty? || line.start_with?('#')

      parts = line.split(' ')
      u = parts[0].to_i
      v = parts[1].to_i
      cap = parts[2].to_i
      add_edge(u, v, cap)
    end
  end

  # BFS to build the Level Graph (distances from S)
  def bfs(level)
    level.fill(-1)
    level[@source] = 0
    queue = [@source]

    until queue.empty?
      u = queue.shift
      @graph[u].each do |e|
        if e.cap - e.flow > 0 && level[e.to] < 0
          level[e.to] = level[u] + 1
          queue << e.to
        end
      end
    end
    level[@sink] >= 0
  end

  # DFS to find blocking flow in the Level Graph
  def dfs(u, pushed, level, ptr)
    return pushed if pushed == 0 || u == @sink

    # ptr[u] tracks next edge to explore (pruning)
    (ptr[u]...@graph[u].size).each do |i|
      ptr[u] = i # Update pointer to avoid re-scanning
      e = @graph[u][i]

      next if level[u] + 1 != level[e.to] || e.cap - e.flow == 0

      tr = dfs(e.to, [pushed, e.cap - e.flow].min, level, ptr)

      next if tr == 0

      e.flow += tr
      @graph[e.to][e.rev].flow -= tr
      return tr
    end
    0
  end

  def max_flow
    puts '[STARTING ALGORITHM]'
    flow = 0
    level = Array.new(@num_nodes)

    # While we can reach Sink in the residual graph
    while bfs(level)
      ptr = Array.new(@num_nodes, 0)
      while (pushed = dfs(@source, Float::INFINITY, level, ptr)) > 0
        flow += pushed
      end
    end
    flow
  end
end

file_path = '../../input/mock/generated_graph.csv'

if File.exist?(file_path)
  # [TIMER] Start Load
  t_start_load = Process.clock_gettime(Process::CLOCK_MONOTONIC)

  solver = DinicSolver.new(file_path)

  # [TIMER] End Load
  t_end_load = Process.clock_gettime(Process::CLOCK_MONOTONIC)

  puts "Validating: #{file_path}"
  puts "Source: #{solver.source}, Sink: #{solver.sink}, Nodes: #{solver.num_nodes}"
  puts ">> Graph Load & Build Time: #{(t_end_load - t_start_load).round(6)} seconds."

  # [TIMER] Start Algo
  t_start_algo = Process.clock_gettime(Process::CLOCK_MONOTONIC)

  result = solver.max_flow

  # [TIMER] End Algo
  t_end_algo = Process.clock_gettime(Process::CLOCK_MONOTONIC)

  puts ">> Algorithm Runtime: #{(t_end_algo - t_start_algo).round(6)} seconds."
  puts '------------------------------------------'
  puts "TOTAL Runtime: #{(t_end_algo - t_start_load).round(6)} seconds."
  puts "MAX FLOW: #{result}"
else
  puts 'File not found.'
end
