#!/usr/bin/env ruby
require 'fileutils'
require 'open3'
require 'time'
require 'timeout'

# --- CONFIGURATION ---
DIRS = {
  "dimacs" => "graphs/dimacs",
  "ecl"    => "graphs/ecl",
  "pbbs"   => "graphs/pbbs",
  "bin"    => "binaries",
  "logs"   => "logs"
}

THREAD_COUNTS = [128]
ITERATIONS = 10
TIMEOUT_SEC = 300

# Ensure log directory exists
FileUtils.mkdir_p(DIRS["logs"])

# Generate log filename
timestamp = Time.now.strftime("%Y%m%d_%H%M%S")
LOG_FILE = File.join(DIRS["logs"], "log_#{timestamp}.log")

# Open log file in append mode
$log_handle = File.open(LOG_FILE, "a")

def log_message(message, to_console: true)
  puts message if to_console
  $log_handle.puts(message)
  $log_handle.flush
end

def get_graph_info(dimacs_path)
  info = { "nodes" => nil, "edges" => nil, "source" => nil, "sink" => nil }

  begin
    File.foreach(dimacs_path) do |line|
      parts = line.split
      next if parts.empty?

      if parts[0] == 'p' && parts.length >= 4
        info["nodes"] = parts[2]
        info["edges"] = parts[3]
      elsif parts[0] == 'n' && parts.length >= 3
        node_id = parts[1].to_i
        node_type = parts[2]
        adjusted_id = node_id - 1 # 0-based adjustment

        if node_type == 's'
          info["source"] = adjusted_id
        elsif node_type == 't'
          info["sink"] = adjusted_id
        end
      end
      break if info["nodes"] && info["edges"] && info["source"] && info["sink"]
    end
  rescue => e
    puts "Error parsing #{dimacs_path}: #{e}"
    return nil
  end
  info
end

def run_command(cmd_str, header_info)
  header = <<~HEADER

    #{'='*60}
    BENCHMARK_RUN
    BINARY: #{header_info[:binary]}
    GRAPH: #{header_info[:graph_name]}
    ITERATION: #{header_info[:iteration]}
    THREADS: #{header_info[:threads] || 'N/A'}
    CMD: #{cmd_str}
    TIMEOUT_LIMIT: #{TIMEOUT_SEC}s
    #{'-'*60}
  HEADER

  log_message(header, to_console: false)
  print "    --> Run #{header_info[:iteration]}: #{header_info[:binary]}... "

  output = ""
  status_code = 0
  
  begin
    Open3.popen2e(cmd_str) do |stdin, stdout_stderr, wait_thr|
      stdin.close
      
      begin
        Timeout.timeout(TIMEOUT_SEC) do
          # Read output until EOF
          output = stdout_stderr.read
          # Wait for process to finish and get exit code
          status_code = wait_thr.value.exitstatus
        end
        
        puts "DONE" # Finish the console line
        log_message(output, to_console: false)
        log_message("EXIT_STATUS: #{status_code}", to_console: false)

      rescue Timeout::Error
        puts "TIMED OUT" # Finish the console line
        
        # Kill the process
        Process.kill("KILL", wait_thr.pid) rescue nil
        
        # Log the timeout failure
        log_message("\n*** ERROR: EXECUTION TIMED OUT AFTER #{TIMEOUT_SEC}s ***", to_console: false)
        
        log_message("PARTIAL_OUTPUT:\n#{output}", to_console: false) unless output.empty?
        
        # Use -1 to denote timeout/failure in parser
        log_message("EXIT_STATUS: -1", to_console: false)
      end
    end

  rescue => e
    puts "ERROR"
    log_message("CRITICAL_SCRIPT_ERROR: #{e}", to_console: true)
  end
end


log_message("Starting Benchmark. Logging to #{LOG_FILE}")
log_message("Timeout Threshold: #{TIMEOUT_SEC} seconds per run.")

log_message(">>> PHASE 1: CHECKING/CONVERTING GRAPHS")

dimacs_files = Dir.glob(File.join(DIRS["dimacs"], "*.dimacs"))

if dimacs_files.empty?
  puts "No .dimacs files found in graphs/dimacs/"
  exit
end

dimacs_files.each do |d_path|
  basename = File.basename(d_path, ".*")
  
  ecl_path = File.join(DIRS["ecl"], "#{basename}.ecl")
  pbbs_path = File.join(DIRS["pbbs"], "#{basename}.pbbs")

  # Check/Convert ECL
  unless File.exist?(ecl_path)
    converter = File.join(DIRS["bin"], "dimacs2ecl")
    if File.exist?(converter)
      puts "Converting #{basename} to ECL..."
      system("#{converter} #{d_path} #{ecl_path}")
    end
  end

  # Check/Convert PBBS
  unless File.exist?(pbbs_path)
    converter = File.join(DIRS["bin"], "dimacs2pbbs")
    if File.exist?(converter)
      puts "Converting #{basename} to PBBS..."
      system("#{converter} #{d_path} #{pbbs_path}")
    end
  end
end

log_message("\n>>> PHASE 2: RUNNING SOLVERS")

dimacs_files.each do |d_path|
  graph_name = File.basename(d_path, ".*")
  log_message("\n=== PROCESSING GRAPH: #{graph_name} ===")

  info = get_graph_info(d_path)
  
  if info.nil? || info["source"].nil? || info["sink"].nil?
    log_message("SKIPPING #{graph_name}: Bad header/info.")
    next
  end

  meta_header = <<~META
    GRAPH_METADATA
    NAME: #{graph_name}
    NODES: #{info['nodes']}
    EDGES: #{info['edges']}
    SOURCE_ID_ADJUSTED: #{info['source']}
    SINK_ID_ADJUSTED: #{info['sink']}
  META
  
  log_message(meta_header, to_console: false)

  ecl_input = File.join(DIRS["ecl"], "#{graph_name}.ecl")
  pbbs_input = File.join(DIRS["pbbs"], "#{graph_name}.pbbs")
  
  # 1. ECL_MaxFlow
  bin = File.join(DIRS["bin"], "ECL_MaxFlow")
  cmd = "#{bin} #{ecl_input} #{info['source']} #{info['sink']}"
  (1..ITERATIONS).each do |i|
    run_command(cmd, { binary: "ECL_MaxFlow", graph_name: graph_name, iteration: i })
  end

  # 2. hpf_pseudo_fifo
  bin = File.join(DIRS["bin"], "hpf_pseudo_fifo")
  cmd = "#{bin} < #{d_path}"
  (1..ITERATIONS).each do |i|
    run_command(cmd, { binary: "hpf_pseudo_fifo", graph_name: graph_name, iteration: i })
  end

  # 3. hipr4
  bin = File.join(DIRS["bin"], "hipr4")
  cmd = "#{bin} < #{d_path}"
  (1..ITERATIONS).each do |i|
    run_command(cmd, { binary: "hipr4", graph_name: graph_name, iteration: i })
  end

  # 4. pbbs_syncpar
  bin = File.join(DIRS["bin"], "pbbs_syncpar")
  cmd = "#{bin} #{pbbs_input}"
  (1..ITERATIONS).each do |i|
    run_command(cmd, { binary: "pbbs_syncpar", graph_name: graph_name, iteration: i })
  end

  # 5. Threaded Solvers
  # ["knfs_gpu", "knfs_cpu"].each do |bin_base|
  #   bin_path = File.join(DIRS["bin"], bin_base)
  #   THREAD_COUNTS.each do |threads|
  #     cmd = "#{bin_path} #{d_path} #{threads}"
  #     (1..ITERATIONS).each do |i|
  #       run_command(cmd, { binary: bin_base, graph_name: graph_name, iteration: i, threads: threads })
  #     end
  #   end
  # end

end

log_message("\nBenchmark finished. Logs saved to #{LOG_FILE}")
$log_handle.close