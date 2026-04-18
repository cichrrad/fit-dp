all: cpu

cpu:
	cmake --preset cpu
	cmake --build --preset cpu
	cp build/cpu/knfs knfs
	mv knfs knfs_cpu_multipar

gpu:
	cmake --preset gpu
	cmake --build --preset gpu
	cp build/gpu/knfs knfs
	mv knfs knfs_gpu_multipar

clean:
	rm -rf build

run_cpu_multipar:
	OMP_PROC_BIND=spread OMP_PLACES=threads ./knfs_cpu_multipar

run_gpu_multipar:
	OMP_PROC_BIND=spread OMP_PLACES=threads ./knfs_gpu_multipar

generate_graph:
	cd helpers/lazy_check && ruby generator.rb

to_dimacs:
	cd helpers/format_convertors/ && ruby csv_to_dimacs.rb ../../input/mock/generated_graph.csv > ../../input/mock/generated_graph.dimacs