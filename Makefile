.PHONY: build clean

build:
	mkdir -p build && cd build && cmake .. && cmake .. && make && cp knfs ../knfs && cd ../

clean:
	rm -rf build && mkdir build && cmake -B build -S . && cmake -B build -S . && cmake --build build && cp build/knfs knfs
