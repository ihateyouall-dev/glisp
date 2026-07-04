add_executable(lexer_tests lexer_tests.c)

target_link_libraries(lexer_tests PRIVATE lexer)

add_test(NAME lexer_tests COMMAND lexer_tests)
