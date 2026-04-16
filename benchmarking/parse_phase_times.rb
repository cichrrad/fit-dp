#!/usr/bin/env ruby
require 'csv'

# Check if the user provided an argument
if ARGV.empty?
  puts "Usage: #{$0} <path_to_log_file>"
  exit 1
end

def parse_and_average_logs(log_file_path, output_csv_path)
  data = {}

  current_graph = nil
  current_solver = nil

  # Check if file exists before trying to read it
  unless File.exist?(log_file_path)
    puts "Error: File '#{log_file_path}' not found."
    exit 1
  end

  # Initialize the data structure and parse the file
  File.foreach(log_file_path) do |line|
    # Match Graph Name
    if line.match(/^GRAPH_INFO\s+\|\s+NAME:\s+([^\s|]+)/)
      current_graph = Regexp.last_match(1)
      data[current_graph] ||= {
        'knfs_cpu' => { io: [], init: [], algo: [], total: [] },
        'knfs_gpu' => { io: [], init: [], algo: [], total: [] }
      }
      next
    end

    # Match Solver (Binary)
    if line.match(/^---\s+BINARY:\s+(\w+)\s+\|/)
      current_solver = Regexp.last_match(1)
      next
    end

    # Only extract one of the target solvers
    next unless %w[knfs_cpu knfs_gpu].include?(current_solver) && current_graph

    # Extract Times
    if line.match(/>>\s+IO Time \(CSV Read\):\s+([\d.]+)\s+seconds\./)
      data[current_graph][current_solver][:io] << Regexp.last_match(1).to_f
    elsif line.match(/>>\s+Graph Build & Init Time:\s+([\d.]+)\s+seconds\./)
      data[current_graph][current_solver][:init] << Regexp.last_match(1).to_f
    elsif line.match(/>>\s+Algorithm Runtime:\s+([\d.]+)\s+seconds\./)
      data[current_graph][current_solver][:algo] << Regexp.last_match(1).to_f
    elsif line.match(/TOTAL Runtime \(IO \+ Init \+ Algo\):\s+([\d.]+)\s+seconds\./)
      data[current_graph][current_solver][:total] << Regexp.last_match(1).to_f
    end
  end

  # Write to CSV
  headers = %w[
    Graph
    knfs_cpu_io_avg knfs_cpu_init_avg knfs_cpu_algo_avg knfs_cpu_total_avg
    knfs_gpu_io_avg knfs_gpu_init_avg knfs_gpu_algo_avg knfs_gpu_total_avg
  ]

  CSV.open(output_csv_path, 'w', write_headers: true, headers: headers) do |csv|
    data.each do |graph, solvers|
      row = [graph]

      %w[knfs_cpu knfs_gpu].each do |solver|
        %i[io init algo total].each do |phase|
          times = solvers[solver][phase]
          if times.any?
            avg = times.sum / times.size.to_f
            row << avg.round(6)
          else
            row << 'N/A' # Handle cases where a run crashed and yielded no times
          end
        end
      end

      csv << row
    end
  end

  puts "Extraction complete! Data written to: #{output_csv_path}"
end

# Setup input and dynamic output paths
log_file = ARGV[0]

# Extract the base name without the extension
base_name = File.basename(log_file, '.*')

# Create the new filename in the same directory as the script runs
output_csv = "#{base_name}_phase_avgs.csv"

# Run the parser
parse_and_average_logs(log_file, output_csv)
