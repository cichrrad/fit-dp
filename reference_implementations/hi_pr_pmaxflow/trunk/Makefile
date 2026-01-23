CC = g++
all: pmaxsolve fprf_c_serial Makefile


pmaxsolve: main.c fprf.c fprf_queue.c fprf.h common.h fprf_queue.h fprf_queue_struct.h linux_atomic.h common.h
	$(CC) -g -O4 -Winline -Wall -DNDEBUG -DTBB_ATOMIC -o fprf_cpp_tbb main.c fprf.c fprf_queue.c -lpthread ${FLAGS}
	$(CC) -pg -Winline -Wall -DDEBUG -DTBB_ATOMIC -DPROFILE_WITH_GPROF -o fprf_cpp_tbb_profile main.c fprf.c fprf_queue.c -lpthread ${FLAGS}
	$(CC) -g -Winline -Wall -DDEBUG -DTBB_ATOMIC -o fprf_tbb_cpp_debug main.c fprf.c fprf_queue.c -lpthread ${FLAGS}

fprf_c_serial: main.c fprf.c fprf_queue.c fprf.h common.h fprf_queue.h fprf_queue_struct.h linux_atomic.h common.h
	$(CC) -O4 -Winline -Wall -DNDEBUG -DNO_PARALLEL -o fprf_c_serial main.c fprf.c fprf_queue.c -lpthread ${FLAGS}
	$(CC) -pg -Winline -Wall -DDEBUG -DNO_PARALLEL -DPROFILE_WITH_GPROF -o fprf_c_serial_profile main.c fprf.c fprf_queue.c -lpthread ${FLAGS}
	$(CC) -g -Winline -Wall -DDEBUG -DNO_PARALLEL -o fprf_c_serial_debug main.c fprf.c fprf_queue.c -lpthread ${FLAGS}
