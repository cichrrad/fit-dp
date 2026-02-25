require 'erb'

# Configuration
EXPECTED_RUNS = 10
SOURCE_OF_TRUTH_SOLVER = 'hpf_pseudo_fifo'

GRAPH_REGEX = /^GRAPH_INFO \| NAME: ([^ ]+)/
BINARY_REGEX = /^--- BINARY: ([^ ]+) \|/

# Flow Formats
FLOW_REGEXES = {
  'ECL_MaxFlow'     => /Maximum flow from nodes \d+ to \d+: (\d+)/,
  'hpf_pseudo_fifo' => /^s Max Flow\s+:\s+(\d+)/,
  'hipr4'           => /^c flow:\s+([\d\.]+)/,
  'pbbs_syncpar'    => /flow=(\d+)/,
  'knfs_gpu'        => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu'        => /MAX FLOW IS\s+(\d+)/
}

# Parse the Log File
results = Hash.new { |h, k| h[k] = Hash.new { |h2, k2| h2[k2] = [] } }
solvers_seen = []

current_graph = nil
current_solver = nil

log_file = ARGV[0] || 'benchmark.log'
unless File.exist?(log_file)
  puts "Error: Could not find log file '#{log_file}'."
  puts "Usage: ruby parse_benchmark.rb <path_to_log_file>"
  exit 1
end

File.foreach(log_file) do |line|
  if line =~ GRAPH_REGEX
    current_graph = $1
  elsif line =~ BINARY_REGEX
    current_solver = $1
    solvers_seen << current_solver unless solvers_seen.include?(current_solver)
  elsif current_graph && current_solver && FLOW_REGEXES[current_solver]
    if line =~ FLOW_REGEXES[current_solver]
      raw_flow = $1
      normalized_flow = raw_flow.to_f.to_i.to_s 
      results[current_graph][current_solver] << normalized_flow
    end
  end
end

# Ensure the source of truth is always the first column
if solvers_seen.include?(SOURCE_OF_TRUTH_SOLVER)
  solvers_seen.delete(SOURCE_OF_TRUTH_SOLVER)
  solvers_seen.unshift(SOURCE_OF_TRUTH_SOLVER)
end

# Process Data for the Template
# We pre-calculate all the cell data here to avoid ERB scope errors
table_rows = []

results.each do |graph_name, solvers_data|
  row = { graph_name: graph_name, cells: [] }
  
  # Get the truth flow for comparison
  truth_flows = solvers_data[SOURCE_OF_TRUTH_SOLVER]
  truth_flow = truth_flows&.first 

  solvers_seen.each do |solver|
    flows = solvers_data[solver] || []
    unique_flows = flows.uniq
    timeout_count = EXPECTED_RUNS - flows.size
    
    # Format Timeout String
    timeout_str = timeout_count > 0 ? "<span class='timeout-text'>(#{timeout_count} timeouts)</span>" : ""

    cell_class = ""
    cell_text = ""

    if flows.empty?
      cell_class = "timeout-cell"
      cell_text = "Timed out (10/10)"
    elsif solver == SOURCE_OF_TRUTH_SOLVER
      cell_class = "match"
      cell_text = unique_flows.join(' vs ') + timeout_str
    elsif unique_flows.size > 1
      cell_class = "warning"
      formatted_flows = unique_flows.map do |f|
        f == truth_flow ? "<span class='truth-match'>#{f}</span>" : f
      end
      cell_text = formatted_flows.join(' vs ') + timeout_str
    elsif unique_flows.first == truth_flow
      cell_class = "match"
      cell_text = unique_flows.first + timeout_str
    else
      # Single result, but it doesn't match the source of truth
      cell_class = "error"
      cell_text = unique_flows.first + timeout_str
    end

    row[:cells] << { class: cell_class, text: cell_text }
  end
  
  table_rows << row
end

# Render HTML
html_template = <<-ERB
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Max Flow Solvers Table</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 20px; color: #333; }
    h1 { color: #2c3e50; }
    table { border-collapse: collapse; width: 100%; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    th, td { border: 1px solid #cbd5e1; padding: 12px 15px; text-align: left; vertical-align: top; }
    th { background-color: #f8fafc; font-weight: bold; color: #475569; }
    
    .match { background-color: #dcfce7; color: #166534; } 
    .warning { background-color: #fef08a; color: #854d0e; } 
    .error { background-color: #fee2e2; color: #991b1b; } 
    .timeout-cell { background-color: #f1f5f9; color: #64748b; font-style: italic; } 
    
    .timeout-text { color: #ef4444; font-size: 0.85em; font-weight: bold; margin-left: 5px; }
    .truth-match { font-weight: bold; text-decoration: underline; }
  </style>
</head>
<body>
  <h1>Max Flow Solvers table</h1>
  <table>
    <thead>
      <tr>
        <th>Graph Name</th>
        <% solvers_seen.each do |solver| %>
          <th><%= solver %> <%= "(Truth)" if solver == SOURCE_OF_TRUTH_SOLVER %></th>
        <% end %>
      </tr>
    </thead>
    <tbody>
      <% table_rows.each do |row| %>
        <tr>
          <td><strong><%= row[:graph_name] %></strong></td>
          <% row[:cells].each do |cell| %>
            <td class="<%= cell[:class] %>"><%= cell[:text] %></td>
          <% end %>
        </tr>
      <% end %>
    </tbody>
  </table>
</body>
</html>
ERB

renderer = ERB.new(html_template)
html_output = renderer.result(binding)
# YUCKY AF
output_filename = "benchmark_report_#{(ARGV[0].split('/')[1])[0..-5]}.html"
File.write(output_filename, html_output)

puts "Successfully parsed log and generated HTML report: #{output_filename}"