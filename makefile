
RAYLIB_FLAGS := `pkg-config --cflags raylib`
RAYLIB_LIB := `pkg-config --libs raylib`

RELEASE_FLAGS = -std=c++2c $(RAYLIB_FLAGS)

FLAGS = -std=c++2c -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wconversion -g $(RAYLIB_FLAGS)

NOFLAGS = -Wno-deprecated-volatile -Wno-missing-field-initializers \
		  -Wno-c99-extensions -Wno-unused-function -Wno-sign-conversion \
		  -Wno-implicit-int-float-conversion -Wno-implicit-float-conversion -Werror
INCLUDES = -Ivendor/ -Isrc/ 
LIBS = -L. -Lvendor/ $(RAYLIB_LIB)

SRC_FILES := $(wildcard src/*.cpp src/**/*.cpp)
H_FILES := $(wildcard src/**/*.h src/**/*.hpp)
OBJ_DIR := ./output
OBJ_FILES := $(SRC_FILES:%.cpp=$(OBJ_DIR)/%.o)

DEPENDS := $(patsubst %.cpp,%.d,$(SOURCES))
-include $(DEPENDS)


OUTPUT_EXE := ui.exe

# CXX := /Users/gabeochoa/homebrew/Cellar/gcc/14.2.0_1/bin/g++-14
CXX := clang++
# CXX := g++-14

.PHONY: all clean sub build run

all: build

build:
	$(CXX) $(FLAGS) $(NOFLAGS) $(INCLUDES) $(LIBS) src/main.cpp -o $(OUTPUT_EXE)

run: build
	./$(OUTPUT_EXE)

sub:
	git submodule update --init

clean:
	rm -f $(OUTPUT_EXE)

