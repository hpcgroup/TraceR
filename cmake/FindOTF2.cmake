# Find the otf2-config program and run it to get paths and flags for OTF2
find_program(OTF2_CONFIG_EXECUTABLE NAMES otf2-config
    HINTS $ENV{OTF2_DIR} $ENV{SCOREP_DIR} ${SCOREP_DIR} ${OTF2_DIR}
    PATH_SUFFIXES bin
)

if(NOT OTF2_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "otf2-config was not found, set OTF2_DIR to the install prefix for otf2/score-p")
endif()

# Get compile/link flags for otf2
execute_process(COMMAND ${OTF2_CONFIG_EXECUTABLE} --libs
    OUTPUT_VARIABLE OTF2_LIBRARIES
)
execute_process(COMMAND ${OTF2_CONFIG_EXECUTABLE} --cflags
    OUTPUT_VARIABLE OTF2_COMPILE_FLAGS
)
execute_process(COMMAND ${OTF2_CONFIG_EXECUTABLE} --ldflags
    OUTPUT_VARIABLE OTF2_LINK_FLAGS
)

# Leading/trailing whitespace is an error, remove it
string(STRIP ${OTF2_LIBRARIES} OTF2_LIBRARIES)
string(STRIP ${OTF2_COMPILE_FLAGS} OTF2_COMPILE_FLAGS)
string(STRIP ${OTF2_LINK_FLAGS} OTF2_LINK_FLAGS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OTF2
    REQUIRED_VARS OTF2_CONFIG_EXECUTABLE OTF2_LIBRARIES OTF2_COMPILE_FLAGS OTF2_LINK_FLAGS
)

if(OTF2_FOUND AND NOT TARGET OTF2::OTF2)
    add_library(OTF2::OTF2 INTERFACE IMPORTED)
    target_compile_options(OTF2::OTF2 INTERFACE ${OTF2_COMPILE_FLAGS})
    target_link_libraries(OTF2::OTF2 INTERFACE ${OTF2_LIBRARIES} ${OTF2_LINK_FLAGS})
endif()
