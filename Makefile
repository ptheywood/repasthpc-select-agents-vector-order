# ------------
# Load/Set System specific paths/linker options if required

# Set default values to expected variables from env if they were ommitted.
# Specify the MPI C++ compiler to use. Use an abs path it mpicxx is not on PATH
MPICXX ?= mpicxx
# The version of REPAST in use, required as REPAST HPC does not provide version-less .so's
REPASTVERSION ?= 2.3.1
# Set the path to the Repast HPC include directory, use -isystem for better warnings.
# Alternatively add it C_PLUS_INCLUDE_PATH (which uses -isystem, rather than -I)
REPAST_HPC_INCLUDE ?= # -isystem $(HOME)/sfw/repast_hpc-$(REPASTVERSION)/include
# Set the path to the repast libraries (with -L), or add it to your LIBRARY_PATH
REPAST_HPC_LIB_DIR ?= # -L$(HOME)/sfw/repast_hpc-$(REPASTVERSION)/lib
# Specify the lib's which must be linked via -l.
REPAST_HPC_LIB ?= -lrepast_hpc-${REPASTVERSION} -lrelogo-${REPASTVERSION}
# Specify the BOOST include path via -isystem, or set in C_PLUS_INCLUDE environment variable
BOOST_INCLUDE ?= # -isystem $(HOME)/sfw/Boost/Boost_1.61/include/
# Specify the BOOST library directory via -L, or set it in LIBRARY_PATH
BOOST_LIB_DIR ?= # -L$(HOME)/sfw/Boost/Boost_1.61/lib/
# Specify the boost libraries which must be linked against. the -mt suffix may not be required on some platforms.
BOOST_LIBS ?= -lboost_mpi-mt -lboost_serialization-mt -lboost_system-mt -lboost_filesystem-mt

# ------------

# Directory for the binary
BIN_DIR := ./bin
# The final binary target to produce
TARGET := ${BIN_DIR}/main

# ------------

# Set the language standards to use
CFLAGS =
CXXFLAGS = -std=c++14

# Define a variable to track flags that are common to C and CXX compilers
C_CXX_FLAGS = 

# Control optimisation level / build configuration.
C_CXX_FLAGS += -O0 -g
# ----------------------

# Enable a high level of warnings
C_CXX_FLAGS += -Wall -Wextra -Wuninitialized -Wmaybe-uninitialized
# Suppress some unwatned warnings
C_CXX_FLAGS += -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable
# Optionally promote warnigns to errors. This can make development more painful but enforces no warnings
# C_CXX_FLAGS += -Werror

# ------------

# Add include dependency generation, for simpler makefiles (with a slight compiler time increase)
C_CXX_FLAGS += -MD

# ------------

# Define commonly used directories
INC_DIR := ./include
SRC_DIR := ./src
CORE_DIR:= ../core
CORE_INC_DIR := $(CORE_DIR)/include
CORE_SRC_DIR := $(CORE_DIR)/src
OBJ_DIR := ./objects

# Build a list of .o files to compile, by finding .cpp files in known directories.
SRC_DIRS := $(SRC_DIR) $(CORE_SRC_DIR)
INC_DIRS := $(INC_DIR) $(CORE_INC_DIR)

# ------------

# Find all source files in the source directories
SRC_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(addprefix $(dir)/*,.cpp)))
# Discard .template.cpp files if any were found.
SRC_FILES := $(filter-out %.template.cpp,$(SRC_FILES))
# Discard any directories from the above, to support ../
# This would allow name collisions where core and mega_model contain source files with the same name.
SRC_FILES := $(notdir ${SRC_FILES})
# Build the list of .o files to build and link into the final binary
OBJS := $(addprefix ${OBJ_DIR}/,$(addsuffix .o,$(basename $(SRC_FILES))))

# Build the list of include flags from the listed include directories + boost and repast.
INCLUDE = $(foreach dir,$(INC_DIRS),-I$(dir)) $(BOOST_INCLUDE) $(REPAST_HPC_INCLUDE)

# From the list of objects build the list of dependency files created via -MD
DEPENDS=$(OBJS:.o=.d)

# ------------

# Variables used to control how C/CXX files are compiled, or how binaries are linked
COMPILE_C = $(MPICXX) $(CFLAGS) $(C_CXX_FLAGS) $(INCLUDE)
COMPILE_CXX = $(MPICXX) $(CXXFLAGS) $(C_CXX_FLAGS) $(INCLUDE)
LINK_CXX = $(MPICXX) $(CXXFLAGS) $(C_CXX_FLAGS) $(LDFLAGS) $(BOOST_LIB_DIR) $(REPAST_HPC_LIB_DIR)

# ------------

# Clear default .SUFFIXES rules
.SUFFIXES:

# List targets which do not directly create files with the same name as the target
.PHONY: all clean createfolders compile 

# Default / all rule
all: createfolders compile

# Rule for debugging makefile variabels, use make echo-<VAR>
echo-%: ; @echo $*=$($*)

# Clean files generated by this makefile or running the binary
clean:
	rm -f $(BIN_DIR)/*
	rm -f $(OBJ_DIR)/*.o
	rm -fr $(OBJ_DIR)
	rm -fr $(BIN_DIR)

# Create required directories
createfolders:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Generic compilation rule, depends on directories and the final binary
compile: createfolders $(TARGET)

# Include the generated .d files for rule inclusion
-include $(DEPENDS)

# Rule for compiling a C++ file into an object file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(COMPILE_CXX) -c $< -o $@

# Rule for compiling a C++ source file into an object file from the CORESRC directory.
# As both this rule and the above rule for SRC_DIR build objects into the same directory there is a name collision risk. Ideally objects should be in subdirectories as appropraite.
# If object subdirectories are implemented, this rule and the above rule can be merged into a single generic rule
$(OBJ_DIR)/%.o: $(CORE_SRC_DIR)/%.cpp
	$(COMPILE_CXX) -c $< -o $@

# Rule for linking the final executable from a list of objects.
$(TARGET): $(OBJS)
	$(LINK_CXX) $^ $(BOOST_LIBS) $(REPAST_HPC_LIB) -o $@
