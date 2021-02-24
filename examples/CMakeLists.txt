if(TRACER_ENABLE_OTF2)
    add_subdirectory(stencil4d-otf)
endif()

if(TRACER_ENABLE_BIGSIM)
    add_subdirectory(jacobi2d-bigsim)
endif()
