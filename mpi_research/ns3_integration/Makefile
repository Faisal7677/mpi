# ns3_integration/Makefile - NS-3 Integration Build

# Configuration
NS3_HOME ?= $(HOME)/wangyl/ns-allinone-3.37
BUILD_TYPE ?= release
INSTALL_DIR ?= ../install
BUILD_DIR = build

# NS-3 configuration
NS3_WAF = $(NS3_HOME)/waf
NS3_BUILD_PROFILE = $(BUILD_TYPE)

# Source files
MODEL_SRCS = $(wildcard models/*.cc)
SCENARIO_SRCS = $(wildcard scenarios/*.cc)
SCRIPTS = $(wildcard scripts/*)

# Check NS-3 installation
CHECK_NS3 = $(shell test -d $(NS3_HOME) && test -f $(NS3_WAF) && echo "found")

# Main target
all: check-ns3 build

# Check NS-3 installation
check-ns3:
ifneq ($(CHECK_NS3), found)
	   $(error NS-3 not found at $(NS3_HOME). Please set NS3_HOME environment variable)
endif
	   @echo "NS-3 found: $(NS3_HOME)"

# Copy source files to NS-3
setup: check-ns3
	   @echo "Setting up NS-3 integration..."
	   @mkdir -p $(NS3_HOME)/scratch/mpi-research
	   @cp $(MODEL_SRCS) $(SCENARIO_SRCS) $(NS3_HOME)/scratch/mpi-research/

	   # Create module directory if it doesn't exist
	   @mkdir -p $(NS3_HOME)/src/mpi-research
	   @cp models/*.h $(NS3_HOME)/src/mpi-research/ 2>/dev/null || true

	   @echo "Source files copied to NS-3"

# Configure NS-3 build
configure: setup
	   @echo "Configuring NS-3 build..."
	   cd $(NS3_HOME) && ./waf configure --build-profile=$(NS3_BUILD_PROFILE)
	   @echo "NS-3 configured for $(NS3_BUILD_PROFILE) build"

# Build NS-3 projects
build: configure
	   @echo "Building NS-3 integration..."
	   cd $(NS3_HOME) && ./waf build
	   @echo "NS-3 build completed"

# Build specific scenarios
fat_tree: build
	   cd $(NS3_HOME) && ./waf --run fat_tree_scenario

torus_2d: build
	   cd $(NS3_HOME) && ./waf --run torus_scenario --command-template="%s --x=4 --y=4"

torus_3d: build
	   cd $(NS3_HOME) && ./waf --run torus_scenario --command-template="%s --x=4 --y=4 --z=2 --3d=true"

# Run simulations
simulate: build
	   @echo "Running NS-3 simulations..."
	   ./scripts/run_ns3_simulations.sh -t fat_tree -o ../results/ns3_simulations/fat_tree
	   ./scripts/run_ns3_simulations.sh -t torus_2d -o ../results/ns3_simulations/torus_2d
	   @echo "Simulations completed"

# Install scripts
install: 
	   @echo "Installing NS-3 integration scripts..."
	   @mkdir -p $(INSTALL_DIR)/bin
	   @cp scripts/run_ns3_simulations.sh scripts/analyze_ns3_results.py $(INSTALL_DIR)/bin/
	   @chmod +x $(INSTALL_DIR)/bin/*.sh
	   @echo "Scripts installed to $(INSTALL_DIR)/bin"

# Clean NS-3 build
clean:
	   @echo "Cleaning NS-3 integration..."
	   cd $(NS3_HOME) && ./waf clean 2>/dev/null || true
	   rm -rf $(BUILD_DIR)
	   @echo "NS-3 integration cleaned"

# Deep clean
distclean: clean
	   @echo "Deep cleaning NS-3 integration..."
	   rm -rf $(NS3_HOME)/scratch/mpi-research
	   rm -rf $(NS3_HOME)/src/mpi-research 2>/dev/null || true
	   @echo "NS-3 integration distclean completed"

.PHONY: all check-ns3 setup configure build fat_tree torus_2d torus_3d simulate install clean distclean