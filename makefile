
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
OBJ_FILES := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

DEPENDS := $(patsubst %.cpp,%.d,$(SOURCES))
-include $(DEPENDS)


OUTPUT_EXE := ui.exe

# CXX := /Users/gabeochoa/homebrew/Cellar/gcc/14.2.0_1/bin/g++-14
CXX := clang++
# CXX := g++-14

.PHONY: all clean sub build run

all: build

build: $(OBJ_FILES)
	$(CXX) $(FLAGS) $(NOFLAGS) $(INCLUDES) $(LIBS) $(OBJ_FILES) -o $(OUTPUT_EXE) && say compile done

$(OBJ_DIR)/%.o: %.cpp $(H_FILES)
	@mkdir -p $(dir $@)
	$(CXX) $(FLAGS) $(NOFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

run: 
	./$(OUTPUT_EXE)

sub:
	git submodule update --init

clean:
	rm -f $(OUTPUT_EXE)

