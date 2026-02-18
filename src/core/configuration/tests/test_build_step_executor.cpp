#include "../build_step_executor.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>

using namespace cppup::configuration;

void test_build_step_status() {
    BuildStepResult result;
    
    // Test initial state
    assert(result.status == BuildStepStatus::NotStarted);
    assert(!result.is_success());
    assert(!result.is_failure());
    assert(!result.is_finished());
    
    // Test completed state
    result.status = BuildStepStatus::Completed;
    assert(result.is_success());
    assert(!result.is_failure());
    assert(result.is_finished());
    
    // Test failed state
    result.status = BuildStepStatus::Failed;
    assert(!result.is_success());
    assert(result.is_failure());
    assert(result.is_finished());
    
    // Test skipped state
    result.status = BuildStepStatus::Skipped;
    assert(!result.is_success());
    assert(!result.is_failure());
    assert(result.is_finished());
    
    std::cout << "BuildStepStatus tests passed\n";
}

void test_build_step_execution_result() {
    BuildStepExecutionResult result;
    
    // Test initial state
    assert(!result.is_success());
    assert(result.is_failure());
    assert(result.step_results.empty());
    assert(result.get_failed_steps().empty());
    
    // Add some step results
    BuildStepResult step1;
    step1.step_name = "step1";
    step1.status = BuildStepStatus::Completed;
    result.step_results.push_back(step1);
    
    BuildStepResult step2;
    step2.step_name = "step2";
    step2.status = BuildStepStatus::Failed;
    step2.error_message = "Test error";
    result.step_results.push_back(step2);
    
    // Test get_step_result
    auto* found_step = result.get_step_result("step1");
    assert(found_step != nullptr);
    assert(found_step->step_name == "step1");
    assert(found_step->is_success());
    
    auto* not_found = result.get_step_result("nonexistent");
    assert(not_found == nullptr);
    
    // Test get_failed_steps
    auto failed = result.get_failed_steps();
    assert(failed.size() == 1);
    assert(failed[0] == "step2");
    
    std::cout << "BuildStepExecutionResult tests passed\n";
}

void test_build_step_execution_options() {
    BuildStepExecutionOptions options;
    
    // Test default values
    assert(options.parallel_execution);
    assert(options.max_parallel_steps > 0);
    assert(options.stop_on_first_failure);
    assert(!options.verbose_logging);
    assert(options.step_timeout.count() > 0);
    
    std::cout << "BuildStepExecutionOptions tests passed\n";
}

void test_validate_build_steps() {
    BuildStepExecutor executor;
    
    // Test valid build steps
    std::vector<BuildStep> valid_steps = {
        BuildStep{"step1", [](){}},
        BuildStep{"step2", [](){}}.depends_on({"step1"}),
        BuildStep{"step3", [](){}}.depends_on({"step1", "step2"})
    };
    
    auto error = executor.validate_build_steps(valid_steps);
    assert(error.empty());
    
    // Test duplicate names
    std::vector<BuildStep> duplicate_steps = {
        BuildStep{"step1", [](){}},
        BuildStep{"step1", [](){}} // Duplicate name
    };
    
    error = executor.validate_build_steps(duplicate_steps);
    assert(!error.empty());
    assert(error.find("Duplicate") != std::string::npos);
    
    // Test missing dependency
    std::vector<BuildStep> missing_dep_steps = {
        BuildStep{"step1", [](){}}.depends_on({"nonexistent"})
    };
    
    error = executor.validate_build_steps(missing_dep_steps);
    assert(!error.empty());
    assert(error.find("non-existent") != std::string::npos);
    
    // Test circular dependency
    std::vector<BuildStep> circular_steps = {
        BuildStep{"step1", [](){}}.depends_on({"step2"}),
        BuildStep{"step2", [](){}}.depends_on({"step1"})
    };
    
    error = executor.validate_build_steps(circular_steps);
    assert(!error.empty());
    assert(error.find("Circular") != std::string::npos);
    
    // Test empty name
    BuildStep empty_step("", [](){});
    std::vector<BuildStep> empty_name_steps = {empty_step};
    
    error = executor.validate_build_steps(empty_name_steps);
    assert(!error.empty());
    assert(error.find("empty name") != std::string::npos);
    
    std::cout << "Validate build steps tests passed\n";
}

void test_create_execution_order() {
    BuildStepExecutor executor;
    
    // Test simple dependency chain
    std::vector<BuildStep> steps = {
        BuildStep{"step3", [](){}}.depends_on({"step1", "step2"}),
        BuildStep{"step1", [](){}},
        BuildStep{"step2", [](){}}.depends_on({"step1"})
    };
    
    auto order = executor.create_execution_order(steps);
    assert(order.size() == 3);
    
    // step1 should come first
    assert(order[0] == "step1");
    
    // step2 should come before step3
    auto step2_pos = std::find(order.begin(), order.end(), "step2") - order.begin();
    auto step3_pos = std::find(order.begin(), order.end(), "step3") - order.begin();
    assert(step2_pos < step3_pos);
    
    // Test no dependencies
    std::vector<BuildStep> no_deps = {
        BuildStep{"step1", [](){}},
        BuildStep{"step2", [](){}},
        BuildStep{"step3", [](){}}
    };
    
    order = executor.create_execution_order(no_deps);
    assert(order.size() == 3);
    
    std::cout << "Create execution order tests passed\n";
}

void test_execute_single_step() {
    BuildStepExecutor executor;
    
    // Test successful step
    bool executed = false;
    BuildStep success_step("test_step", [&executed]() {
        executed = true;
    });
    
    auto result = executor.execute_single_step(success_step);
    assert(result.is_success());
    assert(result.step_name == "test_step");
    assert(executed);
    assert(result.execution_time.count() >= 0);
    
    // Test failing step
    BuildStep failing_step("failing_step", []() {
        throw std::runtime_error("Test error");
    });
    
    result = executor.execute_single_step(failing_step);
    assert(result.is_failure());
    assert(result.step_name == "failing_step");
    assert(!result.error_message.empty());
    assert(result.exception != nullptr);
    
    // Test step with no callback
    BuildStep no_callback_step("no_callback", nullptr);
    
    result = executor.execute_single_step(no_callback_step);
    assert(result.status == BuildStepStatus::Skipped);
    assert(!result.error_message.empty());
    
    std::cout << "Execute single step tests passed\n";
}

void test_dependencies_satisfied() {
    BuildStepExecutor executor;
    
    std::vector<BuildStep> steps = {
        BuildStep{"step1", [](){}},
        BuildStep{"step2", [](){}}.depends_on({"step1"}),
        BuildStep{"step3", [](){}}.depends_on({"step1", "step2"})
    };
    
    std::set<std::string> completed;
    
    // step1 has no dependencies
    assert(executor.are_dependencies_satisfied("step1", completed, steps));
    
    // step2 depends on step1
    assert(!executor.are_dependencies_satisfied("step2", completed, steps));
    
    completed.insert("step1");
    assert(executor.are_dependencies_satisfied("step2", completed, steps));
    
    // step3 depends on step1 and step2
    assert(!executor.are_dependencies_satisfied("step3", completed, steps));
    
    completed.insert("step2");
    assert(executor.are_dependencies_satisfied("step3", completed, steps));
    
    std::cout << "Dependencies satisfied tests passed\n";
}

void test_get_ready_steps() {
    BuildStepExecutor executor;
    
    std::vector<BuildStep> steps = {
        BuildStep{"step1", [](){}},
        BuildStep{"step2", [](){}}.depends_on({"step1"}),
        BuildStep{"step3", [](){}}.depends_on({"step1"}),
        BuildStep{"step4", [](){}}.depends_on({"step2", "step3"})
    };
    
    std::set<std::string> remaining = {"step1", "step2", "step3", "step4"};
    std::set<std::string> completed;
    
    // Initially, only step1 should be ready
    auto ready = executor.get_ready_steps(remaining, completed, steps);
    assert(ready.size() == 1);
    assert(ready[0] == "step1");
    
    // After step1 completes, step2 and step3 should be ready
    completed.insert("step1");
    remaining.erase("step1");
    ready = executor.get_ready_steps(remaining, completed, steps);
    assert(ready.size() == 2);
    assert(std::find(ready.begin(), ready.end(), "step2") != ready.end());
    assert(std::find(ready.begin(), ready.end(), "step3") != ready.end());
    
    // After step2 and step3 complete, step4 should be ready
    completed.insert("step2");
    completed.insert("step3");
    remaining.erase("step2");
    remaining.erase("step3");
    ready = executor.get_ready_steps(remaining, completed, steps);
    assert(ready.size() == 1);
    assert(ready[0] == "step4");
    
    std::cout << "Get ready steps tests passed\n";
}

void test_execute_build_steps_sequential() {
    BuildStepExecutionOptions options;
    options.parallel_execution = false;
    options.verbose_logging = false;
    
    BuildStepExecutor executor(options);
    
    std::vector<int> execution_order;
    std::mutex order_mutex;
    
    BuildConfiguration config{
        .build_steps = {
            BuildStep{"step1", [&]() {
                std::lock_guard<std::mutex> lock(order_mutex);
                execution_order.push_back(1);
            }},
            BuildStep{"step2", [&]() {
                std::lock_guard<std::mutex> lock(order_mutex);
                execution_order.push_back(2);
            }}.depends_on({"step1"}),
            BuildStep{"step3", [&]() {
                std::lock_guard<std::mutex> lock(order_mutex);
                execution_order.push_back(3);
            }}.depends_on({"step2"})
        }
    };
    
    auto result = executor.execute_build_steps(config);
    assert(result.is_success());
    assert(result.step_results.size() == 3);
    
    // Check execution order
    assert(execution_order.size() == 3);
    assert(execution_order[0] == 1);
    assert(execution_order[1] == 2);
    assert(execution_order[2] == 3);
    
    // Check all steps completed successfully
    for (const auto& step_result : result.step_results) {
        assert(step_result.is_success());
    }
    
    std::cout << "Execute build steps sequential tests passed\n";
}

void test_execute_build_steps_parallel() {
    BuildStepExecutionOptions options;
    options.parallel_execution = true;
    options.max_parallel_steps = 4;
    options.verbose_logging = false;
    
    BuildStepExecutor executor(options);
    
    std::atomic<int> concurrent_count{0};
    std::atomic<int> max_concurrent{0};
    
    BuildConfiguration config{
        .build_steps = {
            BuildStep{"step1", [&]() {
                int current = ++concurrent_count;
                int expected = max_concurrent.load();
                while (expected < current && !max_concurrent.compare_exchange_weak(expected, current)) {}
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                --concurrent_count;
            }},
            BuildStep{"step2", [&]() {
                int current = ++concurrent_count;
                int expected = max_concurrent.load();
                while (expected < current && !max_concurrent.compare_exchange_weak(expected, current)) {}
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                --concurrent_count;
            }},
            BuildStep{"step3", [&]() {
                int current = ++concurrent_count;
                int expected = max_concurrent.load();
                while (expected < current && !max_concurrent.compare_exchange_weak(expected, current)) {}
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                --concurrent_count;
            }}
        }
    };
    
    auto result = executor.execute_build_steps(config);
    assert(result.is_success());
    assert(result.step_results.size() == 3);
    
    // Check that steps ran in parallel (max concurrent should be > 1)
    assert(max_concurrent.load() > 1);
    
    // Check all steps completed successfully
    for (const auto& step_result : result.step_results) {
        assert(step_result.is_success());
    }
    
    std::cout << "Execute build steps parallel tests passed\n";
}

void test_execute_build_steps_with_failure() {
    BuildStepExecutionOptions options;
    options.parallel_execution = false;
    options.stop_on_first_failure = true;
    options.verbose_logging = false;
    
    BuildStepExecutor executor(options);
    
    bool step3_executed = false;
    
    BuildConfiguration config{
        .build_steps = {
            BuildStep{"step1", []() {
                // Success
            }},
            BuildStep{"step2", []() {
                throw std::runtime_error("Step 2 failed");
            }}.depends_on({"step1"}),
            BuildStep{"step3", [&]() {
                step3_executed = true;
            }}.depends_on({"step2"})
        }
    };
    
    auto result = executor.execute_build_steps(config);
    assert(result.is_failure());
    assert(!result.error_message.empty());
    
    // step3 should not have executed due to stop_on_first_failure
    assert(!step3_executed);
    
    // Check failed steps
    auto failed = result.get_failed_steps();
    assert(failed.size() == 1);
    assert(failed[0] == "step2");
    
    std::cout << "Execute build steps with failure tests passed\n";
}

void test_execute_build_steps_empty() {
    BuildStepExecutor executor;
    
    BuildConfiguration config; // No build steps
    
    auto result = executor.execute_build_steps(config);
    assert(result.is_success());
    assert(result.step_results.empty());
    assert(result.total_execution_time.count() >= 0);
    
    std::cout << "Execute build steps empty tests passed\n";
}

void test_status_callback() {
    BuildStepExecutionOptions options;
    options.parallel_execution = false;
    
    std::vector<std::pair<std::string, BuildStepStatus>> status_updates;
    options.status_callback = [&](const std::string& name, BuildStepStatus status) {
        status_updates.emplace_back(name, status);
    };
    
    BuildStepExecutor executor(options);
    
    BuildConfiguration config{
        .build_steps = {
            BuildStep{"test_step", []() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }}
        }
    };
    
    auto result = executor.execute_build_steps(config);
    assert(result.is_success());
    
    // Should have received status updates
    assert(!status_updates.empty());
    
    bool found_running = false, found_completed = false;
    for (const auto& [name, status] : status_updates) {
        if (name == "test_step") {
            if (status == BuildStepStatus::Running) found_running = true;
            if (status == BuildStepStatus::Completed) found_completed = true;
        }
    }
    
    assert(found_running);
    assert(found_completed);
    
    std::cout << "Status callback tests passed\n";
}

int main() {
    test_build_step_status();
    test_build_step_execution_result();
    test_build_step_execution_options();
    test_validate_build_steps();
    test_create_execution_order();
    test_execute_single_step();
    test_dependencies_satisfied();
    test_get_ready_steps();
    test_execute_build_steps_sequential();
    test_execute_build_steps_parallel();
    test_execute_build_steps_with_failure();
    test_execute_build_steps_empty();
    test_status_callback();
    
    std::cout << "All build step executor tests passed!\n";
    return 0;
}