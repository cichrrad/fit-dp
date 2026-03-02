def convert_to_dimacs_maxflow(input_file, output_file, capacity = 1)
  header_parsed = false
  current_node = 1
  num_nodes = 0
  num_edges = 0

  puts "Starting conversion of '#{input_file}'..."

  begin
    File.open(output_file, 'w') do |out|
      File.foreach(input_file) do |line|
        line = line.strip
        
        # Skip comments and empty lines
        next if line.start_with?('%') || line.empty?
        
        if !header_parsed
          num_nodes, num_edges = line.split.map(&:to_i)
          
          # Write the DIMACS Max Flow header
          out.puts "c Converted from DIMACS 10 / METIS to DIMACS Max Flow"
          out.puts "c Source: Node 1 | Sink: Node #{num_nodes} | Default Capacity: #{capacity}"
          out.puts "p max #{num_nodes} #{num_edges * 2}"
          
          # Declare Source (s) and Sink (t)
          out.puts "n 1 s"
          out.puts "n #{num_nodes} t"
          
          header_parsed = true
        else
          # Process the adjacency list for the current node
          neighbors = line.split.map(&:to_i)
          neighbors.each do |neighbor|
            out.puts "a #{current_node} #{neighbor} #{capacity}"
          end
          
          current_node += 1
        end
      end
    end

    puts "Conversion complete!"
    puts "Generated #{num_edges * 2} directed arcs for #{num_nodes} nodes."
    puts "Output saved to: '#{output_file}'"

  rescue Errno::ENOENT
    puts "Error: Could not find the input file '#{input_file}'."
    exit(1)
  end
end

if ARGV.length != 2
  puts "Usage: ruby convert.rb <input_file_path> <output_file_path>"
  puts "Example: ruby convert.rb luxembourg.osm.graph my_output.dat"
  exit(1)
end

input_filename = ARGV[0]
output_filename = ARGV[1]

convert_to_dimacs_maxflow(input_filename, output_filename)