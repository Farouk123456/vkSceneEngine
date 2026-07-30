CXX      := clang++
CXXFLAGS := -std=gnu++23 -g -fopenmp -O3
LDFLAGS  := -ldl -pthread

PKG_CFLAGS := $(shell pkg-config --cflags glfw3 vulkan freetype2 tbb plutosvg raqm )
PKG_LIBS   := $(shell pkg-config --libs  glfw3 vulkan freetype2 tbb plutosvg raqm )

SRC_DIR     := src
SHADER_DIR  := shaders

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)

VERT_SHADERS := $(wildcard $(SHADER_DIR)/*.vert)
FRAG_SHADERS := $(wildcard $(SHADER_DIR)/*.frag)
COMP_SHADERS := $(wildcard $(SHADER_DIR)/*.comp)

VERT_SPV := $(VERT_SHADERS:.vert=_vert.spv)
FRAG_SPV := $(FRAG_SHADERS:.frag=_frag.spv)
COMP_SPV := $(COMP_SHADERS:.comp=_comp.spv)

BIN := Solver

.PHONY: all clean shaders FORCE

all: FORCE shaders $(BIN)

# --------------------
# Shader compilation
# --------------------

shaders: FORCE $(VERT_SPV) $(FRAG_SPV) $(COMP_SPV)

%_vert.spv: %.vert FORCE
	glslc --target-env=vulkan1.3 $< -o $@ 

%_frag.spv: %.frag FORCE
	glslc --target-env=vulkan1.3 $< -o $@

%_comp.spv: %.comp FORCE
	glslc --target-env=vulkan1.3 $< -o $@

# --------------------
# C++ compilation
# --------------------

$(BIN): $(OBJS)
	$(CXX) $^ -o $@ $(PKG_LIBS) $(LDFLAGS)
	rm -f $(OBJS)

%.o: %.cpp FORCE
	$(CXX) $(CXXFLAGS) $(PKG_CFLAGS) -c $< -o $@

# --------------------
# Cleanup
# --------------------

clean:
	rm -f $(OBJS) $(BIN) $(VERT_SPV) $(FRAG_SPV)
