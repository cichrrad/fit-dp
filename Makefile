.PHONY: build clean run run_lazy_check

build:
	mkdir -p build && cd build && cmake .. && cmake .. && make && cp knfs ../knfs && cd ../

clean:
	rm -rf build && mkdir build && cmake -B build -S . && cmake -B build -S . && cmake --build build && cp build/knfs knfs

run:
	mkdir -p build && cd build && cmake .. && cmake .. && make && cp knfs ../knfs && cd ../ && ./knfs

run_lazy_check:
	./knfs && cd tmp/ && ruby checker.rb && ruby generator.rb && cd ../