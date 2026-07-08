add_library(lexer lexer.c)
add_library(vector INTERFACE)
add_library(hashmap INTERFACE)

target_include_directories(lexer PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(vector INTERFACE utils)
target_include_directories(hashmap INTERFACE utils)

target_link_libraries(lexer PUBLIC vector)
