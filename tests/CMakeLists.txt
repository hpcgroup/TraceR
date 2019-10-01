set(TEST_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(TEST_RUN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/bin/run-example-test-with-conf.sh")

add_test(NAME stencil4d-otf-dfly
    COMMAND ${TEST_RUN_SCRIPT} "stencil4d-otf" "dfly"
)

add_test(NAME stencil4d-otf-fattree
    COMMAND ${TEST_RUN_SCRIPT} "stencil4d-otf" "fattree"
)

add_test(NAME stencil4d-otf-hypre
    COMMAND ${TEST_RUN_SCRIPT} "stencil4d-otf" "hypreX_expressMesh"
)

add_test(NAME stencil4d-otf-torus
    COMMAND ${TEST_RUN_SCRIPT} "stencil4d-otf" "torus"
)
