# THIS DOES NOT WORK AT THE MOMENT
# Score-P has a compiler wrapper that plays weird games with linking/compiling flags
# There are a pair of undefined symbols, scorep_subsystems and scorep_number_of_subsystems
# that are located in the libscorep_measurement.so and .a libraries (as shown by running nm on them).
# Somehow with the make file that doesn't have a separate linking stage,  these undefined symbols suddenly
# become defined in the compiled executable. After hours digging into it, I still haven't figured out
# what is going on; this is an issue that probably will require support from the Score-P developers,
# ideally in the form of a CMake package with macros for adding targets that are compiled using Score-P.

find_package(MPI REQUIRED)

# Find the scorep compiler wrapper
find_program(SCOREP_EXECUTABLE NAMES scorep
    HINTS $ENV{SCOREP_DIR} ${SCOREP_DIR}
    PATH_SUFFIXES bin
)

if(NOT SCOREP_EXECUTABLE)
    message(FATAL_ERROR "scorep was not found, set SCOREP_DIR to the install prefix for score-p")
endif()

# Find scorep-config
find_program(SCOREP_CONFIG_EXECUTABLE NAMES scorep-config
    HINTS $ENV{SCOREP_DIR} ${SCOREP_DIR}
    PATH_SUFFIXES bin
)

if(NOT SCOREP_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "scorep-config was not found, set SCOREP_DIR to the install prefix for score-p")
endif()

execute_process(COMMAND ${SCOREP_CONFIG_EXECUTABLE} --user --cppflags
    OUTPUT_VARIABLE SCOREP_CXX_FLAGS
)
execute_process(COMMAND ${SCOREP_CONFIG_EXECUTABLE} --user --libs
    OUTPUT_VARIABLE SCOREP_LIBRARIES
)
execute_process(COMMAND ${SCOREP_CONFIG_EXECUTABLE} --user --ldflags
    OUTPUT_VARIABLE SCOREP_LDFLAGS
)

# Leading/trailing whitespace is an error, remove it
string(STRIP ${SCOREP_CXX_FLAGS} SCOREP_CXX_FLAGS)
string(STRIP ${SCOREP_LIBRARIES} SCOREP_LIBRARIES)
# Remove "-tcollect"; unrecognized compiler option
string(REPLACE "-tcollect" "" SCOREP_LDFLAGS ${SCOREP_LDFLAGS})
string(STRIP ${SCOREP_LDFLAGS} SCOREP_LDFLAGS)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SCOREP_LDFLAGS}")

# Compile stencil4d with the scorep wrapper
list(APPEND SCOREP_LAUNCHER "${SCOREP_EXECUTABLE}" "--user" "--nocompiler" "--noopenmp" "--nopomp" "--nocuda" "--noopenacc" "--noopencl" "--nomemory")
set(CMAKE_C_COMPILER_LAUNCHER ${SCOREP_LAUNCHER})
set(CMAKE_CXX_COMPILER_LAUNCHER ${SCOREP_LAUNCHER})

# Create the stencil4d target
add_executable(stencil4d stencil4d.C)
target_compile_options(stencil4d PRIVATE ${SCOREP_CXX_FLAGS})
target_link_libraries(stencil4d ${SCOREP_LIBRARIES})
target_compile_definitions(stencil4d PRIVATE WRITE_OTF2_TRACE=1)

# With CMake 3.9, this is simplified to just target_link_libraries(traceR MPI::MPI_C)
target_include_directories(stencil4d SYSTEM PRIVATE ${MPI_C_INCLUDE_PATH})
target_link_libraries(stencil4d ${MPI_C_LIBRARIES})
target_compile_options(stencil4d PRIVATE ${MPI_C_COMPILE_FLAGS})
target_link_libraries(stencil4d ${MPI_C_LINK_FLAGS})
