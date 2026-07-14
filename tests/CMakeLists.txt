add_executable(lexer_tests lexer_tests.c)
add_executable(vector_tests vector_tests.c)
add_executable(hashmap_tests hashmap_tests.c)
add_executable(parser_tests parser_tests.c)

target_link_libraries(lexer_tests PRIVATE lexer)
target_link_libraries(vector_tests PRIVATE vector)
target_link_libraries(hashmap_tests PRIVATE hashmap)
target_link_libraries(parser_tests PRIVATE parser)

add_test(NAME lexer_tests COMMAND lexer_tests)
add_test(NAME vector_tests COMMAND vector_tests)
add_test(NAME hashmap_tests COMMAND hashmap_tests)
add_test(NAME parser_tests COMMAND parser_tests)
