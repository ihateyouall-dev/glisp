add_library(lexer lexer.c)
add_library(vector INTERFACE)
add_library(hashmap INTERFACE)
add_library(ast ast.c)

target_include_directories(lexer PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(vector INTERFACE utils)
target_include_directories(hashmap INTERFACE utils)
target_include_directories(ast PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(lexer PUBLIC vector)

add_library(parser parser.c)
target_include_directories(parser PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(parser PUBLIC lexer ast)
