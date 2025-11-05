#!/bin/bash

# Advanced Experiment Runner for MPI Research
# Orchestrates comprehensive experiments and data collection

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RESULTS_DIR="$PROJECT_ROOT/results"
EXPERIMENT_LOG="$RESULTS_DIR/experiment.log"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$EXPERIMENT_LOG"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$EXPERIMENT_LOG"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$EXPERIMENT_LOG"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$EXPERIMENT_LOG"
}

# Load environment
load_environment() {
    if [ -f "$PROJECT_ROOT/.env" ]; then
        source "$PROJECT_ROOT/.env"
    else
        log_error "Environment file not found. Run install_dependencies.sh first."
        exit 1
    fi
}

# Check required tools
check_requirements() {
    log_info "Checking system requirements..."
    
    local missing_tools=()
    
    for tool in mpirun python3 make; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        exit 1
    fi
    
    log_success "All requirements satisfied"
}

# Build project
build_project() {
    log_info "Building project..."
    
    cd "$PROJECT_ROOT"
    
    if make all -j$(nproc); then
        log_success "Project built successfully"
    else
        log_error "Build failed"
        exit 1
    fi
}

# Run micro-benchmarks
run_micro_benchmarks() {
    local output_dir="$RESULTS_DIR/micro_benchmarks_$TIMESTAMP"
    mkdir -p "$output_dir"
    
    log_info "Running micro-benchmarks..."
    
    local benchmarks=(
        "broadcast_benchmark"
        "collective_benchmark" 
        "reduce_benchmark"
    )
    
    for benchmark in "${benchmarks[@]}"; do
        local benchmark_path="$BUILD_DIR/benchmarks/micro/$benchmark"
        
        if [ -f "$benchmark_path" ]; then
            log_info "Running $benchmark..."
            
            # Run with different process counts
            for processes in 2 4 8 16; do
                if [ $processes -le $(nproc) ]; then
                    local output_file="$output_dir/${benchmark}_${processes}procs.csv"
                    mpirun -np $processes "$benchmark_path" > "$output_file" 2>&1
                    log_success "Completed $benchmark with $processes processes"
                fi
            done
        else
            log_warning "Benchmark not found: $benchmark_path"
        fi
    done
    
    log_success "Micro-benchmarks completed"
}

# Run scalability benchmarks
run_scalability_benchmarks() {
    local output_dir="$RESULTS_DIR/scalability_$TIMESTAMP"
    mkdir -p "$output_dir"
    
    log_info "Running scalability benchmarks..."
    
    local scalability_tests=(
        "scalability_analysis"
        "weak_strong_scaling"
    )
    
    for test in "${scalability_tests[@]}"; do
        local test_path="$BUILD_DIR/benchmarks/scalability/$test"
        
        if [ -f "$test_path" ]; then
            log_info "Running $test..."
            
            # Use available cores, up to 32
            local max_processes=$(( $(nproc) > 32 ? 32 : $(nproc) ))
            mpirun -np $max_processes "$test_path" > "$output_dir/${test}.csv" 2>&1
            
            log_success "Completed $test with $max_processes processes"
        else
            log_warning "Scalability test not found: $test_path"
        fi
    done
    
    log_success "Scalability benchmarks completed"
}

# Run validation tests
run_validation_tests() {
    local output_dir="$RESULTS_DIR/validation_$TIMESTAMP"
    mkdir -p "$output_dir"
    
    log_info "Running validation tests..."
    
    local validation_tests=(
        "correctness_tests"
        "unit_tests"
    )
    
    local all_passed=true
    
    for test in "${validation_tests[@]}"; do
        local test_path="$BUILD_DIR/benchmarks/validation/$test"
        
        if [ -f "$test_path" ]; then
            log_info "Running $test..."
            
            if mpirun -np 4 "$test_path" > "$output_dir/${test}.log" 2>&1; then
                log_success "$test passed"
            else
                log_error "$test failed"
                all_passed=false
            fi
        else
            log_warning "Validation test not found: $test_path"
        fi
    done
    
    if [ "$all_passed" = true ]; then
        log_success "All validation tests passed"
    else
        log_error "Some validation tests failed"
        exit 1
    fi
}

# Run NS-3 simulations
run_ns3_simulations() {
    if [ -z "$NS3_HOME" ] || [ ! -d "$NS3_HOME" ]; then
        log_warning "NS3_HOME not set or NS-3 not installed, skipping simulations"
        return 0
    fi
    
    local output_dir="$RESULTS_DIR/ns3_simulations_$TIMESTAMP"
    mkdir -p "$output_dir"
    
    log_info "Running NS-3 simulations..."
    
    local topologies=("fat_tree" "torus_2d" "torus_3d")
    
    for topology in "${topologies[@]}"; do
        log_info "Running $topology simulation..."
        
        # Run simulation
        cd "$NS3_HOME"
        ./waf --run "mpi_research_$topology" > "$output_dir/${topology}.log" 2>&1
        
        # Copy results
        if [ -f "mpi_performance.csv" ]; then
            cp "mpi_performance.csv" "$output_dir/${topology}_performance.csv"
            log_success "$topology simulation completed"
        else
            log_warning "No results file for $topology simulation"
        fi
    done
    
    log_success "NS-3 simulations completed"
}

# Run comprehensive analysis
run_analysis() {
    local output_dir="$RESULTS_DIR/analysis_$TIMESTAMP"
    mkdir -p "$output_dir"
    
    log_info "Running comprehensive analysis..."
    
    # Run analysis scripts
    local analysis_scripts=(
        "performance_plots.py"
        "scalability_analysis.py"
        "statistical_analysis.py"
    )
    
    for script in "${analysis_scripts[@]}"; do
        local script_path="$PROJECT_ROOT/scripts/analysis/$script"
        
        if [ -f "$script_path" ]; then
            log_info "Running $script..."
            
            # Activate virtual environment
            source "$PROJECT_ROOT/venv/bin/activate"
            
            if python3 "$script_path" "$RESULTS_DIR" --output "$output_dir" >> "$EXPERIMENT_LOG" 2>&1; then
                log_success "$script completed"
            else
                log_error "$script failed"
            fi
        else
            log_warning "Analysis script not found: $script_path"
        fi
    done
    
    # Generate report
    generate_report "$output_dir"
    
    log_success "Comprehensive analysis completed"
}

# Generate experiment report
generate_report() {
    local output_dir="$1"
    
    log_info "Generating experiment report..."
    
    local report_file="$output_dir/experiment_report.md"
    
    cat > "$report_file" << EOF
# MPI Research Experiment Report
**Generated:** $(date)  
**Experiment ID:** $TIMESTAMP  
**Project Version:** $(git describe --tags 2>/dev/null || echo "unknown")

## Experiment Summary

### Benchmarks Executed
- Micro-benchmarks: broadcast, collective operations, reduce
- Scalability analysis: strong and weak scaling  
- Validation tests: correctness and unit tests
- NS-3 simulations: fat tree, 2D torus, 3D torus

### Results Location
- Raw data: \`$RESULTS_DIR/*_$TIMESTAMP/\`
- Analysis output: \`$output_dir/\`
- Log file: \`$EXPERIMENT_LOG\`

## Key Findings

*(This section would be populated with actual analysis results)*

### Performance Characteristics
- Topology performance comparison
- Scaling efficiency analysis
- Statistical significance of results

### Recommendations
- Optimal topology selections
- Parameter tuning suggestions
- Further research directions

## Technical Details

### System Configuration
- CPU: $(lscpu | grep "Model name" | cut -d: -f2 | xargs)
- Cores: $(nproc)
- Memory: $(free -h | grep Mem | awk '{print $2}')
- MPI: $(mpirun --version | head -n1)

### Software Versions
- Python: $(python3 --version 2>/dev/null || echo "not available")
- GCC: $(gcc --version | head -n1)
- OpenMPI: $(mpirun --version | head -n1)

## Next Steps

1. Review generated plots and analysis in \`$output_dir/\`
2. Examine statistical significance results
3. Validate findings with additional experiments
4. Update models and algorithms based on insights

---
*Report generated automatically by MPI Research Experiment Runner*
EOF
    
    log_success "Experiment report generated: $report_file"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    
    # Remove temporary files
    find "$RESULTS_DIR" -name "*.tmp" -delete
    
    log_success "Cleanup completed"
}

# Display usage information
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Run comprehensive MPI research experiments"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help              Show this help message"
    echo "  --skip-build            Skip building project"
    echo "  --skip-micro            Skip micro-benchmarks"
    echo "  --skip-scalability      Skip scalability benchmarks"
    echo "  --skip-validation       Skip validation tests"
    echo "  --skip-ns3              Skip NS-3 simulations"
    echo "  --skip-analysis         Skip analysis phase"
    echo "  --phases PHASES         Run specific phases (comma-separated)"
    echo "  --quick                 Run quick test suite"
    echo "  --comprehensive         Run comprehensive experiments (default)"
    echo ""
    echo "PHASES: build,micro,scalability,validation,ns3,analysis,report"
    echo ""
    echo "EXAMPLES:"
    echo "  $0 --quick              Run quick validation and basic benchmarks"
    echo "  $0 --skip-ns3           Run everything except NS-3 simulations"
    echo "  $0 --phases=micro,analysis  Run only micro-benchmarks and analysis"
}

# Main experiment function
main() {
    # Parse command line arguments
    local SKIP_BUILD=false
    local SKIP_MICRO=false
    local SKIP_SCALABILITY=false
    local SKIP_VALIDATION=false
    local SKIP_NS3=false
    local SKIP_ANALYSIS=false
    local EXPERIMENT_MODE="comprehensive"
    local CUSTOM_PHASES=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --skip-micro)
                SKIP_MICRO=true
                shift
                ;;
            --skip-scalability)
                SKIP_SCALABILITY=true
                shift
                ;;
            --skip-validation)
                SKIP_VALIDATION=true
                shift
                ;;
            --skip-ns3)
                SKIP_NS3=true
                shift
                ;;
            --skip-analysis)
                SKIP_ANALYSIS=true
                shift
                ;;
            --quick)
                EXPERIMENT_MODE="quick"
                shift
                ;;
            --comprehensive)
                EXPERIMENT_MODE="comprehensive"
                shift
                ;;
            --phases)
                CUSTOM_PHASES="$2"
                shift 2
                ;;
            --phases=*)
                CUSTOM_PHASES="${1#*=}"
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    # Initialize
    load_environment
    check_requirements
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    
    # Start log
    echo "MPI Research Experiment Log" > "$EXPERIMENT_LOG"
    echo "Started: $(date)" >> "$EXPERIMENT_LOG"
    echo "Experiment ID: $TIMESTAMP" >> "$EXPERIMENT_LOG"
    echo "Mode: $EXPERIMENT_MODE" >> "$EXPERIMENT_LOG"
    echo "======================================" >> "$EXPERIMENT_LOG"
    
    log_info "Starting MPI Research Experiments"
    log_info "Experiment ID: $TIMESTAMP"
    log_info "Mode: $EXPERIMENT_MODE"
    
    # Handle custom phases
    if [ -n "$CUSTOM_PHASES" ]; then
        IFS=',' read -ra PHASES <<< "$CUSTOM_PHASES"
        for phase in "${PHASES[@]}"; do
            case $phase in
                build) build_project ;;
                micro) run_micro_benchmarks ;;
                scalability) run_scalability_benchmarks ;;
                validation) run_validation_tests ;;
                ns3) run_ns3_simulations ;;
                analysis) run_analysis ;;
                report) generate_report "$RESULTS_DIR/report_$TIMESTAMP" ;;
                *) log_warning "Unknown phase: $phase" ;;
            esac
        done
        exit 0
    fi
    
    # Quick mode
    if [ "$EXPERIMENT_MODE" = "quick" ]; then
        log_info "Running in quick mode..."
        
        if [ "$SKIP_BUILD" = false ]; then
            build_project
        fi
        
        run_validation_tests
        run_micro_benchmarks
        
        exit 0
    fi
    
    # Comprehensive mode (default)
    log_info "Running in comprehensive mode..."
    
    # Build project
    if [ "$SKIP_BUILD" = false ]; then
        build_project
    fi
    
    # Run validation
    if [ "$SKIP_VALIDATION" = false ]; then
        run_validation_tests
    fi
    
    # Run benchmarks
    if [ "$SKIP_MICRO" = false ]; then
        run_micro_benchmarks
    fi
    
    if [ "$SKIP_SCALABILITY" = false ]; then
        run_scalability_benchmarks
    fi
    
    # Run simulations
    if [ "$SKIP_NS3" = false ]; then
        run_ns3_simulations
    fi
    
    # Run analysis
    if [ "$SKIP_ANALYSIS" = false ]; then
        run_analysis
    fi
    
    # Cleanup
    cleanup
    
    log_success "All experiments completed successfully!"
    log_info "Results available in: $RESULTS_DIR"
    log_info "Experiment ID: $TIMESTAMP"
    log_info "Log file: $EXPERIMENT_LOG"
}

# Run main function
main "$@"