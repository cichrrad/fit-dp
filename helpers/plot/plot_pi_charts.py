import re
import matplotlib.pyplot as plt
import os

LOG_FILE_PATH = '../../logs/batch_run.log'  
OUTPUT_DIR = 'pi_chart_outputs'

def parse_log_file(filepath):
    """Parses log file into a list of dictionaries."""
    entries = []
    current_entry = {}
    
    patterns = {
        'filename': re.compile(r">>> RUNNING: (.+)"),
        'io_time': re.compile(r">> IO Time \(CSV Read\):\s+([\d\.]+)"),
        'init_time': re.compile(r">> Graph Build & Init Time:\s+([\d\.]+)"),
        'algo_time': re.compile(r">> Algorithm Runtime:\s+([\d\.]+)")
    }

    if not os.path.exists(filepath):
        print(f"Error: File '{filepath}' not found.")
        return []

    with open(filepath, 'r') as f:
        lines = f.readlines()

    for line in lines:
        line = line.strip()
        
        # Start of a new run
        match_file = patterns['filename'].search(line)
        if match_file:
            # If we have a previous entry recorded, save it
            if current_entry.get('filename'):
                entries.append(current_entry)
            
            current_entry = {
                'filename': os.path.basename(match_file.group(1)),
                'io_time': 0.0, 
                'init_time': 0.0, 
                'algo_time': 0.0
            }
            continue

        # Extract timings
        if 'filename' in current_entry:
            m_io = patterns['io_time'].search(line)
            if m_io: current_entry['io_time'] = float(m_io.group(1))
            
            m_init = patterns['init_time'].search(line)
            if m_init: current_entry['init_time'] = float(m_init.group(1))
            
            m_algo = patterns['algo_time'].search(line)
            if m_algo: current_entry['algo_time'] = float(m_algo.group(1))

    # Add the very last entry
    if current_entry.get('filename'):
        entries.append(current_entry)
        
    return entries

def generate_pie_charts(data, output_folder):
    """Generates a pie chart for each parsed entry."""
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    for i, entry in enumerate(data):
        # Prepare data for plotting
        labels = ['IO Read', 'Build & Init', 'Algorithm']
        sizes = [entry['io_time'], entry['init_time'], entry['algo_time']]
        
        # Colors: Red for IO, Blue for Init, Green for Algo
        colors = ['#ff9999', '#66b3ff', '#99ff99']
        explode = (0.05, 0.05, 0.05)  # Slightly separate slices for clarity

        plt.figure(figsize=(8, 6))
        
        # Create Pie Chart
        plt.pie(sizes, labels=labels, colors=colors, autopct='%1.1f%%',
                startangle=140, pctdistance=0.85, explode=explode, shadow=True)
        
        plt.title(f"Runtime Distribution: {entry['filename']}")
        
        # Save File
        safe_name = entry['filename'].replace('.', '_')
        save_path = os.path.join(output_folder, f"pie_{i}_{safe_name}.png")
        
        plt.savefig(save_path)
        plt.close()
        print(f"Generated: {save_path}")

if __name__ == "__main__":
    data = parse_log_file(LOG_FILE_PATH)
    if data:
        print(f"Found {len(data)} entries. Generating pie charts...")
        generate_pie_charts(data, OUTPUT_DIR)
        print("Done.")
    else:
        print("No entries found or log file is empty.")