#include "performance_measurement.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <thread>

namespace TopologyAwareResearch {

    PerformanceTimer::PerformanceTimer() {
        reset();
    }

    PerformanceTimer::~PerformanceTimer() {}

    void PerformanceTimer::start(const std::string& section_name) {
        current_section_ = section_name;
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void PerformanceTimer::stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_);

        timings_[current_section_] += duration.count() / 1000.0; // Convert to milliseconds
        current_section_.clear();
    }

    void PerformanceTimer::reset() {
        timings_.clear();
        current_section_.clear();
    }

    double PerformanceTimer::get_elapsed_time(const std::string& section_name) const {
        auto it = timings_.find(section_name);
        if (it != timings_.end()) {
            return it->second;
        }
        return 0.0;
    }

    std::map<std::string, double> PerformanceTimer::get_all_timings() const {
        return timings_;
    }

    void PerformanceTimer::print_timings() const {
        std::cout << "=== Performance Timings ===" << std::endl;
        for (const auto& timing : timings_) {
            std::cout << timing.first << ": " << timing.second << " ms" << std::endl;
        }
        std::cout << "===========================" << std::endl;
    }

    double PerformanceTimer::get_current_time() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
    }

    void PerformanceTimer::busy_wait_us(int microseconds) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + std::chrono::microseconds(microseconds);

        while (std::chrono::high_resolution_clock::now() < end) {
            // Busy wait
            std::this_thread::yield();
        }
    }

    // BandwidthMeasurer implementation
    BandwidthMeasurer::BandwidthMeasurer(MPI_Comm comm)
        : comm_(comm), iterations_(10), warmup_iterations_(5) {
    }

    BandwidthMeasurer::~BandwidthMeasurer() {}

    double BandwidthMeasurer::measure_point_to_point_bandwidth(int src_rank, int dst_rank,
        int message_size, int iterations) {
        int world_rank;
        MPI_Comm_rank(comm_, &world_rank);

        if (world_rank != src_rank && world_rank != dst_rank) {
            return 0.0;
        }

        std::vector<char> buffer(message_size);
        double total_bandwidth = 0.0;

        // Warmup
        warmup(src_rank, dst_rank, message_size);

        for (int i = 0; i < iterations; ++i) {
            MPI_Barrier(comm_);

            auto start_time = MPI_Wtime();

            if (world_rank == src_rank) {
                MPI_Send(buffer.data(), message_size, MPI_BYTE, dst_rank, 0, comm_);
            }
            else if (world_rank == dst_rank) {
                MPI_Recv(buffer.data(), message_size, MPI_BYTE, src_rank, 0, comm_, MPI_STATUS_IGNORE);
            }

            auto end_time = MPI_Wtime();

            if (world_rank == src_rank || world_rank == dst_rank) {
                double elapsed = end_time - start_time;
                double bandwidth = (message_size * 8.0) / (elapsed * 1e6); // Mbps
                total_bandwidth += bandwidth;
            }
        }

        return total_bandwidth / iterations;
    }

    std::vector<std::vector<double>> BandwidthMeasurer::measure_all_to_all_bandwidth(int message_size) {
        int world_size, world_rank;
        MPI_Comm_size(comm_, &world_size);
        MPI_Comm_rank(comm_, &world_rank);

        std::vector<std::vector<double>> bandwidth_matrix(world_size,
            std::vector<double>(world_size, 0.0));

        // Measure bandwidth between all pairs
        for (int src = 0; src < world_size; ++src) {
            for (int dst = src + 1; dst < world_size; ++dst) {
                double bandwidth = measure_point_to_point_bandwidth(src, dst, message_size, iterations_);
                bandwidth_matrix[src][dst] = bandwidth;
                bandwidth_matrix[dst][src] = bandwidth;
            }
        }

        return bandwidth_matrix;
    }

    void BandwidthMeasurer::warmup(int src_rank, int dst_rank, int message_size) {
        int world_rank;
        MPI_Comm_rank(comm_, &world_rank);

        std::vector<char> buffer(message_size);

        for (int i = 0; i < warmup_iterations_; ++i) {
            if (world_rank == src_rank) {
                MPI_Send(buffer.data(), message_size, MPI_BYTE, dst_rank, 0, comm_);
            }
            else if (world_rank == dst_rank) {
                MPI_Recv(buffer.data(), message_size, MPI_BYTE, src_rank, 0, comm_, MPI_STATUS_IGNORE);
            }
        }
    }

    // LatencyMeasurer implementation
    LatencyMeasurer::LatencyMeasurer(MPI_Comm comm)
        : comm_(comm), iterations_(1000), warmup_iterations_(100) {
    }

    LatencyMeasurer::~LatencyMeasurer() {}

    double LatencyMeasurer::measure_point_to_point_latency(int src_rank, int dst_rank, int iterations) {
        int world_rank;
        MPI_Comm_rank(comm_, &world_rank);

        if (world_rank != src_rank && world_rank != dst_rank) {
            return 0.0;
        }

        return ping_pong_latency(src_rank, dst_rank, iterations);
    }

    double LatencyMeasurer::ping_pong_latency(int rank1, int rank2, int iterations) {
        int world_rank;
        MPI_Comm_rank(comm_, &world_rank);

        int buffer = 42;
        double total_latency = 0.0;

        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            if (world_rank == rank1) {
                MPI_Send(&buffer, 1, MPI_INT, rank2, 0, comm_);
                MPI_Recv(&buffer, 1, MPI_INT, rank2, 0, comm_, MPI_STATUS_IGNORE);
            }
            else if (world_rank == rank2) {
                MPI_Recv(&buffer, 1, MPI_INT, rank1, 0, comm_, MPI_STATUS_IGNORE);
                MPI_Send(&buffer, 1, MPI_INT, rank1, 0, comm_);
            }
        }

        // Actual measurement
        for (int i = 0; i < iterations; ++i) {
            MPI_Barrier(comm_);

            auto start_time = MPI_Wtime();

            if (world_rank == rank1) {
                MPI_Send(&buffer, 1, MPI_INT, rank2, 0, comm_);
                MPI_Recv(&buffer, 1, MPI_INT, rank2, 0, comm_, MPI_STATUS_IGNORE);
            }
            else if (world_rank == rank2) {
                MPI_Recv(&buffer, 1, MPI_INT, rank1, 0, comm_, MPI_STATUS_IGNORE);
                MPI_Send(&buffer, 1, MPI_INT, rank1, 0, comm_);
            }

            auto end_time = MPI_Wtime();

            if (world_rank == rank1 || world_rank == rank2) {
                double round_trip = end_time - start_time;
                total_latency += (round_trip * 1e6) / 2.0; // One-way latency in microseconds
            }
        }

        return total_latency / iterations;
    }

    // StatisticalAnalyzer implementation
    StatisticalAnalyzer::StatisticalAnalyzer()
        : confidence_level_(0.95) {
    }

    StatisticalAnalyzer::~StatisticalAnalyzer() {}

    void StatisticalAnalyzer::add_sample(double value) {
        samples_.push_back(value);
    }

    void StatisticalAnalyzer::clear_samples() {
        samples_.clear();
    }

    double StatisticalAnalyzer::calculate_mean() const {
        if (samples_.empty()) return 0.0;

        double sum = std::accumulate(samples_.begin(), samples_.end(), 0.0);
        return sum / samples_.size();
    }

    double StatisticalAnalyzer::calculate_median() const {
        if (samples_.empty()) return 0.0;

        std::vector<double> sorted_samples = samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());

        size_t n = sorted_samples.size();
        if (n % 2 == 0) {
            return (sorted_samples[n / 2 - 1] + sorted_samples[n / 2]) / 2.0;
        }
        else {
            return sorted_samples[n / 2];
        }
    }

    double StatisticalAnalyzer::calculate_standard_deviation() const {
        if (samples_.size() <= 1) return 0.0;

        double mean = calculate_mean();
        double sum_squared_diff = 0.0;

        for (double sample : samples_) {
            double diff = sample - mean;
            sum_squared_diff += diff * diff;
        }

        return std::sqrt(sum_squared_diff / (samples_.size() - 1));
    }

    double StatisticalAnalyzer::calculate_variance() const {
        double stddev = calculate_standard_deviation();
        return stddev * stddev;
    }

    double StatisticalAnalyzer::calculate_confidence_interval() const {
        if (samples_.size() <= 1) return 0.0;

        double stddev = calculate_standard_deviation();
        double z_score = 1.96; // For 95% confidence

        return z_score * stddev / std::sqrt(samples_.size());
    }

    double StatisticalAnalyzer::calculate_min() const {
        if (samples_.empty()) return 0.0;
        return *std::min_element(samples_.begin(), samples_.end());
    }

    double StatisticalAnalyzer::calculate_max() const {
        if (samples_.empty()) return 0.0;
        return *std::max_element(samples_.begin(), samples_.end());
    }

    bool StatisticalAnalyzer::is_normal_distribution(double significance) const {
        // Simplified normality test using skewness and kurtosis
        if (samples_.size() < 20) return false;

        double mean = calculate_mean();
        double stddev = calculate_standard_deviation();

        if (stddev == 0.0) return false;

        // Calculate skewness
        double skewness = 0.0;
        for (double sample : samples_) {
            double z = (sample - mean) / stddev;
            skewness += z * z * z;
        }
        skewness /= samples_.size();

        // Calculate kurtosis
        double kurtosis = 0.0;
        for (double sample : samples_) {
            double z = (sample - mean) / stddev;
            kurtosis += z * z * z * z;
        }
        kurtosis /= samples_.size();

        // Rough check for normality
        return (std::abs(skewness) < 1.0) && (std::abs(kurtosis - 3.0) < 2.0);
    }

    std::vector<double> StatisticalAnalyzer::detect_outliers(double threshold) const {
        std::vector<double> outliers;

        if (samples_.size() < 4) return outliers;

        std::vector<double> sorted_samples = samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());

        size_t n = sorted_samples.size();
        double q1 = sorted_samples[n / 4];
        double q3 = sorted_samples[3 * n / 4];
        double iqr = q3 - q1;

        double lower_bound = q1 - threshold * iqr;
        double upper_bound = q3 + threshold * iqr;

        for (double sample : samples_) {
            if (sample < lower_bound || sample > upper_bound) {
                outliers.push_back(sample);
            }
        }

        return outliers;
    }

    bool StatisticalAnalyzer::remove_outliers(double threshold) {
        std::vector<double> outliers = detect_outliers(threshold);

        if (outliers.empty()) return false;

        std::vector<double> cleaned_samples;
        std::vector<double> sorted_samples = samples_;
        std::sort(sorted_samples.begin(), sorted_samples.end());

        size_t n = sorted_samples.size();
        double q1 = sorted_samples[n / 4];
        double q3 = sorted_samples[3 * n / 4];
        double iqr = q3 - q1;

        double lower_bound = q1 - threshold * iqr;
        double upper_bound = q3 + threshold * iqr;

        for (double sample : samples_) {
            if (sample >= lower_bound && sample <= upper_bound) {
                cleaned_samples.push_back(sample);
            }
        }

        if (cleaned_samples.size() < samples_.size()) {
            samples_ = cleaned_samples;
            return true;
        }

        return false;
    }

} // namespace TopologyAwareResearch