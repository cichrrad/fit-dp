#!/usr/bin/env ruby

def convert_bk_to_csv(bk_file_path, csv_file_path)
  puts "Processing #{bk_file_path}..."

  max_node_id = 0

  # Find the maximum node ID
  # We need to scan first to safely assign Source and Sink IDs higher than any existing node.
  File.foreach(bk_file_path) do |line|
    parts = line.split
    next if parts.empty?

    tag = parts[0]

    if tag == 'n' && parts.length >= 2
      u = parts[1].to_i
      max_node_id = [max_node_id, u].max
    elsif tag == 'a' && parts.length >= 3
      u = parts[1].to_i
      v = parts[2].to_i
      max_node_id = [max_node_id, u, v].max
    end
  end

  source_id = max_node_id + 1
  sink_id = max_node_id + 2

  puts "  Max ID found: #{max_node_id}"
  puts "  Assigned Source: #{source_id}, Sink: #{sink_id}"

  # Read and Write to CSV
  edge_count = 0

  File.open(csv_file_path, 'w') do |f_out|
    # Write Header per your csv_loader.hpp format
    f_out.puts "#{source_id} #{sink_id} -1 #SOURCE SINK HEADER"

    File.foreach(bk_file_path) do |line|
      parts = line.split
      next if parts.empty?

      tag = parts[0]

      # Handle Node (Terminal) Links
      # Format: n <u_id> <cap_source> <cap_sink>
      if tag == 'n' && parts.length >= 4
        u = parts[1].to_i
        cap_s = parts[2].to_i
        cap_t = parts[3].to_i

        # Normalization logic for negative numbers (DIMACS note)
        # If values are negative, shift them up so the minimum becomes 0
        min_val = [cap_s, cap_t].min
        if min_val < 0
          shift = min_val.abs
          cap_s += shift
          cap_t += shift
        end

        # Write edge Source -> u
        if cap_s > 0
          f_out.puts "#{source_id} #{u} #{cap_s}"
          edge_count += 1
        end

        # Write edge u -> Sink
        if cap_t > 0
          f_out.puts "#{u} #{sink_id} #{cap_t}"
          edge_count += 1
        end

      # Handle Arc (Neighbor) Links
      # Format: a <u_id> <v_id> <cap_uv> <cap_vu>
      elsif tag == 'a' && parts.length >= 4
        u = parts[1].to_i
        v = parts[2].to_i
        cap_uv = parts[3].to_i

        # Some variants might omit cap_vu, assume 0 if missing
        cap_vu = parts.length > 4 ? parts[4].to_i : 0

        if cap_uv > 0
          f_out.puts "#{u} #{v} #{cap_uv}"
          edge_count += 1
        end

        if cap_vu > 0
          f_out.puts "#{v} #{u} #{cap_vu}"
          edge_count += 1
        end
      end
    end
  end

  puts "  Done. Wrote #{edge_count} edges to #{csv_file_path}"
end

if ARGV.length != 2
  puts 'Usage: ruby bk2csv.rb <input_directory> <output_directory>'
  exit 1
end

input_dir = ARGV[0]
output_dir = ARGV[1]

# Validate Input Directory
unless Dir.exist?(input_dir)
  puts "Error: Input directory '#{input_dir}' does not exist."
  exit 1
end

# Create Output Directory
unless Dir.exist?(output_dir)
  puts "Creating output directory: #{output_dir}"
  FileUtils.mkdir_p(output_dir)
end

# Process Files
files = Dir.glob(File.join(input_dir, '*')).select { |f| File.file?(f) }
puts "Found #{files.length} files in #{input_dir}"

files.each do |input_path|
  # Determine output path: /out_dir/filename.csv
  filename_no_ext = File.basename(input_path, '.*')
  output_path = File.join(output_dir, "#{filename_no_ext}.csv")

  convert_bk_to_csv(input_path, output_path)
end

puts 'All done!'
