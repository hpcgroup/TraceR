# Note: For finding the TraceR dependencies like CODES, ROSS, OTF2 installed in custom locations
# Set CMAKE_PREFIX_PATH to a ;-separated list of install prefix paths (folder with bin/lib subfolders)
# May be easier than setting individual paths

# Dependencies
find_package(Threads REQUIRED)
find_package(MPI REQUIRED)
find_package(ROSS REQUIRED)

# Find CODES using PkgConfig, because it doesn't use CMake (yet)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CODES REQUIRED IMPORTED_TARGET codes)

if(TRACER_ENABLE_OTF2)
    find_package(OTF2)
endif()

if(TRACER_ENABLE_BIGSIM)
    find_package(BigSim)
endif()

# traceR option
option(TRACER_DISABLE_COMM_BUILD "Sets NO_COMM_BUILD=1" OFF)

# traceR source files
set(TRACER_SRCS
    coll-events.C
    p2p-events.C
    tracer-driver.C
    elements/MsgEntry.C
    elements/PE.C
    elements/Task.C
    reader/CWrapper.C
    reader/TraceReader.C
    reader/otf2_reader.C
)

# traceR executable
add_executable(traceR ${TRACER_SRCS})
target_include_directories(traceR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(traceR Threads::Threads)

target_include_directories(traceR PRIVATE ${ROSS_INCLUDE_DIRS})
target_link_libraries(traceR ROSS)

target_link_libraries(traceR PkgConfig::CODES)

if(TRACER_ENABLE_OTF2)
    # Link traceR target with OTF2 library
    target_compile_definitions(traceR PRIVATE TRACER_OTF_TRACES=1)
    target_link_libraries(traceR OTF2::OTF2)
endif()

if(TRACER_ENABLE_BIGSIM)
    # Link traceR with BigSim library
    target_compile_definitions(traceR PRIVATE TRACER_BIGSIM_TRACES=1)
    target_link_libraries(traceR CHARM::BigSim)
    # Set the standard to use C++11 (needed for Charm++)
    set_property(TARGET traceR PROPERTY CXX_STANDARD 11)
    set_property(TARGET traceR PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

# With CMake 3.9, this is simplified to just target_link_libraries(traceR MPI::MPI_C)
target_include_directories(traceR SYSTEM PRIVATE ${MPI_C_INCLUDE_PATH})
target_link_libraries(traceR ${MPI_C_LIBRARIES})
target_compile_options(traceR PRIVATE ${MPI_C_COMPILE_FLAGS})
target_link_libraries(traceR ${MPI_C_LINK_FLAGS})

if(TRACER_DISABLE_COMM_BUILD)
    target_compile_definitions(traceR PRIVATE NO_COMM_BUILD=1)
endif()
