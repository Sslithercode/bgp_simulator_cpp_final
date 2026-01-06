cmake_minimum_required(VERSION 3.15)
project(BGPSimulator VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Find required packages
find_package(CURL REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})

# Print information
message(STATUS "============================================")
message(STATUS "BGP Simulator Build Configuration")
message(STATUS "============================================")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CURL Include: ${CURL_INCLUDE_DIRS}")
message(STATUS "CURL Library: ${CURL_LIBRARIES}")
message(STATUS "============================================")

# Task 2.2: CAIDA Downloader
add_executable(caida_downloader src/caida_downloader.cpp)
target_link_libraries(caida_downloader PRIVATE ${CURL_LIBRARIES})

# Section 3: BGP Functionality
add_library(bgp STATIC src/announcement.cpp src/bgp_policy.cpp)
set_target_properties(bgp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Task 2.3: AS Graph Library (depends on bgp)
add_library(as_graph STATIC src/as_graph.cpp)
target_link_libraries(as_graph PUBLIC bgp)
set_target_properties(as_graph PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Task 2.3 & 2.4: AS Graph Test/Demo
# Commented out because tests are optional and the source file may be missing
# add_executable(as_graph_test tests/as_graph_test.cpp)
# target_link_libraries(as_graph_test PRIVATE as_graph)

# Section 3: BGP Simulator (full) - main production version
add_executable(bgp_simulator src/bgp_simulator_main.cpp)
target_link_libraries(bgp_simulator PRIVATE as_graph)

# Section 3: BGP Simulator (simple test version)
add_executable(bgp_simulator_simple src/bgp_simulator.cpp)
target_link_libraries(bgp_simulator_simple PRIVATE as_graph)

# Section 4: ROV Test
add_executable(bgp_rov_test src/bgp_rov_test.cpp)
target_link_libraries(bgp_rov_test PRIVATE as_graph)

# Python Bindings (optional - requires pybind11)
find_package(Python COMPONENTS Interpreter Development QUIET)
if(Python_FOUND)
    # Try to find pybind11
    find_package(pybind11 QUIET)
    if(pybind11_FOUND)
        message(STATUS "============================================")
        message(STATUS "Python bindings will be built")
        message(STATUS "Python: ${Python_VERSION}")
        message(STATUS "pybind11: ${pybind11_VERSION}")
        message(STATUS "============================================")

        pybind11_add_module(bgp_simulator_py src/python_bindings.cpp)
        target_link_libraries(bgp_simulator_py PRIVATE as_graph)

        # Set output name to bgp_simulator (without _py suffix)
        set_target_properties(bgp_simulator_py PROPERTIES OUTPUT_NAME bgp_simulator)
    else()
        message(STATUS "pybind11 not found - Python bindings will not be built")
        message(STATUS "Install with: pip install pybind11")
    endif()
else()
    message(STATUS "Python development files not found - Python bindings will not be built")
endif()
