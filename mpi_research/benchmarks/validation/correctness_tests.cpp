#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../src/core/collective_optimizer.h"
#include "../../src/algorithms/topology_aware_broadcast.h"

using namespace TopologyAwareResearch;

class CorrectnessTests {
private:
    MPI_Comm comm_;
    int world_rank_, world_size_;
    CollectiveOptimizer optimizer_;
    TopologyAwareBroadcast topology_broadcast_;

    double tolerance_;
    int test_iterations_;

public:
    CorrectnessTests(MPI_Comm comm)
        : comm_(comm),
        optimizer_(),
        topology_broadcast_(optimizer_.get_network_characteristics()) {

        MPI_Comm_rank(comm_, &world_rank_);
        MPI_Comm_size(comm_, &world_size_);

        tolerance_ = 1e-9;
        test_iterations_ = 5;
    }

    bool run_all_correctness_tests() {
        if (world_rank_ == 0) {
            std::cout << "=== Running Correctness Tests ===" << std::endl;
        }

        bool all_passed = true;

        // Test broadcast operations
        all_passed &= test_broadcast_correctness();

        // Test reduce operations
        all_passed &= test_reduce_correctness();

        // Test allreduce operations
        all_passed &= test_allreduce_correctness();

        // Test allgather operations
        all_passed &= test_allgather_correctness();

        // Test topology-aware algorithms
        all_passed &= test_topology_aware_correctness();

        if (world_rank_ == 0) {
            if (all_passed) {
                std::cout << "=== ALL CORRECTNESS TESTS PASSED ===" << std::endl;
            }
            else {
                std::cout << "=== SOME TESTS FAILED ===" << std::endl;
            }
        }

        return all_passed;
    }

    bool test_broadcast_correctness() {
        if (world_rank_ == 0) {
            std::cout << "Testing Broadcast Correctness..." << std::endl;
        }

        bool all_passed = true;
        std::vector<int> test_sizes = { 1, 16, 256, 4096 };
        std::vector<int> roots = { 0, world_size_ / 2, world_size_ - 1 };

        for (int size : test_sizes) {
            for (int root : roots) {
                bool passed = test_single_broadcast(size, root);
                all_passed &= passed;

                if (world_rank_ == 0 && !passed) {
                    std::cerr << "  FAILED: Broadcast size=" << size
                        << ", root=" << root << std::endl;
                }
            }
        }

        if (world_rank_ == 0 && all_passed) {
            std::cout << "  All broadcast tests passed" << std::endl;
        }

        return all_passed;
    }

    bool test_single_broadcast(int size, int root) {
        std::vector<double> send_buffer(size);
        std::vector<double> recv_buffer(size);

        // Initialize source buffer
        if (world_rank_ == root) {
            initialize_sequential(send_buffer.data(), size, root);
        }

        // Test native MPI broadcast
        MPI_Bcast(send_buffer.data(), size, MPI_DOUBLE, root, comm_);
        bool native_correct = verify_sequential(recv_buffer.data(), size, root);

        // Reset buffers
        if (world_rank_ == root) {
            initialize_sequential(send_buffer.data(), size, root);
        }
        else {
            std::fill(send_buffer.begin(), send_buffer.end(), 0.0);
        }

        // Test optimized broadcast
        optimizer_.optimize_broadcast(send_buffer.data(), size, MPI_DOUBLE, root, comm_);
        bool optimized_correct = verify_sequential(send_buffer.data(), size, root);

        return native_correct && optimized_correct;
    }

    bool test_reduce_correctness() {
        if (world_rank_ == 0) {
            std::cout << "Testing Reduce Correctness..." << std::endl;
        }

        bool all_passed = true;
        std::vector<int> test_sizes = { 1, 16, 256 };
        std::vector<MPI_Op> operations = { MPI_SUM, MPI_MAX, MPI_MIN, MPI_PROD };
        std::vector<int> roots = { 0, world_size_ - 1 };

        for (int size : test_sizes) {
            for (MPI_Op op : operations) {
                for (int root : roots) {
                    bool passed = test_single_reduce(size, op, root);
                    all_passed &= passed;

                    if (world_rank_ == 0 && !passed) {
                        std::cerr << "  FAILED: Reduce size=" << size
                            << ", op=" << op_to_string(op)
                            << ", root=" << root << std::endl;
                    }
                }
            }
        }

        if (world_rank_ == 0 && all_passed) {
            std::cout << "  All reduce tests passed" << std::endl;
        }

        return all_passed;
    }

    bool test_single_reduce(int size, MPI_Op op, int root) {
        std::vector<double> send_buffer(size);
        std::vector<double> native_recv(size);
        std::vector<double> optimized_recv(size);

        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test native MPI reduce
        if (world_rank_ == root) {
            MPI_Reduce(MPI_IN_PLACE, native_recv.data(), size, MPI_DOUBLE, op, root, comm_);
        }
        else {
            MPI_Reduce(send_buffer.data(), nullptr, size, MPI_DOUBLE, op, root, comm_);
        }

        // Reset for optimized reduce
        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test optimized reduce
        optimizer_.optimize_reduce(send_buffer.data(), optimized_recv.data(),
            size, MPI_DOUBLE, op, root, comm_);

        // Verify results (only root has the result)
        if (world_rank_ == root) {
            return verify_reduce_result(native_recv.data(), optimized_recv.data(), size, op);
        }

        return true;
    }

    bool test_allreduce_correctness() {
        if (world_rank_ == 0) {
            std::cout << "Testing Allreduce Correctness..." << std::endl;
        }

        bool all_passed = true;
        std::vector<int> test_sizes = { 1, 16, 256, 4096 };
        std::vector<MPI_Op> operations = { MPI_SUM, MPI_MAX, MPI_MIN };

        for (int size : test_sizes) {
            for (MPI_Op op : operations) {
                bool passed = test_single_allreduce(size, op);
                all_passed &= passed;

                if (world_rank_ == 0 && !passed) {
                    std::cerr << "  FAILED: Allreduce size=" << size
                        << ", op=" << op_to_string(op) << std::endl;
                }
            }
        }

        if (world_rank_ == 0 && all_passed) {
            std::cout << "  All allreduce tests passed" << std::endl;
        }

        return all_passed;
    }

    bool test_single_allreduce(int size, MPI_Op op) {
        std::vector<double> send_buffer(size);
        std::vector<double> native_recv(size);
        std::vector<double> optimized_recv(size);

        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test native MPI allreduce
        MPI_Allreduce(send_buffer.data(), native_recv.data(), size, MPI_DOUBLE, op, comm_);

        // Reset for optimized allreduce
        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test optimized allreduce
        optimizer_.optimize_allreduce(send_buffer.data(), optimized_recv.data(),
            size, MPI_DOUBLE, op, comm_);

        return verify_allreduce_result(native_recv.data(), optimized_recv.data(), size, op);
    }

    bool test_allgather_correctness() {
        if (world_rank_ == 0) {
            std::cout << "Testing Allgather Correctness..." << std::endl;
        }

        bool all_passed = true;
        std::vector<int> test_sizes = { 1, 4, 16, 64 };

        for (int size : test_sizes) {
            bool passed = test_single_allgather(size);
            all_passed &= passed;

            if (world_rank_ == 0 && !passed) {
                std::cerr << "  FAILED: Allgather size=" << size << std::endl;
            }
        }

        if (world_rank_ == 0 && all_passed) {
            std::cout << "  All allgather tests passed" << std::endl;
        }

        return all_passed;
    }

    bool test_single_allgather(int size) {
        std::vector<double> send_buffer(size);
        std::vector<double> native_recv(size * world_size_);
        std::vector<double> optimized_recv(size * world_size_);

        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test native MPI allgather
        MPI_Allgather(send_buffer.data(), size, MPI_DOUBLE,
            native_recv.data(), size, MPI_DOUBLE, comm_);

        // Reset for optimized allgather
        initialize_sequential(send_buffer.data(), size, world_rank_);

        // Test optimized allgather
        optimizer_.optimize_allgather(send_buffer.data(), optimized_recv.data(),
            size, MPI_DOUBLE, comm_);

        return verify_allgather_result(native_recv.data(), optimized_recv.data(), size);
    }

    bool test_topology_aware_correctness() {
        if (world_rank_ == 0) {
            std::cout << "Testing Topology-Aware Algorithms..." << std::endl;
        }

        bool all_passed = true;
        std::vector<int> test_sizes = { 1, 16, 256, 4096 };
        std::vector<int> roots = { 0, world_size_ - 1 };

        for (int size : test_sizes) {
            for (int root : roots) {
                bool passed = test_topology_aware_broadcast(size, root);
                all_passed &= passed;

                if (world_rank_ == 0 && !passed) {
                    std::cerr << "  FAILED: Topology-aware broadcast size=" << size
                        << ", root=" << root << std::endl;
                }
            }
        }

        if (world_rank_ == 0 && all_passed) {
            std::cout << "  All topology-aware tests passed" << std::endl;
        }

        return all_passed;
    }

    bool test_topology_aware_broadcast(int size, int root) {
        std::vector<double> buffer1(size);
        std::vector<double> buffer2(size);

        // Initialize source buffer
        if (world_rank_ == root) {
            initialize_sequential(buffer1.data(), size, root);
            initialize_sequential(buffer2.data(), size, root);
        }

        // Test different topology-aware algorithms
        topology_broadcast_.binomial_tree_broadcast(buffer1.data(), size, MPI_DOUBLE, root, comm_);
        topology_broadcast_.pipeline_broadcast(buffer2.data(), size, MPI_DOUBLE, root, comm_);

        bool binomial_correct = verify_sequential(buffer1.data(), size, root);
        bool pipeline_correct = verify_sequential(buffer2.data(), size, root);

        return binomial_correct && pipeline_correct;
    }

private:
    void initialize_sequential(double* buffer, int size, int rank) {
        for (int i = 0; i < size; ++i) {
            buffer[i] = static_cast<double>(i + rank + 1);
        }
    }

    void initialize_random(double* buffer, int size) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 100.0);

        for (int i = 0; i < size; ++i) {
            buffer[i] = dis(gen);
        }
    }

    bool verify_sequential(const double* buffer, int size, int root) {
        for (int i = 0; i < size; ++i) {
            double expected = static_cast<double>(i + root + 1);
            if (std::abs(buffer[i] - expected) > tolerance_) {
                return false;
            }
        }
        return true;
    }

    bool verify_reduce_result(const double* native, const double* optimized, int size, MPI_Op op) {
        for (int i = 0; i < size; ++i) {
            if (std::abs(native[i] - optimized[i]) > tolerance_) {
                return false;
            }
        }
        return true;
    }

    bool verify_allreduce_result(const double* native, const double* optimized, int size, MPI_Op op) {
        for (int i = 0; i < size; ++i) {
            if (std::abs(native[i] - optimized[i]) > tolerance_) {
                return false;
            }
        }
        return true;
    }

    bool verify_allgather_result(const double* native, const double* optimized, int size) {
        int total_size = size * world_size_;
        for (int i = 0; i < total_size; ++i) {
            if (std::abs(native[i] - optimized[i]) > tolerance_) {
                return false;
            }
        }
        return true;
    }

    std::string op_to_string(MPI_Op op) {
        if (op == MPI_SUM) return "MPI_SUM";
        if (op == MPI_MAX) return "MPI_MAX";
        if (op == MPI_MIN) return "MPI_MIN";
        if (op == MPI_PROD) return "MPI_PROD";
        return "UNKNOWN";
    }
};

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    MPI_Comm comm = MPI_COMM_WORLD;

    CorrectnessTests tests(comm);
    bool all_passed = tests.run_all_correctness_tests();

    MPI_Finalize();
    return all_passed ? 0 : 1;
}