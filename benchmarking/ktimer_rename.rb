#!/usr/bin/env ruby

require 'optparse'
require 'fileutils'
require 'pathname'

options = {
  dump_dir: './profiler_dumps/kernel_timer',
  dry_run: false
}

parser = OptionParser.new do |opts|
  opts.banner = 'Usage: ruby rename_dumps.rb [options] <log_file>'

  opts.on('--dump_dir DIR', String,
          'Directory containing the .dat dumps (default: ./profiler_dumps/kernel_timer)') do |dir|
    options[:dump_dir] = dir
  end

  opts.on('--dry-run', 'Print what would happen without actually renaming files.') do
    options[:dry_run] = true
  end

  opts.on('-h', '--help', 'Prints this help') do
    puts opts
    exit
  end
end

parser.parse!
log_file = ARGV.first

if log_file.nil?
  puts 'Error: Missing log file argument.'
  puts parser.help
  exit 1
end

log_path = Pathname.new(log_file)
dump_dir = Pathname.new(options[:dump_dir])

unless log_path.exist?
  puts "Error: Log file '#{log_path}' not found."
  exit 1
end

if !dump_dir.exist? && !options[:dry_run]
  puts "Error: Dump directory '#{dump_dir}' not found. Check the path or use --dry-run."
  exit 1
end

header_pattern = /^---\s*BINARY:\s*(knfs_gpu|.*_cpu)\s*\|\s*ITERATION:\s*(\d+)/
graph_pattern = /Loading graph from:\s*(.*)/
dump_pattern = /KokkosP: Kernel timing written to\s*(.*\.dat)\s*$/

current_binary = nil
current_iteration = nil
current_graph = nil

puts "Scanning log: #{log_path.basename}...\n" + '-' * 40

File.foreach(log_path).with_index(1) do |line, line_num|
  line.strip!

  # Look for a valid header
  if match = header_pattern.match(line)
    current_binary = match[1]
    current_iteration = match[2].to_i
    current_graph = nil # Reset graph state for the new block
    next
  end

  # If we are in an ignored binary's block, skip parsing other lines
  next unless current_binary

  # Look for the graph file
  if match = graph_pattern.match(line)
    current_graph = File.basename(match[1])
    next
  end

  # Look for the dump file output
  if match = dump_pattern.match(line)
    full_dump_path = match[1]
    original_dump_name = File.basename(full_dump_path)

    if current_graph && current_iteration
      # Construct the new filename with 2-digit zero-padded iteration
      new_name = format('%s_%02d_%s_%s', current_binary, current_iteration, current_graph, original_dump_name)

      old_file = dump_dir.join(original_dump_name)
      new_file = dump_dir.join(new_name)

      puts 'Match found:'
      puts "  Target:  #{original_dump_name}"
      puts "  Rename:  #{new_name}"

      if options[:dry_run]
        puts '  [DRY RUN] Would rename file.'
      elsif old_file.exist?
        FileUtils.mv(old_file, new_file)
        puts '  [SUCCESS] File renamed.'
      else
        puts "  [WARNING] Could not find '#{old_file}' in directory."
      end
      puts '-' * 40
    else
      puts "[WARNING] Line #{line_num}: Found dump file but missing graph or iteration info."
    end

    # Reset state after successfully handling a dump line
    current_binary = nil
    current_iteration = nil
    current_graph = nil
  end
end
