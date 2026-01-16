.PHONY: build clean run run_lazy_check

build:
	mkdir -p build && cd build && cmake .. && cmake .. && make && cp knfs ../knfs && cd ../

clean:
	rm -rf build && mkdir build && cmake -B build -S . && cmake -B build -S . && cmake --build build && cp build/knfs knfs

run:
	export OMP_PROC_BIND=spread && export OMP_PLACES=threads && ./knfs

run_generate_check:
	cd helpers/lazy_check && ruby generator.rb && cd ../../ && make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../

run_check:
	make run && cd helpers/lazy_check/ && ruby checker.rb && cd ../../
