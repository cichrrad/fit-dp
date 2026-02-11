#!/usr/bin/env ruby
require 'erb'

# Usage: ruby log2html.rb path/to/benchmark.log
class HtmlLogParser
  SOLVER_REGEX = {
    "ECL_MaxFlow"     => /Maximum flow from nodes \d+ to \d+: (\d+)/,
    "hpf_pseudo_fifo" => /s Max Flow\s+:\s+(\d+)/,
    "hipr4"           => /c flow:\s+(\d+\.\d+)/,
    "pbbs_syncpar"    => /flow=(\d+)/,
    "knfs_gpu"        => /MAX FLOW IS\s+(\d+)/,
    "knfs_cpu"        => /MAX FLOW IS\s+(\d+)/
  }

  def initialize(filepath)
    @filepath = filepath
    @data = Hash.new { |h, k| h[k] = Hash.new { |hh, kk| hh[kk] = [] } }
    @solvers_seen = []
  end

  def parse
    current_graph = nil
    current_binary = nil

    puts "Parsing #{@filepath}..."
    File.foreach(@filepath) do |line|
      line.strip!
      if line =~ /=== PROCESSING GRAPH: (.*) ===/
        current_graph = $1.strip
        next
      end
      if line =~ /^BINARY: (.*)/
        current_binary = $1.strip
        @solvers_seen |= [current_binary] # Add unique
        next
      end

      next unless current_graph && current_binary

      if regex = SOLVER_REGEX[current_binary]
        if match = line.match(regex)
          val = match[1].to_f.to_i # Handle float strings
          @data[current_graph][current_binary] << val
        end
      end
    end

    generate_html
  end

  def get_consensus(graph_data)
    # Gather all successful flows from all solvers
    all_flows = []
    graph_data.each do |solver, flows|
      # Only consider if solver was stable (1 unique result)
      all_flows << flows.first if flows.uniq.length == 1
    end

    return nil if all_flows.empty?

    # Find the most common value (Mode)
    freq = all_flows.inject(Hash.new(0)) { |h, v| h[v] += 1; h }
    max_freq = freq.values.max
    # Return the value that appears most often
    freq.key(max_freq)
  end

  def generate_html
    template = <<~ERB
      <!DOCTYPE html>
      <html>
      <head>
        <title>Max Flow Benchmark Results</title>
        <style>
          body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; padding: 20px; background: #f4f4f9; }
          h1 { color: #333; }
          table { width: 100%; border-collapse: collapse; background: white; box-shadow: 0 1px 3px rgba(0,0,0,0.2); }
          th, td { padding: 12px; border: 1px solid #ddd; text-align: center; }
          th { background: #333; color: white; position: sticky; top: 0; }
          tr:hover { background-color: #f1f1f1; }
          
          /* Status Colors */
          .match { background-color: #d4edda; color: #155724; } /* Green */
          .mismatch { background-color: #f8d7da; color: #721c24; font-weight: bold; } /* Red */
          .variance { background-color: #fff3cd; color: #856404; } /* Yellow */
          .timeout { background-color: #e2e3e5; color: #6c757d; font-style: italic; } /* Grey */
          
          .legend { margin-bottom: 20px; padding: 10px; background: white; display: inline-block; border-radius: 5px; }
          .dot { height: 10px; width: 10px; display: inline-block; border-radius: 50%; margin-right: 5px; }
        </style>
      </head>
      <body>
        <div class="legend">
          <strong>Legend:</strong>
          <span style="margin-left:15px"><span class="dot" style="background:#d4edda"></span>Agrees with Majority</span>
          <span style="margin-left:15px"><span class="dot" style="background:#f8d7da"></span>Disagrees (Possible Bug)</span>
          <span style="margin-left:15px"><span class="dot" style="background:#fff3cd"></span>Internal Variance</span>
          <span style="margin-left:15px"><span class="dot" style="background:#e2e3e5"></span>Timeout</span>
        </div>

        <table>
          <thead>
            <tr>
              <th style="text-align:left;">Graph Name</th>
              <% @solvers_seen.sort.each do |solver| %>
                <th><%= solver %></th>
              <% end %>
            </tr>
          </thead>
          <tbody>
            <% @data.keys.sort.each do |graph| %>
              <% consensus = get_consensus(@data[graph]) %>
              <tr>
                <td style="text-align:left; font-weight:bold;"><%= graph %></td>
                <% @solvers_seen.sort.each do |solver| %>
                  <% 
                    flows = @data[graph][solver]
                    unique_flows = flows.uniq
                    css_class = ""
                    display_text = ""

                    if flows.empty?
                      display_text = "TIMEOUT"
                      css_class = "timeout"
                    elsif unique_flows.length > 1
                      display_text = "⚠ " + unique_flows.join(", ")
                      css_class = "variance"
                    else
                      val = unique_flows.first
                      display_text = val.to_s
                      if consensus && val != consensus
                        css_class = "mismatch"
                      elsif consensus && val == consensus
                        css_class = "match"
                      end
                    end
                  %>
                  <td class="<%= css_class %>">
                    <%= display_text %>
                  </td>
                <% end %>
              </tr>
            <% end %>
          </tbody>
        </table>
        <p style="margin-top: 20px; color: #666; font-size: 0.9em;">Generated on <%= Time.now %></p>
      </body>
      </html>
    ERB

    renderer = ERB.new(template)
    output_filename = "benchmark_report.html"
    File.write(output_filename, renderer.result(binding))
    puts "Report generated: #{output_filename}"
  end
end

log_file = ARGV[0] || Dir["logs/*.log"].sort.last
unless log_file
  puts "No log file found."
  exit
end

HtmlLogParser.new(log_file).parse