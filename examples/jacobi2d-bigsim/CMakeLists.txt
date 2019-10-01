# This likely won't work since it is trying to switch the compiler,
# which is an area that would be useful to have Charm support CMake
# similar to the support Qt support in CMake where the wrapper/generator
# programs are run automatically.

find_program(AMPICC_EXECUTABLE NAMES ampiCC
    HINTS $ENV{CHARMPATH} ${CHARMPATH}
    PATH_SUFFIXES bin
)
if(NOT AMPICC_EXECUTABLE)
    message(FATAL_ERROR "ampiCC was not found, set CHARMPATH to the install prefix for Charm++")
endif()

set(CMAKE_CXX_COMPILER ${AMPICC_EXECUTABLE})
set(CMAKE_C_COMPILER ${AMPICC_EXECUTABLE})

add_executable(jacobi2d jacobi2d.C)
