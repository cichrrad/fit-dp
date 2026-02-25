#!/usr/bin/env ruby
require 'erb'
require 'cgi'

# Configuration
EXPECTED_RUNS = 10
SOURCE_OF_TRUTH_SOLVER = 'hpf_pseudo_fifo'

FLOW_REGEXES = {
  'ECL_MaxFlow'     => /Maximum flow from nodes \d+ to \d+: (\d+)/,
  'hpf_pseudo_fifo' => /^s Max Flow\s+:\s+(\d+)/,
  'hipr4'           => /^c flow:\s+([\d\.]+)/,
  'pbbs_syncpar'    => /flow=(\d+)/,
  'knfs_gpu'        => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu'        => /MAX FLOW IS\s+(\d+)/
}

log_file = ARGV[0]
unless log_file && File.exist?(log_file)
  puts "Usage: ruby parse_master.rb <path_to_log_file>"
  exit 1
end

# Data Structure: results[graph][solver] = { flows: [], times: [] }
results = Hash.new { |h, k| h[k] = Hash.new { |h2, k2| h2[k2] = { flows: [], times: [] } } }
solvers_seen = []

current_graph = nil
current_solver = nil
current_time = 0.0
valid_run = false

# PARSE LOG DATA
File.foreach(log_file) do |line|
  # Graph Header
  if line =~ /^GRAPH_INFO \| NAME: ([^ ]+)/
    if valid_run && current_graph && current_solver && current_time > 0
      results[current_graph][current_solver][:times] << current_time
    end
    current_graph = $1
    current_solver = nil
    valid_run = false
    next
  end

  # Binary Header
  if line =~ /^--- BINARY: ([^ ]+) \|/
    if valid_run && current_graph && current_solver && current_time > 0
      results[current_graph][current_solver][:times] << current_time
    end
    current_solver = $1
    solvers_seen << current_solver unless solvers_seen.include?(current_solver)
    current_time = 0.0
    valid_run = true
    next
  end

  # Handle Failures
  if line =~ /!!! RESULT: (TIMEOUT|FAILED)/
    valid_run = false
    next
  end

  # Accumulate Data
  if current_graph && current_solver && valid_run
    # Extract Flow
    if FLOW_REGEXES[current_solver] && line =~ FLOW_REGEXES[current_solver]
      results[current_graph][current_solver][:flows] << $1.to_f.to_i.to_s
    end
    
    # Extract Time
    case current_solver
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

# Catch EOF
if valid_run && current_graph && current_solver && current_time > 0
  results[current_graph][current_solver][:times] << current_time
end

# Ensure the source of truth is always the first column
if solvers_seen.include?(SOURCE_OF_TRUTH_SOLVER)
  solvers_seen.delete(SOURCE_OF_TRUTH_SOLVER)
  solvers_seen.unshift(SOURCE_OF_TRUTH_SOLVER)
end

# PREPARE DATA FOR TEMPLATE
graphs_data = []

results.each do |graph_name, solvers_data|
  truth_flows = solvers_data[SOURCE_OF_TRUTH_SOLVER][:flows] || []
  truth_flow = truth_flows.first
  
  # Check if this graph is a "Perfect Graph" (All solvers completely correct)
  # Criteria: Every solver must have returned at least 1 flow, and ALL returned flows must match HPF.
  is_perfect = true
  if truth_flow.nil?
    is_perfect = false
  else
    solvers_seen.each do |solver|
      flows = solvers_data[solver][:flows] || []
      if flows.empty? || flows.any? { |f| f != truth_flow }
        is_perfect = false
        break
      end
    end
  end

  # Calculate fastest time for highlighting
  averages = {}
  solvers_seen.each do |solver|
    times = solvers_data[solver][:times] || []
    averages[solver] = times.sum / times.size unless times.empty?
  end
  fastest_time = averages.values.min

  # Build Row Data
  row_correctness = []
  row_times = []

  solvers_seen.each do |solver|
    flows = solvers_data[solver][:flows] || []
    times = solvers_data[solver][:times] || []
    unique_flows = flows.uniq
    timeout_count = EXPECTED_RUNS - flows.size
    timeout_str = timeout_count > 0 ? "<br><span class='timeout-text'>(#{timeout_count} timeouts)</span>" : ""
    
    # Formatting Correctness Cell
    c_class = ""
    c_text = ""
    if flows.empty?
      c_class = "timeout-cell"
      c_text = "Timed out"
    elsif solver == SOURCE_OF_TRUTH_SOLVER
      c_class = "match"
      c_text = unique_flows.join(' vs ') + timeout_str
    elsif unique_flows.size > 1
      c_class = "warning"
      formatted_flows = unique_flows.map { |f| f == truth_flow ? "<span class='truth-match'>#{f}</span>" : f }
      c_text = formatted_flows.join(' vs ') + timeout_str
    elsif unique_flows.first == truth_flow
      c_class = "match"
      c_text = unique_flows.first + timeout_str
    else
      c_class = "error"
      c_text = unique_flows.first + timeout_str
    end
    row_correctness << { class: c_class, text: c_text }

    # Formatting Time Cell
    t_class = ""
    t_text = ""
    if times.empty?
      t_class = "timeout-cell"
      t_text = "N/A"
    else
      avg = times.sum / times.size
      t_class = (avg == fastest_time) ? "match-time text-bold" : ""
      t_text = sprintf("%.4f s", avg) + timeout_str
    end
    row_times << { class: t_class, text: t_text }
  end
  
  graphs_data << {
    name: graph_name,
    is_perfect: is_perfect,
    correctness_cells: row_correctness,
    time_cells: row_times
  }
end

# RENDER HTML
html_template = <<-ERB
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Unified Max Flow Benchmark Dashboard</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 20px; color: #333; background-color: #f4f6f8; }
    .container { max-width: 1400px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
    h1, h2 { color: #2c3e50; }
    h2 { margin-top: 40px; border-bottom: 2px solid #e2e8f0; padding-bottom: 10px; }
    
    table { border-collapse: collapse; width: 100%; margin-top: 20px; font-size: 0.95em; }
    th, td { border: 1px solid #cbd5e1; padding: 12px 15px; text-align: left; vertical-align: top; }
    th { background-color: #f8fafc; font-weight: bold; color: #475569; position: sticky; top: 0; }
    
    /* Cell Styles */
    .match { background-color: #dcfce7; color: #166534; } 
    .match-time { background-color: #dcfce7; color: #166534; font-weight: bold; border: 2px solid #22c55e; } 
    .warning { background-color: #fef08a; color: #854d0e; } 
    .error { background-color: #fee2e2; color: #991b1b; } 
    .timeout-cell { background-color: #f1f5f9; color: #64748b; font-style: italic; } 
    
    .timeout-text { color: #ef4444; font-size: 0.85em; font-weight: bold; }
    .truth-match { font-weight: bold; text-decoration: underline; }
    
    /* Perfect Graph Highlighting */
    .perfect-graph td { border-top: 2px solid #3b82f6; border-bottom: 2px solid #3b82f6; }
    .perfect-graph .graph-name { background-color: #eff6ff; color: #1d4ed8; font-weight: bold; }
    .perfect-badge { display: inline-block; background: #3b82f6; color: white; padding: 2px 6px; border-radius: 4px; font-size: 0.7em; margin-left: 8px; vertical-align: middle; }
    .imperfect-graph { opacity: 0.85; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Unified Benchmark Dashboard</h1>
    <p>Graphs where all solvers perfectly matched the <strong><%= SOURCE_OF_TRUTH_SOLVER %></strong> source of truth are highlighted with a <span class="perfect-badge">PERFECT</span> badge.</p>

    <h2>1. Flow Correctness Matrix</h2>
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
        <% graphs_data.each do |row| %>
          <tr class="<%= row[:is_perfect] ? 'perfect-graph' : 'imperfect-graph' %>">
            <td class="graph-name">
              <%= row[:name] %>
              <%= "<span class='perfect-badge'>PERFECT</span>" if row[:is_perfect] %>
            </td>
            <% row[:correctness_cells].each do |cell| %>
              <td class="<%= cell[:class] %>"><%= cell[:text] %></td>
            <% end %>
          </tr>
        <% end %>
      </tbody>
    </table>

    <h2>2. Performance Times (Average Seconds)</h2>
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
        <% graphs_data.each do |row| %>
          <tr class="<%= row[:is_perfect] ? 'perfect-graph' : 'imperfect-graph' %>">
            <td class="graph-name">
              <%= row[:name] %>
              <%= "<span class='perfect-badge'>PERFECT</span>" if row[:is_perfect] %>
            </td>
            <% row[:time_cells].each do |cell| %>
              <td class="<%= cell[:class] %>"><%= cell[:text] %></td>
            <% end %>
          </tr>
        <% end %>
      </tbody>
    </table>
  </div>
</body>
</html>
ERB

renderer = ERB.new(html_template)
html_output = renderer.result(binding)

base_name = File.basename(log_file, '.*')
output_filename = "benchmark_dashboard_#{base_name}.html"

File.write(output_filename, html_output)
puts "Successfully unified logs! Generated dashboard: #{output_filename}"