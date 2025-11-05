#!/bin/bash

# Environment Setup Script for MPI Research Project
# Sets up development and runtime environment

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Setting up MPI Research Project environment...${NC}"

# Check if .env file exists
if [ ! -f "$PROJECT_ROOT/.env" ]; then
    echo "Creating environment file..."
    
    cat > "$PROJECT_ROOT/.env" << EOF
# MPI Research Project Environment Variables
export PROJECT_ROOT="$PROJECT_ROOT"
export BUILD_DIR="$PROJECT_ROOT/build"
export INSTALL_DIR="$PROJECT_ROOT/install"
export RESULTS_DIR="$PROJECT_ROOT/results"
export NS3_HOME="\${NS3_HOME:-\$PROJECT_ROOT/ns-3-allinone/ns-3.37}"
export PATH="\$INSTALL_DIR/bin:\$PATH"
export LD_LIBRARY_PATH="\$INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
export PYTHONPATH="\$PROJECT_ROOT/scripts:\$PYTHONPATH"

# Python virtual environment
export VIRTUAL_ENV="$PROJECT_ROOT/venv"
export PATH="\$VIRTUAL_ENV/bin:\$PATH"

# MPI configuration
export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# Build configuration
export CC=mpicc
export CXX=mpicxx
export CFLAGS="-O3 -march=native"
export CXXFLAGS="-O3 -march=native -std=c++17"

# Optional: Set these if you have specific MPI installations
# export MPI_HOME=/usr/lib/openmpi
# export PATH=\$MPI_HOME/bin:\$PATH
# export LD_LIBRARY_PATH=\$MPI_HOME/lib:\$LD_LIBRARY_PATH
EOF
fi

# Source the environment file
source "$PROJECT_ROOT/.env"

# Create necessary directories
mkdir -p "$BUILD_DIR" "$INSTALL_DIR" "$RESULTS_DIR"

# Check if virtual environment exists
if [ ! -d "$VIRTUAL_ENV" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv "$VIRTUAL_ENV"
fi

# Activate virtual environment
source "$VIRTUAL_ENV/bin/activate"

echo -e "${GREEN}Environment setup completed!${NC}"
echo ""
echo "Project Structure:"
echo "  Project Root: $PROJECT_ROOT"
echo "  Build Directory: $BUILD_DIR"
echo "  Install Directory: $INSTALL_DIR"
echo "  Results Directory: $RESULTS_DIR"
echo ""
echo "Available Commands:"
echo "  make all              - Build entire project"
echo "  make test             - Run tests"
echo "  make bench            - Run benchmarks"
echo "  make analyze          - Run analysis"
echo "  make clean            - Clean build"
echo ""
echo "Next steps:"
echo "  1. Build project: make all"
echo "  2. Run validation: make test"
echo "  3. Run experiments: ./scripts/deployment/run_experiments.sh"
echo ""
echo "To use this environment in new terminals, run:"
echo "  source $PROJECT_ROOT/setup_environment.sh"