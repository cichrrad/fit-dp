cmake_minimum_required(VERSION 3.16)
project(pbgl_test CXX)

find_package(MPI REQUIRED)
# Ask for the parallel component explicitly
find_package(Boost REQUIRED COMPONENTS graph_parallel graph mpi serialization)

add_executable(pbgl_test pbgl_test.cpp)
target_compile_features(pbgl_test PRIVATE cxx_std_17)

# Optional: belt-and-suspenders to ensure the MPI path inside headers is active
# target_compile_definitions(pbgl_test PRIVATE BOOST_GRAPH_USE_MPI)

target_link_libraries(pbgl_test PRIVATE
  MPI::MPI_CXX
  Boost::graph_parallel
  Boost::graph
  Boost::mpi
  Boost::serialization
)
