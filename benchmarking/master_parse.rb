#!/usr/bin/env ruby
require 'erb'
require 'cgi'
require 'json'

# Configuration
EXPECTED_RUNS = 10
SOURCE_OF_TRUTH_SOLVER = 'hpf_pseudo_fifo'

FLOW_REGEXES = {
  'ECL_MaxFlow' => /Maximum flow from nodes \d+ to \d+: (\d+)/,
  'hpf_pseudo_fifo' => /^s Max Flow\s+:\s+(\d+)/,
  'hipr4' => /^c flow:\s+([\d.]+)/,
  'pbbs_syncpar' => /flow=(\d+)/,
  'knfs_gpu_AE' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu_AP' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu_AP2' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu_old' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_AE' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_AP' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_AP2' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_loader' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu_loader' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_old' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu' => /MAX FLOW IS\s+(\d+)/,
  'knfs_gpu_multipar' => /MAX FLOW IS\s+(\d+)/,
  'knfs_cpu_multipar' => /MAX FLOW IS\s+(\d+)/
}

log_file = ARGV[0]
unless log_file && File.exist?(log_file)
  puts 'Usage: ruby parse_master.rb <path_to_log_file>'
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
    current_graph = Regexp.last_match(1)
    current_solver = nil
    valid_run = false
    next
  end

  # Binary Header
  if line =~ /^--- BINARY: ([^ ]+) \|/
    if valid_run && current_graph && current_solver && current_time > 0
      results[current_graph][current_solver][:times] << current_time
    end
    current_solver = Regexp.last_match(1)
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
      results[current_graph][current_solver][:flows] << Regexp.last_match(1).to_f.to_i.to_s
    end

    # Extract Time
    case current_solver
    when 'ECL_MaxFlow'
      if line =~ /header total init time:\s+([\d.]+)s/ || line =~ /^runtime:\s+([\d.]+)s/
        current_time += Regexp.last_match(1).to_f
      end
    when 'hpf_pseudo_fifo'
      current_time += Regexp.last_match(2).to_f if line =~ /c Time to (read|initialize|min cut|max flow)\s*:\s+([\d.]+)/
    when 'hipr4'
      current_time += Regexp.last_match(2).to_f if line =~ /c (time|init tm):\s+([\d.]+)/
    when 'pbbs_syncpar'
      current_time += Regexp.last_match(1).to_f if line =~ /PBBS-time:\s+([\d.]+)/ || line =~ /deinit time:\s+([\d.]+)/
    when /^knfs_/
      current_time += Regexp.last_match(1).to_f if line =~ /TOTAL Runtime .*:\s+([\d.]+)/
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
  row_raw_times = []
  row_is_correct = [] # Boolean array to track correctness for the chart

  solvers_seen.each do |solver|
    flows = solvers_data[solver][:flows] || []
    times = solvers_data[solver][:times] || []
    unique_flows = flows.uniq
    timeout_count = EXPECTED_RUNS - flows.size
    timeout_str = timeout_count > 0 ? "<br><span class='timeout-text'>(#{timeout_count} timeouts)</span>" : ''

    # Evaluate Correctness
    is_correct = false
    c_class = ''
    c_text = ''

    if flows.empty?
      c_class = 'timeout-cell'
      c_text = 'Timed out'
    elsif solver == SOURCE_OF_TRUTH_SOLVER
      c_class = 'match'
      c_text = unique_flows.join(' vs ') + timeout_str
      is_correct = true
    elsif unique_flows.size > 1
      c_class = 'warning'
      formatted_flows = unique_flows.map { |f| f == truth_flow ? "<span class='truth-match'>#{f}</span>" : f }
      c_text = formatted_flows.join(' vs ') + timeout_str
    elsif unique_flows.first == truth_flow
      c_class = 'match'
      c_text = unique_flows.first + timeout_str
      is_correct = true
    else
      c_class = 'error'
      c_text = unique_flows.first + timeout_str
    end

    row_correctness << { class: c_class, text: c_text }
    row_is_correct << is_correct

    # Formatting Time Cell & Extracting Raw Times
    t_class = ''
    t_text = ''
    if times.empty?
      t_class = 'timeout-cell'
      t_text = 'N/A'
      row_raw_times << nil
    else
      avg = times.sum / times.size
      t_class = avg == fastest_time ? 'match-time text-bold' : ''
      t_text = format('%.4f s', avg) + timeout_str
      row_raw_times << avg.round(4)
    end
    row_times << { class: t_class, text: t_text }
  end

  graphs_data << {
    name: graph_name,
    is_perfect: is_perfect,
    correctness_cells: row_correctness,
    time_cells: row_times,
    raw_times: row_raw_times,
    is_correct: row_is_correct # Sent to JS to style the charts
  }
end

# RENDER HTML
html_template = <<~ERB
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <title>Unified Max Flow Benchmark Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
      body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 20px; color: #333; background-color: #f4f6f8; }
      .container { max-width: 1400px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
      h1, h2 { color: #2c3e50; }
  #{'    '}
      .section-header { margin-top: 40px; border-bottom: 2px solid #e2e8f0; padding-bottom: 10px; display: flex; justify-content: space-between; align-items: baseline; }
      .section-header h2 { margin: 0; border: none; padding: 0; }
  #{'    '}
      table { border-collapse: collapse; width: 100%; margin-top: 20px; font-size: 0.95em; }
      th, td { border: 1px solid #cbd5e1; padding: 12px 15px; text-align: left; vertical-align: top; }
      th { background-color: #f8fafc; font-weight: bold; color: #475569; position: sticky; top: 0; }
  #{'    '}
      /* Cell Styles */
      .match { background-color: #dcfce7; color: #166534; }#{' '}
      .match-time { background-color: #dcfce7; color: #166534; font-weight: bold; border: 2px solid #22c55e; }#{' '}
      .warning { background-color: #fef08a; color: #854d0e; }#{' '}
      .error { background-color: #fee2e2; color: #991b1b; }#{' '}
      .timeout-cell { background-color: #f1f5f9; color: #64748b; font-style: italic; }#{' '}
  #{'    '}
      .timeout-text { color: #ef4444; font-size: 0.85em; font-weight: bold; }
      .truth-match { font-weight: bold; text-decoration: underline; }
  #{'    '}
      /* Perfect Graph Highlighting */
      .perfect-graph td { border-top: 2px solid #3b82f6; border-bottom: 2px solid #3b82f6; }
      .perfect-graph .graph-name { background-color: #eff6ff; color: #1d4ed8; font-weight: bold; }
      .perfect-badge { display: inline-block; background: #3b82f6; color: white; padding: 2px 6px; border-radius: 4px; font-size: 0.7em; margin-left: 8px; vertical-align: middle; }
      .imperfect-graph { opacity: 0.85; }

      /* Chart Grid Styles */
      .chart-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(400px, 1fr)); gap: 20px; margin-top: 20px; }
      .chart-card { background: white; padding: 20px; border-radius: 8px; border: 1px solid #cbd5e1; box-shadow: 0 2px 4px rgba(0,0,0,0.02); }
      .chart-card h3 { margin-top: 0; font-size: 1.1em; color: #475569; text-align: center; }
  #{'    '}
      /* Toggle switch label */
      .toggle-label { cursor: pointer; font-weight: bold; color: #475569; font-size: 0.9em; background: #f8fafc; padding: 8px 12px; border-radius: 6px; border: 1px solid #cbd5e1;}
      .toggle-label:hover { background: #e2e8f0; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Unified Benchmark Dashboard</h1>
      <p>Graphs where all solvers perfectly matched the <strong><%= SOURCE_OF_TRUTH_SOLVER %></strong> source of truth are highlighted with a <span class="perfect-badge">PERFECT</span> badge.</p>

      <div class="section-header">
        <h2>1. Flow Correctness Matrix</h2>
      </div>
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

      <div class="section-header">
        <h2>2. Performance Times (Average Seconds)</h2>
      </div>
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

      <div class="section-header">
        <h2>3. Performance Charts</h2>
        <label class="toggle-label">
          <input type="checkbox" id="sortToggle"> Sort by runtime (Ascending)
        </label>
      </div>
      <p style="font-size: 0.85em; color: #64748b; margin-top: -5px;">* Bars for solvers that yielded an <b>incorrect flow</b> or non-deterministic results are drawn with dashed red borders.</p>
  #{'    '}
      <div class="chart-grid">
        <% graphs_data.each_with_index do |row, index| %>
          <div class="chart-card">
            <h3><%= row[:name] %></h3>
            <canvas id="chart_<%= index %>"></canvas>
          </div>
        <% end %>
      </div>
    </div>

    <script>
      const baseSolvers = <%= solvers_seen.to_json %>;
      const graphsData = <%= graphs_data.to_json %>;
      let chartInstances = [];

      // Color palette logic based on correctness flag
      const getBgColor = (isCorrect) => isCorrect ? 'rgba(59, 130, 246, 0.6)' : 'rgba(239, 68, 68, 0.3)'; // Solid Blue vs Pale Red
      const getBorderColor = (isCorrect) => isCorrect ? 'rgba(37, 99, 235, 1)' : 'rgba(220, 38, 38, 1)'; // Solid Blue vs Solid Red
      const getBorderDash = (isCorrect) => isCorrect ? [] : [5, 5]; // Solid vs Dashed

      const chartOptions = {
        responsive: true,
        plugins: {
          legend: { display: false },
          tooltip: {
            callbacks: {
              label: function(context) {
                const valStr = context.raw === null ? 'Timeout' : context.raw + ' s';
                // Tooltip doesn't easily access external is_correct arrays without dirty lookups,#{' '}
                // but visually the bar handles the warning anyway.
                return valStr;
              }
            }
          }
        },
        scales: {
          y: { beginAtZero: true, title: { display: true, text: 'Seconds' } }
        }
      };

      // Initialize all charts
      graphsData.forEach((graph, index) => {
        const canvas = document.getElementById('chart_' + index);
        if (!canvas) return;
  #{'      '}
        const ctx = canvas.getContext('2d');
        const chart = new Chart(ctx, {
          type: 'bar',
          data: {
            labels: [...baseSolvers], // Copy array to allow mutation
            datasets: [{
              label: 'Average Runtime (s)',
              data: [...graph.raw_times],
              backgroundColor: graph.is_correct.map(getBgColor),
              borderColor: graph.is_correct.map(getBorderColor),
              borderWidth: 1.5,
              borderDash: graph.is_correct.map(getBorderDash),
              borderRadius: 4
            }]
          },
          options: chartOptions
        });
  #{'      '}
        // Stash original arrays so we can sort/unsort perfectly
        chart.originalLabels = [...baseSolvers];
        chart.originalData = [...graph.raw_times];
        chart.originalCorrectness = [...graph.is_correct];
  #{'      '}
        chartInstances.push(chart);
      });

      // Handle the sort toggle
      document.getElementById('sortToggle').addEventListener('change', function(e) {
        const doSort = e.target.checked;
  #{'      '}
        chartInstances.forEach(chart => {
          if (doSort) {
            // Pair the label, value, and correctness status together
            let paired = chart.originalLabels.map((label, i) => {
              return {#{' '}
                label: label,#{' '}
                val: chart.originalData[i],#{' '}
                correct: chart.originalCorrectness[i]#{' '}
              };
            });
  #{'          '}
            // Sort ascending (treat null/timeout as Infinity to push to far right)
            paired.sort((a, b) => {
              let valA = a.val === null ? Infinity : a.val;
              let valB = b.val === null ? Infinity : b.val;
              return valA - valB;
            });
  #{'          '}
            // Apply sorted arrays back to the chart
            chart.data.labels = paired.map(p => p.label);
            chart.data.datasets[0].data = paired.map(p => p.val);
            chart.data.datasets[0].backgroundColor = paired.map(p => getBgColor(p.correct));
            chart.data.datasets[0].borderColor = paired.map(p => getBorderColor(p.correct));
            chart.data.datasets[0].borderDash = paired.map(p => getBorderDash(p.correct));
  #{'          '}
          } else {
            // Revert to original parsed order
            chart.data.labels = [...chart.originalLabels];
            chart.data.datasets[0].data = [...chart.originalData];
            chart.data.datasets[0].backgroundColor = chart.originalCorrectness.map(getBgColor);
            chart.data.datasets[0].borderColor = chart.originalCorrectness.map(getBorderColor);
            chart.data.datasets[0].borderDash = chart.originalCorrectness.map(getBorderDash);
          }
  #{'        '}
          chart.update(); // Trigger the animation
        });
      });
    </script>
  </body>
  </html>
ERB

renderer = ERB.new(html_template)
html_output = renderer.result(binding)

base_name = File.basename(log_file, '.*')
output_filename = "benchmark_dashboard_#{base_name}.html"

File.write(output_filename, html_output)
puts "Successfully unified logs! Generated dashboard: #{output_filename}"
