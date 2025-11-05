#!/bin/bash

# NS-3 Simulation Runner for MPI Research
# Advanced script for running comprehensive network simulations

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/ns-3-dev}"
BUILD_TYPE="${BUILD_TYPE:-release}"
LOG_DIR="${SCRIPT_DIR}/../results"
CONFIG_FILE="${SCRIPT_DIR}/simulation_config.conf"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Usage information
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Run NS-3 simulations for MPI research"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help              Show this help message"
    echo "  -t, --topology TYPE     Network topology (fat_tree, torus_2d, torus_3d, dragonfly)"
    echo "  -c, --config FILE       Configuration file"
    echo "  -o, --output DIR        Output directory for results"
    echo "  -j, --jobs N            Number of parallel jobs"
    echo "  --build                 Build NS-3 before running"
    echo "  --clean                 Clean build before compiling"
    echo "  --list-topologies       List available topologies"
    echo ""
    echo "EXAMPLES:"
    echo "  $0 -t fat_tree -o results/fat_tree"
    echo "  $0 -t torus_2d --build -j 4"
    echo "  $0 -c my_config.conf"
}

# Validate NS-3 installation
validate_ns3() {
    if [ ! -d "$NS3_DIR" ]; then
        log_error "NS-3 directory not found: $NS3_DIR"
        log_error "Set NS3_DIR environment variable or install NS-3"
        exit 1
    fi

    if [ ! -f "$NS3_DIR/ns3" ]; then
        log_error "NS-3 executable not found in $NS3_DIR"
        exit 1
    fi

    log_success "NS-3 installation validated: $NS3_DIR"
}

# Build NS-3 project
build_ns3() {
    log_info "Building NS-3 with $BUILD_TYPE configuration..."

    cd "$NS3_DIR"

    if [ "$CLEAN_BUILD" = true ]; then
        log_info "Performing clean build..."
        ./ns3 clean
    fi

    if [ "$PARALLEL_JOBS" -gt 0 ]; then
        ./ns3 configure --build-profile=$BUILD_TYPE --enable-mpi --enable-examples --enable-tests
        ./ns3 build -j"$PARALLEL_JOBS"
    else
        ./ns3 configure --build-profile=$BUILD_TYPE --enable-mpi --enable-examples --enable-tests
        ./ns3 build
    fi

    log_success "NS-3 build completed"
}

# Run fat tree simulation
run_fat_tree_simulation() {
    local k_value=${1:-4}
    local duration=${2:-20}
    local output_dir="${3:-$LOG_DIR/fat_tree_k$k_value}"

    log_info "Running Fat Tree simulation with k=$k_value, duration=${duration}s"

    mkdir -p "$output_dir"

    cd "$NS3_DIR"

    local command="./waf --run \"fat_tree_scenario --k=$k_value\""
    local log_file="$output_dir/simulation.log"

    log_info "Command: $command"
    log_info "Log file: $log_file"

    # Execute simulation
    eval "$command" 2>&1 | tee "$log_file"

    # Check for results
    if [ -f "fat_tree_performance.csv" ]; then
        mv fat_tree_performance.csv "$output_dir/"
        log_success "Results saved to $output_dir/fat_tree_performance.csv"
    fi

    log_success "Fat Tree simulation completed"
}

# Run 2D torus simulation
run_torus_2d_simulation() {
    local dim_x=${1:-4}
    local dim_y=${2:-4}
    local duration=${3:-20}
    local output_dir="${4:-$LOG_DIR/torus_2d_${dim_x}x${dim_y}}"

    log_info "Running 2D Torus simulation: ${dim_x}x${dim_y}, duration=${duration}s"

    mkdir -p "$output_dir"

    cd "$NS3_DIR"

    local command="./waf --run \"torus_scenario --x=$dim_x --y=$dim_y\""
    local log_file="$output_dir/simulation.log"

    log_info "Command: $command"
    log_info "Log file: $log_file"

    eval "$command" 2>&1 | tee "$log_file"

    if [ -f "torus2d_performance.csv" ]; then
        mv torus2d_performance.csv "$output_dir/"
        log_success "Results saved to $output_dir/torus2d_performance.csv"
    fi

    log_success "2D Torus simulation completed"
}

# Run 3D torus simulation
run_torus_3d_simulation() {
    local dim_x=${1:-4}
    local dim_y=${2:-4}
    local dim_z=${3:-2}
    local duration=${4:-20}
    local output_dir="${5:-$LOG_DIR/torus_3d_${dim_x}x${dim_y}x${dim_z}}"

    log_info "Running 3D Torus simulation: ${dim_x}x${dim_y}x${dim_z}, duration=${duration}s"

    mkdir -p "$output_dir"

    cd "$NS3_DIR"

    local command="./waf --run \"torus_scenario --x=$dim_x --y=$dim_y --z=$dim_z --3d=true\""
    local log_file="$output_dir/simulation.log"

    log_info "Command: $command"
    log_info "Log file: $log_file"

    eval "$command" 2>&1 | tee "$log_file"

    if [ -f "torus3d_performance.csv" ]; then
        mv torus3d_performance.csv "$output_dir/"
        log_success "Results saved to $output_dir/torus3d_performance.csv"
    fi

    log_success "3D Torus simulation completed"
}

# Run dragonfly simulation
run_dragonfly_simulation() {
    local groups=${1:-4}
    local routers=${2:-4}
    local duration=${3:-20}
    local output_dir="${4:-$LOG_DIR/dragonfly_${groups}g_${routers}r}"

    log_info "Running Dragonfly simulation: groups=$groups, routers=$routers, duration=${duration}s"

    mkdir -p "$output_dir"

    cd "$NS3_DIR"

    # Note: Dragonfly scenario would need to be implemented
    log_warning "Dragonfly simulation not yet implemented"

    log_success "Dragonfly simulation completed"
}

# Run comprehensive topology comparison
run_topology_comparison() {
    local duration=${1:-15}
    local output_dir="${2:-$LOG_DIR/topology_comparison}"

    log_info "Running comprehensive topology comparison"

    mkdir -p "$output_dir"

    # Test different topologies with similar node counts
    run_fat_tree_simulation 4 $duration "$output_dir/fat_tree_k4"
    run_torus_2d_simulation 4 4 $duration "$output_dir/torus_4x4"
    run_torus_3d_simulation 4 2 2 $duration "$output_dir/torus_4x2x2"

    log_success "Topology comparison completed"
}

# Parse configuration file
parse_config() {
    if [ -n "$CONFIG_FILE" ] && [ -f "$CONFIG_FILE" ]; then
        log_info "Loading configuration from: $CONFIG_FILE"
        source "$CONFIG_FILE"
    fi
}

# List available topologies
list_topologies() {
    echo "Available topologies:"
    echo "  fat_tree    - K-ary fat tree topology"
    echo "  torus_2d    - 2D torus/mesh topology"
    echo "  torus_3d    - 3D torus topology"
    echo "  dragonfly   - Dragonfly topology"
    echo "  comparison  - Compare all topologies"
}

# Main execution
main() {
    # Default values
    TOPOLOGY=""
    OUTPUT_DIR="$LOG_DIR"
    PARALLEL_JOBS=0
    BUILD=false
    CLEAN_BUILD=false
    DURATION=20

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -t|--topology)
                TOPOLOGY="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -j|--jobs)
                PARALLEL_JOBS="$2"
                shift 2
                ;;
            -c|--config)
                CONFIG_FILE="$2"
                shift 2
                ;;
            --build)
                BUILD=true
                shift
                ;;
            --clean)
                CLEAN_BUILD=true
                BUILD=true
                shift
                ;;
            --list-topologies)
                list_topologies
                exit 0
                ;;
            --duration)
                DURATION="$2"
                shift 2
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Parse configuration file
    parse_config

    # Validate NS-3 installation
    validate_ns3

    # Build if requested
    if [ "$BUILD" = true ]; then
        build_ns3
    fi

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Run selected topology
    case $TOPOLOGY in
        fat_tree)
            run_fat_tree_simulation 4 $DURATION "$OUTPUT_DIR"
            ;;
        torus_2d)
            run_torus_2d_simulation 4 4 $DURATION "$OUTPUT_DIR"
            ;;
        torus_3d)
            run_torus_3d_simulation 4 2 2 $DURATION "$OUTPUT_DIR"
            ;;
        dragonfly)
            run_dragonfly_simulation 4 4 $DURATION "$OUTPUT_DIR"
            ;;
        comparison)
            run_topology_comparison $DURATION "$OUTPUT_DIR"
            ;;
        "")
            log_error "No topology specified. Use -t option or see --help"
            exit 1
            ;;
        *)
            log_error "Unknown topology: $TOPOLOGY"
            list_topologies
            exit 1
            ;;
    esac

    log_success "All simulations completed successfully"
    log_info "Results available in: $OUTPUT_DIR"
}

# Run main function
main "$@"