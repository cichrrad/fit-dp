cmake_minimum_required(VERSION 3.20)
project(dp_cichra)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  Kokkos
  URL https://github.com/kokkos/kokkos/archive/refs/tags/4.5.01.zip
)
FetchContent_MakeAvailable(Kokkos)

add_executable(knfs main.cpp)
target_link_libraries(knfs PRIVATE Kokkos::kokkos)

if(MSVC)
  target_compile_options(knfs PRIVATE /O2 /Oi /Ot)
else()
  target_compile_options(knfs PRIVATE -O3)
endif()