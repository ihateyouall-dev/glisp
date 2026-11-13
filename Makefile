.PHONY: all build clean test configure install-deps install package

CMAKE_PRESET ?= release
CMAKE_CONFIGURE_PRESET ?= $(CMAKE_PRESET)
CONAN_BUILD_TYPE ?= Release
BUILD_DIR ?= build
BUILD_TYPE ?= Release
USE_CONAN ?= 1
GLISP_ENABLE_TESTING ?= OFF
CMAKE_ARGS ?=

.ONESHELL:

ifeq ($(BUILD_TYPE),Debug)
CMAKE_PRESET := debug
CONAN_BUILD_TYPE := Debug
else ifeq ($(BUILD_TYPE),Test)
CMAKE_PRESET := test
CONAN_BUILD_TYPE := Debug
GLISP_ENABLE_TESTING := ON
else ifeq ($(BUILD_TYPE),Leaks)
CMAKE_PRESET := leaks
CONAN_BUILD_TYPE := Debug
GLISP_ENABLE_TESTING := ON
endif

# On Windows, Conan generates 'conan-default' configure preset instead of 'conan-release'
ifeq ($(OS),Windows_NT)
ifeq ($(CMAKE_CONFIGURE_PRESET),conan-release)
CMAKE_CONFIGURE_PRESET := conan-default
endif
endif

all: build

ifeq ($(USE_CONAN),1)
configure: CMakeLists.txt install-deps
	cmake --preset $(CMAKE_CONFIGURE_PRESET)
else
configure: CMakeLists.txt
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONAN_BUILD_TYPE) \
		-DGLISP_ENABLE_TESTING=$(GLISP_ENABLE_TESTING) $(CMAKE_ARGS)
endif

build: configure
ifeq ($(USE_CONAN),1)
	cmake --build --preset $(CMAKE_PRESET)
else
	cmake --build $(BUILD_DIR)
endif

install-deps: conanfile.txt $(HOME)/.conan2/profiles/default
	conan install . --output-folder=$(BUILD_DIR) --build=missing \
		-s compiler.cstd=17 -s build_type=$(CONAN_BUILD_TYPE)

$(HOME)/.conan2/profiles/default:
	conan profile detect --force

test:
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install:
	cmake --install $(BUILD_DIR)

package: build
	cd $(BUILD_DIR)
	cpack

clean:
	rm -rf $(BUILD_DIR)
