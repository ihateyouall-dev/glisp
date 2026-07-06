add_library(lexer lexer.c)
add_library(vector INTERFACE)

target_include_directories(lexer PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(vector INTERFACE utils)
