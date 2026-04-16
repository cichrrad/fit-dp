require 'fileutils'

if ARGV.length != 2
  puts 'Usage: ruby extract_dumps.rb <path_to_dat_directory> <output_log_file>'
  exit 1
end

input_dir = ARGV[0]
output_file = ARGV[1]

# Capture Group 1: Solver (knfs_gpu or knfs_cpu)
# Capture Group 2: Iteration (e.g., 05)
# Capture Group 3: Graph Name (e.g., lazybrush-mangadinner.max.csv.dimacs)
FILE_REGEX = /^(knfs_(?:gpu|cpu))_(\d+)_(.+?)_[a-f0-9-]+\.dat$/

# Data structure to hold our files: { [solver, graph] => [ {iteration, path}, ... ] }
grouped_data = Hash.new { |hash, key| hash[key] = [] }

puts "Scanning directory: #{input_dir}..."

Dir.glob(File.join(input_dir, '*.dat')).each do |filepath|
  filename = File.basename(filepath)
  match = FILE_REGEX.match(filename)

  if match
    solver = match[1]
    iteration = match[2].to_i
    graph = match[3]

    grouped_data[[solver, graph]] << { iteration: iteration, path: filepath }
  else
    puts "Warning: Filename did not match expected pattern, skipping: #{filename}"
  end
end

puts "Found #{grouped_data.values.flatten.size} valid runs. Extracting to #{output_file}..."

File.open(output_file, 'w') do |out|
  # Sort alphabetically by solver, then graph
  grouped_data.keys.sort.each do |solver, graph|
    out.puts '=' * 80
    out.puts "GRAPH:  #{graph}"
    out.puts "SOLVER: #{solver}"
    out.puts '=' * 80
    out.puts

    # Sort the runs for this graph/solver by iteration number
    runs = grouped_data[[solver, graph]].sort_by { |run| run[:iteration] }

    runs.each do |run|
      out.puts '-' * 40
      out.puts "ITERATION: #{run[:iteration]}"
      out.puts '-' * 40

      # Execute kp_reader on the binary blob.
      # (If the .dat files are ALREADY text, replace the line below with: output = File.read(run[:path]))
      # hardcoded path *chefs kiss*
      output = `../profile_tools/kp_reader "#{run[:path]}" 2>&1`

      out.puts output
      out.puts "\n\n"
    end
  end
end

puts 'Done! Log file generated successfully.'
