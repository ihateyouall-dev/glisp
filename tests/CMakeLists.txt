add_executable(lexer_tests lexer_tests.c)
add_executable(vector_tests vector_tests.c)

target_link_libraries(lexer_tests PRIVATE lexer)
target_link_libraries(vector_tests PRIVATE vector)

add_test(NAME lexer_tests COMMAND lexer_tests)
add_test(NAME vector_tests COMMAND vector_tests)
