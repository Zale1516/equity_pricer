cmake_minimum_required(VERSION 3.20)
project(equity_pricer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # feeds VS Code IntelliSense
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)
add_compile_options($<$<CONFIG:Release>:-O2>)
add_compile_options($<$<CONFIG:Debug>:-g>)

# Eigen: required for SSVI calibration (Levenberg-Marquardt). Install with: brew install eigen
find_package(Eigen3 REQUIRED NO_MODULE)
message(STATUS "Eigen ${Eigen3_VERSION} found.")

# Core static library: pricing engines, market/vol/date primitives.
add_library(ep STATIC
  src/date.cpp
  src/json.cpp
  src/market.cpp
  src/vol_surface.cpp
  src/accumulator.cpp
  src/ssvi.cpp
  src/local_vol.cpp
  src/pde_engine.cpp
  src/analytic_engine.cpp
)
target_include_directories(ep PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ep PRIVATE Eigen3::Eigen)   # only ssvi.cpp needs Eigen

add_executable(ep_app src/main.cpp)
target_link_libraries(ep_app PRIVATE ep)

enable_testing()
add_executable(ep_smoke tests/smoke_test.cpp)
target_link_libraries(ep_smoke PRIVATE ep)
add_test(NAME smoke COMMAND ep_smoke ${CMAKE_SOURCE_DIR}/data/vol_surface.csv)
