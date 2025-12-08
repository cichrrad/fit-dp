# generator.rb
require 'csv'

def generate_graph(filename, num_nodes, density, max_cap, bidirectional)
  edges = []

  # We force a path from 0 to N-1 to guarantee the graph isn't broken.
  nodes = (0...num_nodes).to_a.shuffle

  (0...num_nodes - 1).each do |i|
    u = nodes[i]
    v = nodes[i + 1]
    cap = rand(1..max_cap)
    edges << [u, v, cap]
    edges << [v, u, cap] if bidirectional
  end

  target_edges = (num_nodes * (num_nodes - 1) * density).to_i

  current_count = edges.length

  while current_count < target_edges
    u = rand(0...num_nodes)
    v = rand(0...num_nodes)

    # Avoid self-loops
    next if u == v

    next if edges.any? { |e| e[0] == u && e[1] == v }

    cap = rand(1..max_cap)
    edges << [u, v, cap]

    if bidirectional
      cap_reverse = rand(1..max_cap)
      edges << [v, u, cap_reverse]
    end

    current_count = edges.length
  end

  File.open(filename, 'w') do |f|
    edges.each do |u, v, w|
      f.puts "#{u} #{v} #{w}"
    end
  end

  puts "Generated #{filename}: #{num_nodes} nodes, #{edges.length} edges."
end

# --- CONFIGURATION ---
NUM_NODES = rand(2..51)
DENSITY = rand(1..10).to_f * 0.1
MAX_CAP = rand(1..2048)
BIDIRECTIONAL = rand(1..3) == 2

puts "NODES -- #{NUM_NODES}"
puts "DENSITY -- #{DENSITY}"
puts "MAX_CAP -- #{MAX_CAP}"
puts "BIDIRECTIONAL -- #{BIDIRECTIONAL}"

generate_graph('../../input/mock/generated_graph.csv', NUM_NODES, DENSITY, MAX_CAP, BIDIRECTIONAL)
