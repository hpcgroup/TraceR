# Find the Charm++ include folder
find_path(CHARM_INCLUDE_DIR charm.h
    HINTS $ENV{CHARMPATH} ${CHARMPATH}
    PATH_SUFFIXES include
)
if(NOT CHARM_INCLUDE_DIR)
    message(FATAL_ERROR "charm include directory was not found, set CHARMPATH to the install prefix for Charm++")
endif()

# Find Charm++ libraries used
find_library(CHARM_CONV_BIGSIM_LOGS_LIBRARY NAMES conv-bigsim-logs
    HINTS $ENV{CHARMPATH} ${CHARMPATH}
    PATH_SUFFIXES lib
)
find_library(CHARM_BLUE_STANDALONE_LIBRARY NAMES blue-standalone
    HINTS $ENV{CHARMPATH} ${CHARMPATH}
    PATH_SUFFIXES lib
)
find_library(CHARM_CONV_UTIL_LIBRARY NAMES conv-util
    HINTS $ENV{CHARMPATH} ${CHARMPATH}
    PATH_SUFFIXES lib
)
list(APPEND CHARM_LIBRARIES "${CHARM_CONV_BIGSIM_LOGS_LIBRARY}")
list(APPEND CHARM_LIBRARIES "${CHARM_BLUE_STANDALONE_LIBRARY}")
list(APPEND CHARM_LIBRARIES "${CHARM_CONV_UTIL_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Charm
    REQUIRED_VARS
        CHARM_INCLUDE_DIR
        CHARM_LIBRARIES
        CHARM_CONV_BIGSIM_LOGS_LIBRARY
        CHARM_BLUE_STANDALONE_LIBRARY
        CHARM_CONV_UTIL_LIBRARY
)

if(CHARM_FOUND AND NOT TARGET CHARM::BigSim)
    add_library(CHARM::BigSim INTERFACE IMPORTED)
    # Target flags needed
    target_include_directories(CHARM::BigSim INTERFACE ${CHARM_INCLUDE_DIR})
    target_link_libraries(CHARM::BigSim INTERFACE ${CHARM_CONV_BIGSIM_LOGS_LIBRARY} ${CHARM_BLUE_STANDALONE_LIBRARY} ${CHARM_CONV_UTIL_LIBRARY})
endif()
