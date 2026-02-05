# Default target
all: cpu

# Build for CPU (OpenMP)
cpu:
	cmake --preset cpu
	cmake --build --preset cpu
	cp build/cpu/knfs knfs

# Build for GPU (CUDA A100)
gpu:
	cmake --preset gpu
	cmake --build --preset gpu
	cp build/gpu/knfs knfs

clean:
	rm -rf build

run:
	export OMP_PROC_BIND=spread && export OMP_PLACES=threads && ./knfs

run_generate_check:
	cd helpers/lazy_check && ruby generator.rb && cd ../format_convertors/ && ruby csv_to_dimacs.rb ../../input/mock/generated_graph.csv > ../../input/mock/generated_graph.dimacs && cd ../../ && make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../

run_check:
	make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../

compare:
	make run_generate_check > tmp && cat tmp | grep -e "Size:" -e "Edges:" -e "Dens:" -e "With Deg:"  && rm tmp && echo "" && (echo "RESULTS FOR CURRENT" && make run && export OMP_PROC_BIND=spread && export OMP_PLACES=threads && echo "RESULTS FOR PAR:" && ./knfs_newer_but_old && export OMP_PROC_BIND=spread && export OMP_PLACES=threads && echo "RESULTS FOR OLD:" && ./knfs_old) | grep -e 'RESULTS FOR' -e 'FLOW' -e 'Time:'

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
