find_program(SPHINX_EXECUTABLE NAMES sphinx-build
    HINTS $ENV{SPHINX_DIR} ${SPHINX_DIR}
    PATH_SUFFIXES bin
)

if(NOT SPHINX_EXECUTABLE)
    message(FATAL_ERROR "sphinx-build was not found, set SPHINX_DIR to the install prefix for sphinx")
endif()

add_custom_target(tracer_docs ALL
    ${SPHINX_EXECUTABLE}
        -M
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building TraceR documentation with Sphinx"
)
