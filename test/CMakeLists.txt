cmake_minimum_required(VERSION 3.16)
project(boost_graph_playground LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(MPI REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph graph_parallel mpi serialization)

add_executable(bgl_demo src/bgl_demo.cpp)
target_link_libraries(bgl_demo PRIVATE Boost::graph)

add_executable(pbgl_demo src/pbgl_demo.cpp)
target_link_libraries(pbgl_demo PRIVATE
  Boost::graph
  Boost::graph_parallel
  Boost::mpi
  Boost::serialization
  MPI::MPI_CXX)

# Optional: a `run_pbgl` target so you can `cmake --build . --target run_pbgl`
add_custom_target(run_pbgl
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:pbgl_demo>
  DEPENDS pbgl_demo
  USES_TERMINAL
)

