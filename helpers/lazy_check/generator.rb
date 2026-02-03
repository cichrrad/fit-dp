# generator.rb
require 'set'

def generate_graph(filename, num_nodes, density, max_cap, source, sink, bidirectional)
  abort 'Error: Invalid Source/Sink configuration.' if source == sink || source >= num_nodes || sink >= num_nodes

  # O(1)
  existing_edges = Set.new
  edges = []

  # force s - t connection
  intermediates = (0...num_nodes).to_a - [source, sink]
  path_nodes = [source] + intermediates.shuffle + [sink]

  (0...path_nodes.length - 1).each do |i|
    u = path_nodes[i]
    v = path_nodes[i + 1]
    cap = rand(1..max_cap)

    edges << [u, v, cap]
    existing_edges.add([u, v])

    if bidirectional
      edges << [v, u, cap]
      existing_edges.add([v, u])
    end
  end

  # Fill the rest of the graph to meet density
  target_edges = (num_nodes * (num_nodes - 1) * density).to_i

  # Safety break
  attempts = 0
  max_attempts = target_edges * 10

  while edges.length < target_edges && attempts < max_attempts
    attempts += 1
    u = rand(0...num_nodes)
    v = rand(0...num_nodes)

    next if u == v
    next if existing_edges.include?([u, v])

    cap = rand(1..max_cap)

    edges << [u, v, cap]
    existing_edges.add([u, v])

    next unless bidirectional && edges.length < target_edges

    rev_cap = rand(1..max_cap)
    edges << [v, u, rev_cap]
    existing_edges.add([v, u])
  end

  # Write to CSV
  File.open(filename, 'w') do |f|
    f.puts "#{source} #{sink} -1 #SOURCE SINK HEADER"

    edges.each do |u, v, w|
      f.puts "#{u} #{v} #{w}"
    end
  end

  puts "Generated #{filename}:"
  puts "Source:   #{source}"
  puts "Sink:     #{sink}"
  puts "Size:     #{num_nodes}"
  puts "Edges:    #{edges.size}"
  puts "Dens:     #{density}"
  puts "Bidir:    #{bidirectional}"
end

# --- CONFIGURATION ---
NUM_NODES = rand(5..2000)
# Ensure Source and Sink are distinct and within bounds
SOURCE_ID = ENV['SOURCE'].to_i if ENV['SOURCE'].to_i < NUM_NODES - 1
SOURCE_ID ||= 0

SINK_ID = ENV['SINK'].to_i if ENV['SINK'].to_i < NUM_NODES && ENV['SINK'].to_i != SOURCE_ID
SINK_ID ||= NUM_NODES - 1

DENSITY = rand(1..9) * 0.1
MAX_CAP = rand(1..40_000)
BIDIRECTIONAL = rand(0..1).positive?

output_file = '../../input/mock/generated_graph.csv'

generate_graph(output_file, NUM_NODES, DENSITY, MAX_CAP, SOURCE_ID, SINK_ID, BIDIRECTIONAL)
