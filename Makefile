all: cpu

cpu:
	cmake --preset cpu
	cmake --build --preset cpu
	cp build/cpu/knfs knfs

gpu:
	cmake --preset gpu
	cmake --build --preset gpu
	cp build/gpu/knfs knfs

clean:
	rm -rf build

run:
	OMP_PROC_BIND=spread OMP_PLACES=threads ./knfs

generate_graph:
	cd helpers/lazy_check && ruby generator.rb

to_dimacs:
	cd helpers/format_convertors/ && ruby csv_to_dimacs.rb ../../input/mock/generated_graph.csv > ../../input/mock/generated_graph.dimacs

SHELL := /bin/bash
OUTER_ITERS = 1000
INNER_ITERS = 100

stress_test:
	@echo "Starting stress test: $(OUTER_ITERS) outer x $(INNER_ITERS) inner loops"
	@for (( i=1; i<=$(OUTER_ITERS); i++ )); do \
		echo "--- Outer Iteration $$i: Generating new graph ---"; \
		cd helpers/lazy_check;\
		ruby generator.rb; \
		cd ../../;\
		make to_dimacs; \
		\
		echo "Running reference solver..."; \
		REF_OUT=$$(./reference_implementations/hpf/pseudo_lifo < input/mock/generated_graph.dimacs | tail -n 1); \
		REF_FLOW=$$(echo $$REF_OUT | grep -oP '(?<=: )\d+'); \
		echo "Reference Flow: $$REF_FLOW"; \
		\
		for (( j=1; j<=$(INNER_ITERS); j++ )); do \
			echo -ne "  Inner Iteration $$j: " ; \
			MY_OUT=$$(make run | tail -n 2); \
			MY_FLOW=$$(echo $$MY_OUT | grep -oP '(?<=MAX FLOW IS )\d+'); \
			\
			if [ "$$REF_FLOW" != "$$MY_FLOW" ]; then \
				echo "FAILED!"; \
				echo "Mismatch found! Ref: $$REF_FLOW, Mine: $$MY_FLOW"; \
				exit 1; \
			else \
				echo "PASSED"; \
			fi; \
		done; \
	done
	@echo "All tests passed successfully!"
