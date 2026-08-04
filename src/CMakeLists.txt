add_library(lexer lexer.c)
add_library(vector INTERFACE)
add_library(hashmap INTERFACE)
add_library(ast ast.c)
add_library(parser parser.c)

file(GLOB runtime_src "runtime/*.c")

add_library(runtime ${runtime_src})

target_include_directories(lexer PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(vector INTERFACE utils)
target_include_directories(hashmap INTERFACE utils)
target_include_directories(ast PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(parser PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(runtime PUBLIC ${PROJECT_SOURCE_DIR}/src/runtime)

target_link_libraries(lexer PUBLIC vector)
target_link_libraries(parser PUBLIC lexer ast vector)
target_link_libraries(runtime PUBLIC parser ast)
