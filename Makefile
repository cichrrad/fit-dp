all: cpu

cpu:
	cmake --preset cpu
	cmake --build --preset cpu
	cp build/cpu/knfs knfs
	cp knfs benchmarking/binaries/knfs_cpu
	mv knfs knfs_cpu

gpu:
	cmake --preset gpu
	cmake --build --preset gpu
	cp build/gpu/knfs knfs
	cp knfs benchmarking/binaries/knfs_gpu
	mv knfs knfs_gpu

clean:
	rm -rf build

run_cpu:
	OMP_PROC_BIND=spread OMP_PLACES=threads ./knfs_cpu

run_gpu:
	OMP_PROC_BIND=spread OMP_PLACES=threads ./knfs_gpu

generate_graph:
	cd helpers/lazy_check && ruby generator.rb

to_dimacs:
	cd helpers/format_convertors/ && ruby csv_to_dimacs.rb ../../input/mock/generated_graph.csv > ../../input/mock/generated_graph.dimacs
