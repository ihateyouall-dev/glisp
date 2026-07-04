cmake_minimum_required(VERSION 3.24)
project(glisp C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLISP_ENABLE_TESTING "Build tests" OFF)

find_package(readline REQUIRED)

add_executable(${PROJECT_NAME} src/main.c)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} PRIVATE readline::readline)

if(GLISP_ENABLE_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
