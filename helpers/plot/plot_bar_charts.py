import re
import matplotlib.pyplot as plt
import os

LOG_FILE_PATH = '../../batch_run.log'  
OUTPUT_DIR = 'bar_chart_outputs'            

def parse_log_file(filepath):
    entries = []
    current_entry = {}
    
    patterns = {
        'filename': re.compile(r">>> RUNNING: (.+)"),
        'io_time': re.compile(r">> IO Time \(CSV Read\):\s+([\d\.]+)\s+seconds"),
        'init_time': re.compile(r">> Graph Build & Init Time:\s+([\d\.]+)\s+seconds"),
        'algo_time': re.compile(r">> Algorithm Runtime:\s+([\d\.]+)\s+seconds"),
        'separator': re.compile(r"^-{10,}$") 
    }

    with open(filepath, 'r') as f:
        lines = f.readlines()

    for line in lines:
        line = line.strip()

        # Check for filename (Start of new entry)
        match_file = patterns['filename'].search(line)
        if match_file:
            # If we were building an entry and hit a new "RUNNING", save the old one
            if current_entry.get('filename'):
                entries.append(current_entry)
            
            current_entry = {
                'filename': os.path.basename(match_file.group(1)), # Clean up path
                'io_time': 0.0,
                'init_time': 0.0,
                'algo_time': 0.0
            }
            continue

        # Extract Times
        match_io = patterns['io_time'].search(line)
        if match_io and 'filename' in current_entry:
            current_entry['io_time'] = float(match_io.group(1))

        match_init = patterns['init_time'].search(line)
        if match_init and 'filename' in current_entry:
            current_entry['init_time'] = float(match_init.group(1))

        match_algo = patterns['algo_time'].search(line)
        if match_algo and 'filename' in current_entry:
            current_entry['algo_time'] = float(match_algo.group(1))

        if patterns['separator'].match(line) and 'algo_time' in current_entry:
            if current_entry.get('filename'):
                entries.append(current_entry)
                current_entry = {}

    if current_entry.get('filename') and current_entry.get('algo_time'):
        entries.append(current_entry)

    return entries

def generate_charts(data, output_folder):
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    for i, entry in enumerate(data):
        filename = entry['filename']
        times = [entry['io_time'], entry['init_time'], entry['algo_time']]
        labels = ['IO Read', 'Build & Init', 'Algorithm']
        colors = ["#ff9e99", '#66b3ff', '#99ff99']

        plt.figure(figsize=(10, 4))
        
        # Create horizontal bar
        y_pos = range(len(labels))
        bars = plt.barh(y_pos, times, color=colors)

        plt.yticks(y_pos, labels)
        plt.xlabel('Time (seconds)')
        plt.title(f'Performance Breakdown: {filename}')
        
        plt.bar_label(bars, fmt='%.4f s', padding=3)

        plt.tight_layout()
        
        safe_name = filename.replace('.', '_').replace('/', '_')
        save_path = os.path.join(output_folder, f"plot_{i}_{safe_name}.png")
        
        plt.savefig(save_path)
        plt.close() # Close memory to avoid eating RAM on large log files
        print(f"Generated plot for {filename} -> {save_path}")

# --- Main Execution ---
if __name__ == "__main__":
    if not os.path.exists(LOG_FILE_PATH):
        print(f"'{LOG_FILE_PATH}' not found.")
    else:
        parsed_data = parse_log_file(LOG_FILE_PATH)
        print(f"Found {len(parsed_data)} entries. Generating charts...")
        generate_charts(parsed_data, OUTPUT_DIR)
        print("Done!")