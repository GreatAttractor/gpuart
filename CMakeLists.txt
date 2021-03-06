project("gpuart")

cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE Release)

#------------- NanoGUI-specific ---------------------------

# Disable building extras we won't need (pure C++ project)
set(NANOGUI_BUILD_EXAMPLE OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_PYTHON  OFF CACHE BOOL " " FORCE)
set(NANOGUI_INSTALL       OFF CACHE BOOL " " FORCE)

# Add the configurations from nanogui
add_subdirectory(ext/nanogui)

include_directories(ext/nanogui/include)

# For reliability of parallel build, make the NanoGUI targets dependencies
set_property(TARGET glfw glfw_objects nanogui PROPERTY FOLDER "dependencies")

# Various preprocessor definitions have been generated by NanoGUI
add_definitions(${NANOGUI_EXTRA_DEFS})

# On top of adding the path to nanogui/include, you may need extras
include_directories(${NANOGUI_EXTRA_INCS})


#----------------------------------------------------------

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
  
  # Multiple occurrences in Eigen (used by NanoGUI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
  
  # Harmless casts when storing uints as floats during BVH compilation
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing")
endif()

add_executable(gpuart
    src/bvh.cpp
    src/core.cpp
    src/gl_utils.cpp
    src/main.cpp
    src/renderer.cpp
    src/scenes.cpp
    src/utils.cpp
)

set_property(TARGET gpuart PROPERTY CXX_STANDARD 11)
set_property(TARGET gpuart PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(gpuart nanogui ${NANOGUI_EXTRA_LIBS})
