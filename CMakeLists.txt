cmake_minimum_required(VERSION 3.6)

project(tracer VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Options
option(TRACER_ENABLE_OTF2 "Enables OTF2 trace support" ON)
option(TRACER_ENABLE_BIGSIM "Enables BigSim trace support" OFF)
option(TRACER_ENABLE_EXAMPLES "Enables building the examples" OFF)
option(TRACER_ENABLE_DOCS "Enables building the docs" OFF)
option(TRACER_ENABLE_MAIN_EXE "Enables building the main TraceR executable" ON)

if(TRACER_ENABLE_OTF2 AND TRACER_ENABLE_BIGSIM)
    message(FATAL_ERROR "OTF2 and BigSim support cannot both be enabled at this time")
elseif(NOT TRACER_ENABLE_OTF2 AND NOT TRACER_ENABLE_BIGSIM)
    message(FATAL_ERROR "One of OTF2 or BigSim support must be enabled (but not both)")
endif()

# Setting for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Main tracer executable
if(TRACER_ENABLE_MAIN_EXE)
    add_subdirectory(tracer)

    # Enable tests
    include(CTest)
    add_subdirectory(tests)
endif()

# Examples
if(TRACER_ENABLE_EXAMPLES)
    message(SEND_ERROR "Support for building examples with CMake is not currently available")
    #add_subdirectory(examples)
endif()

# Docs
if(TRACER_ENABLE_DOCS)
    add_subdirectory(docs)
endif()

