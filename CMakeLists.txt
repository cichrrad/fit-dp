cmake_minimum_required(VERSION 3.16)
project(KokkosPlayground CXX)
cmake_minimum_required(VERSION 3.16)
project(MyProject)

include(FetchContent)
FetchContent_Declare(
  Kokkos
  URL https://github.com/kokkos/kokkos/archive/refs/tags/4.5.01.zip
)

FetchContent_MakeAvailable(Kokkos)

add_executable(Playground main.cpp)
target_link_libraries(Playground PRIVATE Kokkos::kokkos)