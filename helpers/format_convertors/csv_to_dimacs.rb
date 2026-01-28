#!/usr/bin/env ruby

def convert(filename)
  edges = []
  max_id = -1
  source = nil
  sink = nil
  explicit_source_sink = false

  first_line_read = false

  File.foreach(filename).with_index do |line, line_num|
    line = line.strip
    next if line.empty?
    next if line.start_with?('#')

    parts = line.gsub(',', ' ').split.map(&:to_i)
    next if parts.empty?

    unless first_line_read
      # Format: source sink -1
      if parts.length >= 2
        raw_s = parts[0]
        raw_t = parts[1]

        # Check logic: if header_s == header_t, use defaults later
        if raw_s != raw_t
          source = raw_s
          sink = raw_t
          explicit_source_sink = true
        end
      end
      first_line_read = true
      next
    end

    # Format: u v cap
    u = parts[0]
    v = parts[1]
    cap = parts[2] || 1 # Default capacity to 1 if missing, same as your C++

    # Track max ID for node count
    max_id = [max_id, u, v].max

    edges << { u: u, v: v, cap: cap }
  end

  num_nodes = max_id + 1
  num_arcs = edges.size

  # Default Source/Sink logic if not provided or s==t in header
  unless explicit_source_sink
    source = 0
    sink = max_id # Equivalent to num_nodes - 1
  end

  # Problem Line: p max NODES ARCS
  puts "p max #{num_nodes} #{num_arcs}"

  # Node Descriptors: n ID TYPE
  # +1 to IDs because 1-indexed
  puts "n #{source + 1} s"
  puts "n #{sink + 1} t"

  # Arc Descriptors: a U V CAP
  edges.each do |e|
    # Add +1 to u and v
    puts "a #{e[:u] + 1} #{e[:v] + 1} #{e[:cap]}"
  end
end

if ARGV.empty?
  puts 'Usage: ruby csv2dimacs.rb <input_file.csv>'
else
  convert(ARGV[0])
end
