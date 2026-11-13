cmake_minimum_required(VERSION 3.24)
project(glisp C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GLISP_ENABLE_TESTING "Build tests" OFF)

# The interpreter is a tree walker, so deep Lisp recursion is C recursion. The
# default 1 MiB stack on MinGW is far smaller than the stack the other targets
# get, which would make the depth guard in eval.c fire much later than intended.
if(MINGW)
  add_link_options(-Wl,--stack,8388608)
endif()

find_package(readline CONFIG QUIET)

if(TARGET readline::readline)
  set(GLISP_READLINE_TARGET readline::readline)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(READLINE REQUIRED IMPORTED_TARGET readline)
  pkg_check_modules(HISTORY OPTIONAL IMPORTED_TARGET history)
  set(GLISP_READLINE_TARGET PkgConfig::READLINE)
  if(TARGET PkgConfig::HISTORY)
    list(APPEND GLISP_READLINE_TARGET PkgConfig::HISTORY)
  endif()
endif()

add_executable(${PROJECT_NAME} src/main.c)

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} PRIVATE ${GLISP_READLINE_TARGET} runtime)

if(GLISP_ENABLE_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
