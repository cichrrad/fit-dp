
build:
	mkdir -p build && cd build && cmake .. && cmake .. && make && cp knfs ../knfs && cd ../

clean:
	rm -rf build && mkdir build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build -- VERBOSE=1 && cp build/knfs knfs

run:
	export OMP_PROC_BIND=spread && export OMP_PLACES=threads && ./knfs

run_generate_check:
	cd helpers/lazy_check && ruby generator.rb && cd ../../ && make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../

run_check:
	make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../

# Define the shell to ensure bash syntax (loops, etc.) works correctly
SHELL := /bin/bash

stress_test:
	@echo "Starting Stress Test (10 Global Cycles)..."
	@# Loop 1: Repeat the whole process 10 times
	@for cycle in {1..100}; do \
		echo "========================================"; \
		echo "Global Cycle: $$cycle / 100"; \
		echo "========================================"; \
		\
		# 1. Run generate check and capture output \
		echo "Running 'make run_generate_check'..."; \
		output=$$($(MAKE) run_generate_check 2>&1); \
		\
		# Extract the two values using awk (last field $$NF) \
		val1=$$(echo "$$output" | grep "MAX FLOW IS" | awk '{print $$NF}' | tail -n1); \
		val2=$$(echo "$$output" | grep "MAX FLOW:"   | awk '{print $$NF}' | tail -n1); \
		\
		# Debug print (optional, can be removed) \
		# echo "Found: $$val1 vs $$val2"; \
		\
		# Compare values \
		if [ -z "$$val1" ] || [ -z "$$val2" ]; then \
			echo "ERROR: Could not parse output from run_generate_check."; \
			echo "$$output"; \
			exit 1; \
		fi; \
		if [ "$$val1" != "$$val2" ]; then \
			echo "MISMATCH in generate_check!"; \
			echo "MAX FLOW IS: $$val1"; \
			echo "MAX FLOW:    $$val2"; \
			exit 1; \
		fi; \
		echo "Match confirmed ($$val1). Running 100 verification checks..."; \
		\
		# 2. Run check 100 times \
		for i in {1..100}; do \
			# Print a dot every 5 iterations to show progress without spamming \
			if (( i % 5 == 0 )); then echo -n "."; fi; \
			\
			output_sub=$$($(MAKE) run_check 2>&1); \
			sub_val1=$$(echo "$$output_sub" | grep "MAX FLOW IS" | awk '{print $$NF}' | tail -n1); \
			sub_val2=$$(echo "$$output_sub" | grep "MAX FLOW:"   | awk '{print $$NF}' | tail -n1); \
			\
			if [ "$$sub_val1" != "$$sub_val2" ]; then \
				echo ""; \
				echo "MISMATCH in run_check iteration $$i!"; \
				echo "MAX FLOW IS: $$sub_val1"; \
				echo "MAX FLOW:    $$sub_val2"; \
				exit 1; \
			fi; \
		done; \
		echo ""; \
		echo "Cycle $$cycle complete: 100 checks passed."; \
	done; \
	echo "SUCCESS: All 100 global cycles (and 10000 total sub-checks) passed."



batch_run:
	@# Initialize/Clear the log file
	@echo "Starting Batch Run at $$(date)" > batch_run.log
	@echo "----------------------------------------" >> batch_run.log

	@# Find all .csv files in input/data recursively
	@find input/data -type f -name "*.csv" | while read -r csv_file; do \
		echo "Processing: $$csv_file"; \
		\
		# 1. Copy the file to the mock location \
		cp "$$csv_file" input/mock/generated_graph.csv; \
		\
		# 2. Run the check and capture output (stout and stderr) \
		# We use '|| true' so the script doesn't abort if run_check returns an error code \
		OUTPUT=$$(make run 2>&1 || true); \
		\
		# 3. Append Header and Output to log \
		echo ">>> RUNNING: $$csv_file" >> batch_run.log; \
		echo "$$OUTPUT" >> batch_run.log; \
		\
# 		# 4. Extract values for comparison \
# 		# Pattern 1: "MAX FLOW: XXXXX" (Assuming value is the 3rd word) \
# 		VAL1=$$(echo "$$OUTPUT" | grep -o "MAX FLOW: [0-9]*" | awk '{print $$3}'); \
# 		# Pattern 2: "MAX FLOW IS XXXXX" (Assuming value is the 4th word) \
# 		VAL2=$$(echo "$$OUTPUT" | grep -o "MAX FLOW IS [0-9]*" | awk '{print $$4}'); \
# 		\
# 		# 5. Compare values \
# 		if [[ -n "$$VAL1" && -n "$$VAL2" ]]; then \
# 			if [ "$$VAL1" != "$$VAL2" ]; then \
# 				echo "" >> batch_run.log; \
# 				echo "[!!! FLOW MISMATCH!!!] ($$VAL1 vs $$VAL2)" >> batch_run.log; \
# 				echo "   -> Mismatch detected!"; \
# 			fi \
# 		else \
# 			echo "   -> Warning: Could not parse flow values for comparison."; \
# 			echo "[!!! PARSE ERROR !!!]" >> batch_run.log; \
# 		fi; \
		echo "----------------------------------------" >> batch_run.log; \
	done
	@echo "Batch run complete. Results saved in batch_run.log"