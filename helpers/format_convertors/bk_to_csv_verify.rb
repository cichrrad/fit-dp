#!/usr/bin/env ruby

def verify_conversion(bk_path, csv_path)
  puts " Verifying: #{File.basename(bk_path)} vs #{File.basename(csv_path)}"

  puts '   Reading BK file...'

  bk_stats = {
    edge_count: 0,
    total_capacity: 0,
    max_node_id: 0
  }

  File.foreach(bk_path) do |line|
    parts = line.split
    next if parts.empty?

    tag = parts[0]

    if tag == 'n' && parts.length >= 2
      u = parts[1].to_i
      bk_stats[:max_node_id] = [bk_stats[:max_node_id], u].max
    elsif tag == 'a' && parts.length >= 3
      u = parts[1].to_i
      v = parts[2].to_i
      bk_stats[:max_node_id] = [bk_stats[:max_node_id], u, v].max
    end

    if tag == 'n' && parts.length >= 4
      # n u cap_s cap_t
      cap_s = parts[2].to_i
      cap_t = parts[3].to_i

      # normalize for negatives
      min_val = [cap_s, cap_t].min
      if min_val < 0
        shift = min_val.abs
        cap_s += shift
        cap_t += shift
      end

      # Tally expected Source->u edges
      if cap_s > 0
        bk_stats[:edge_count] += 1
        bk_stats[:total_capacity] += cap_s
      end

      # Tally expected u->Sink edges
      if cap_t > 0
        bk_stats[:edge_count] += 1
        bk_stats[:total_capacity] += cap_t
      end

    elsif tag == 'a' && parts.length >= 4
      # a u v cap_uv cap_vu
      cap_uv = parts[3].to_i
      cap_vu = parts.length > 4 ? parts[4].to_i : 0

      if cap_uv > 0
        bk_stats[:edge_count] += 1
        bk_stats[:total_capacity] += cap_uv
      end

      if cap_vu > 0
        bk_stats[:edge_count] += 1
        bk_stats[:total_capacity] += cap_vu
      end
    end
  end

  expected_source = bk_stats[:max_node_id] + 1
  expected_sink = bk_stats[:max_node_id] + 2

  puts '   Reading CSV file...'

  csv_stats = {
    edge_count: 0,
    total_capacity: 0,
    source: -1,
    sink: -1
  }

  first_line = true
  File.foreach(csv_path) do |line|
    next if line.strip.empty?

    parts = line.split.map(&:to_i)

    if first_line
      # Header: source sink -1
      csv_stats[:source] = parts[0]
      csv_stats[:sink] = parts[1]
      first_line = false
      next
    end

    # Edge: u v cap
    cap = parts[2]
    csv_stats[:edge_count] += 1
    csv_stats[:total_capacity] += cap
  end

  puts "\n--- Report ---"

  pass = true

  # Check 1: Edge Counts
  if bk_stats[:edge_count] == csv_stats[:edge_count]
    puts "✅ Edge Count matches: #{bk_stats[:edge_count]}"
  else
    puts "❌ Edge Count MISMATCH! BK: #{bk_stats[:edge_count]}, CSV: #{csv_stats[:edge_count]}"
    pass = false
  end

  # Check 2: Total Capacity Preservation
  if bk_stats[:total_capacity] == csv_stats[:total_capacity]
    puts "✅ Total Capacity matches: #{bk_stats[:total_capacity]}"
  else
    puts "❌ Total Capacity MISMATCH! BK: #{bk_stats[:total_capacity]}, CSV: #{csv_stats[:total_capacity]}"
    pass = false
  end

  # Check 3: Source/Sink IDs
  if csv_stats[:source] == expected_source && csv_stats[:sink] == expected_sink
    puts "✅ Source/Sink IDs correct: S=#{expected_source}, T=#{expected_sink}"
  else
    puts "❌ Source/Sink IDs invalid. Expected S=#{expected_source}/T=#{expected_sink}, Got S=#{csv_stats[:source]}/T=#{csv_stats[:sink]}"
    pass = false
  end

  if pass
    puts "\n🎉 SUCCESS: The graph was converted perfectly."
  else
    puts "\n⚠️ FAILURE: The graph is mathematically different."
  end
end

if ARGV.length == 2
  verify_conversion(ARGV[0], ARGV[1])
else
  puts 'Usage: ruby verify.rb <original.bk> <converted.csv>'
end
