#!/usr/bin/env ruby
require 'erb'
require 'cgi'

EXPECTED_RUNS = 10 #

log_file = ARGV[0]
unless log_file && File.exist?(log_file)
  puts "Usage: ruby parse_times.rb <path_to_log_file>"
  exit 1
end

results = {}
solvers_seen = []
current_graph = nil
current_binary = nil
current_time = 0.0
valid_run = false

# PARSE LOG DATA
File.foreach(log_file) do |line|
  if line =~ /^GRAPH_INFO \| NAME: ([^ ]+)/
    if valid_run && current_graph && current_binary && current_time > 0
      results[current_graph][current_binary] << current_time
    end
    current_graph = $1
    results[current_graph] ||= {}
    current_binary = nil
    valid_run = false
    next
  end

  if line =~ /--- BINARY: ([^ ]+) \| ITERATION: \d+ \|/
    if valid_run && current_graph && current_binary && current_time > 0
      results[current_graph][current_binary] << current_time
    end
    current_binary = $1
    solvers_seen << current_binary unless solvers_seen.include?(current_binary)
    results[current_graph][current_binary] ||= [] if current_graph
    current_time = 0.0
    valid_run = true
    next
  end

  if line =~ /!!! RESULT: (TIMEOUT|FAILED)/
    valid_run = false
    next
  end

  if current_binary && valid_run
    case current_binary
    when 'ECL_MaxFlow'
      current_time += $1.to_f if line =~ /header total init time:\s+([\d\.]+)s/ || line =~ /^runtime:\s+([\d\.]+)s/
    when 'hpf_pseudo_fifo'
      current_time += $2.to_f if line =~ /c Time to (read|initialize|min cut|max flow)\s*:\s+([\d\.]+)/
    when 'hipr4'
      current_time += $2.to_f if line =~ /c (time|init tm):\s+([\d\.]+)/
    when 'pbbs_syncpar'
      current_time += $1.to_f if line =~ /PBBS-time:\s+([\d\.]+)/ || line =~ /deinit time:\s+([\d\.]+)/
    when /^knfs_/ 
      current_time += $1.to_f if line =~ /TOTAL Runtime .*:\s+([\d\.]+)/
    end
  end
end

if valid_run && current_graph && current_binary && current_time > 0
  results[current_graph][current_binary] << current_time
end

# PREPARE DATA FOR TEMPLATE
table_rows = [] #

results.each do |graph_name, solvers_data|
  row = { graph_name: graph_name, cells: [] } #
  
  # Find the fastest average time to highlight it
  averages = {}
  solvers_seen.each do |solver|
    times = solvers_data[solver] || []
    averages[solver] = times.sum / times.size unless times.empty?
  end
  fastest_time = averages.values.min

  solvers_seen.each do |solver|
    times = solvers_data[solver] || []
    timeout_count = EXPECTED_RUNS - times.size #
    
    timeout_str = timeout_count > 0 ? "<br><span class='timeout-text'>(#{timeout_count} timeouts)</span>" : "" #

    cell_class = ""
    cell_text = ""

    if times.empty?
      cell_class = "timeout-cell" #
      cell_text = "Timed out (#{EXPECTED_RUNS}/#{EXPECTED_RUNS})"
    else
      avg = times.sum / times.size
      
      # Highlight the fastest solver in green
      cell_class = (avg == fastest_time) ? "match" : "" #
      cell_text = sprintf("%.4f s", avg) + timeout_str
    end

    row[:cells] << { class: cell_class, text: cell_text } #
  end
  
  table_rows << row
end

# RENDER HTML
html_template = <<-ERB
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Max Flow Solvers: Average Times</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 20px; color: #333; }
    h1 { color: #2c3e50; }
    table { border-collapse: collapse; width: 100%; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    th, td { border: 1px solid #cbd5e1; padding: 12px 15px; text-align: left; vertical-align: top; }
    th { background-color: #f8fafc; font-weight: bold; color: #475569; }
    
    .match { background-color: #dcfce7; color: #166534; font-weight: bold; } /* Used for the fastest time */
    .timeout-cell { background-color: #f1f5f9; color: #64748b; font-style: italic; } 
    .timeout-text { color: #ef4444; font-size: 0.85em; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Max Flow Solvers: Average Total Times</h1>
  <table>
    <thead>
      <tr>
        <th>Graph Name</th>
        <% solvers_seen.each do |solver| %>
          <th><%= solver %></th>
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

renderer = ERB.new(html_template) #
html_output = renderer.result(binding) #

# Clean filename parsing
base_name = File.basename(log_file, '.*')
output_filename = "benchmark_times_#{base_name}.html"

File.write(output_filename, html_output) #
puts "Successfully parsed times and generated HTML report: #{output_filename}"