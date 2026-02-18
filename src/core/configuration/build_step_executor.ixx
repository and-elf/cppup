export module cppup.configuration.build_step_executor;

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>

import cppup.configuration.build_configuration;

export namespace cppup::configuration {

/**
 * Status of a build step execution
 */
export enum class BuildStepStatus {
    NotStarted,
    Waiting,      // Waiting for dependencies
    Running,
    Completed,
    Failed,
    Skipped
};

/**
 * Result of a single build step execution
 */
export struct BuildStepResult {
    std::string step_name;
    BuildStepStatus status = BuildStepStatus::NotStarted;
    std::chrono::milliseconds execution_time{0};
    std::string error_message;
    std::exception_ptr exception;

    [[nodiscard]] bool is_success() const noexcept {
        return status == BuildStepStatus::Completed;
    }
    [[nodiscard]] bool is_failure() const noexcept {
        return status == BuildStepStatus::Failed;
    }
    [[nodiscard]] bool is_finished() const noexcept {
        return status == BuildStepStatus::Completed ||
               status == BuildStepStatus::Failed ||
               status == BuildStepStatus::Skipped;
    }
};

/**
 * Result of build step execution
 */
export struct BuildStepExecutionResult {
    bool success = false;
    std::vector<BuildStepResult> step_results;
    std::chrono::milliseconds total_execution_time{0};
    std::string error_message;

    [[nodiscard]] bool is_success() const noexcept { return success; }
    [[nodiscard]] bool is_failure() const noexcept { return !success; }

    /**
     * Get result for a specific step
     */
    [[nodiscard]] const BuildStepResult* get_step_result(const std::string& step_name) const {
        for (const auto& result : step_results) {
            if (result.step_name == step_name) {
                return &result;
            }
        }
        return nullptr;
    }

    /**
     * Get list of failed steps
     */
    [[nodiscard]] std::vector<std::string> get_failed_steps() const {
        std::vector<std::string> failed;
        for (const auto& result : step_results) {
            if (result.is_failure()) {
                failed.push_back(result.step_name);
            }
        }
        return failed;
    }
};

/**
 * Build step execution options
 */
export struct BuildStepExecutionOptions {
    bool parallel_execution = true;
    size_t max_parallel_steps = std::thread::hardware_concurrency();
    bool stop_on_first_failure = true;
    bool verbose_logging = false;
    std::chrono::milliseconds step_timeout{300000}; // 5 minutes default timeout

    // Callback for step status updates
    std::function<void(const std::string&, BuildStepStatus)> status_callback;
};

/**
 * Build step executor class
 */
export class BuildStepExecutor {
public:
    explicit BuildStepExecutor(BuildStepExecutionOptions options = {})
        : options_(std::move(options)) {}

    /**
     * Execute build steps from a configuration
     * @param config Build configuration containing build steps
     * @return BuildStepExecutionResult with execution results
     */
    [[nodiscard]] BuildStepExecutionResult execute_build_steps(const BuildConfiguration& config);

    /**
     * Execute build steps sequentially
     * @param build_steps List of build steps
     * @param execution_order Order in which to execute steps
     * @return BuildStepExecutionResult with execution results
     */
    [[nodiscard]] BuildStepExecutionResult execute_steps_sequential(
        const std::vector<BuildStep>& build_steps,
        const std::vector<std::string>& execution_order
    );

    /**
     * Execute build steps in parallel
     * @param build_steps List of build steps
     * @param execution_order Order in which to execute steps (respecting dependencies)
     * @return BuildStepExecutionResult with execution results
     */
    [[nodiscard]] BuildStepExecutionResult execute_steps_parallel(
        const std::vector<BuildStep>& build_steps,
        const std::vector<std::string>& execution_order
    );

    /**
     * Validate build step dependencies (check for cycles, missing dependencies)
     * @param build_steps List of build steps to validate
     * @return Error message if invalid, empty string if valid
     */
    [[nodiscard]] std::string validate_build_steps(const std::vector<BuildStep>& build_steps) const;

    /**
     * Create dependency graph and topological ordering
     * @param build_steps List of build steps
     * @return Topologically sorted list of step names, or empty if cycles detected
     */
    [[nodiscard]] std::vector<std::string> create_execution_order(const std::vector<BuildStep>& build_steps) const;

    /**
     * Execute a single build step
     * @param step Build step to execute
     * @return BuildStepResult with execution result
     */
    [[nodiscard]] BuildStepResult execute_single_step(const BuildStep& step);

    /**
     * Check if a step's dependencies are satisfied
     * @param step_name Name of the step to check
     * @param completed_steps Set of completed step names
     * @param build_steps All build steps (for dependency lookup)
     * @return true if dependencies are satisfied
     */
    [[nodiscard]] bool are_dependencies_satisfied(
        const std::string& step_name,
        const std::set<std::string>& completed_steps,
        const std::vector<BuildStep>& build_steps
    ) const;

    /**
     * Get steps that are ready to execute (dependencies satisfied)
     * @param remaining_steps Steps that haven't been executed yet
     * @param completed_steps Set of completed step names
     * @param build_steps All build steps (for dependency lookup)
     * @return List of step names ready for execution
     */
    [[nodiscard]] std::vector<std::string> get_ready_steps(
        const std::set<std::string>& remaining_steps,
        const std::set<std::string>& completed_steps,
        const std::vector<BuildStep>& build_steps
    ) const;

private:
    BuildStepExecutionOptions options_;

    /**
     * Find a build step by name
     */
    [[nodiscard]] const BuildStep* find_build_step(
        const std::vector<BuildStep>& build_steps,
        const std::string& name
    ) const;

    /**
     * Perform depth-first search for cycle detection
     */
    bool has_cycle_dfs(
        const std::string& step_name,
        const std::map<std::string, std::vector<std::string>>& dependencies,
        std::set<std::string>& visited,
        std::set<std::string>& recursion_stack
    ) const;

    /**
     * Topological sort using DFS
     */
    void topological_sort_dfs(
        const std::string& step_name,
        const std::map<std::string, std::vector<std::string>>& dependencies,
        std::set<std::string>& visited,
        std::vector<std::string>& result
    ) const;

    /**
     * Log message if verbose logging is enabled
     */
    void log_verbose(const std::string& message) const;

    /**
     * Notify status callback if set
     */
    void notify_status(const std::string& step_name, BuildStepStatus status) const;
};

// Implementation

BuildStepExecutionResult BuildStepExecutor::execute_build_steps(const BuildConfiguration& config) {
    BuildStepExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (config.build_steps.empty()) {
        result.success = true;
        return result;
    }

    // Validate build steps
    auto validation_error = validate_build_steps(config.build_steps);
    if (!validation_error.empty()) {
        result.error_message = validation_error;
        return result;
    }

    // Create execution order
    auto execution_order = create_execution_order(config.build_steps);
    if (execution_order.empty() && !config.build_steps.empty()) {
        result.error_message = "Failed to create execution order (possible circular dependencies)";
        return result;
    }

    log_verbose("Executing " + std::to_string(config.build_steps.size()) + " build steps");

    // Initialize step results
    for (const auto& step : config.build_steps) {
        BuildStepResult step_result;
        step_result.step_name = step.name;
        step_result.status = BuildStepStatus::NotStarted;
        result.step_results.push_back(step_result);
    }

    if (options_.parallel_execution && options_.max_parallel_steps > 1) {
        // Parallel execution
        result = execute_steps_parallel(config.build_steps, execution_order);
    } else {
        // Sequential execution
        result = execute_steps_sequential(config.build_steps, execution_order);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    log_verbose("Total execution time: " + std::to_string(result.total_execution_time.count()) + "ms");

    return result;
}

BuildStepExecutionResult BuildStepExecutor::execute_steps_sequential(
    const std::vector<BuildStep>& build_steps,
    const std::vector<std::string>& execution_order
) {
    BuildStepExecutionResult result;
    result.success = true;

    // Initialize step results
    for (const auto& step : build_steps) {
        BuildStepResult step_result;
        step_result.step_name = step.name;
        step_result.status = BuildStepStatus::NotStarted;
        result.step_results.push_back(step_result);
    }

    // Execute steps in order
    for (const auto& step_name : execution_order) {
        const BuildStep* step = find_build_step(build_steps, step_name);
        if (!step) {
            result.success = false;
            result.error_message = "Build step not found: " + step_name;
            break;
        }

        log_verbose("Executing step: " + step_name);
        notify_status(step_name, BuildStepStatus::Running);

        auto step_result = execute_single_step(*step);

        // Update result
        for (auto& res : result.step_results) {
            if (res.step_name == step_name) {
                res = step_result;
                break;
            }
        }

        if (step_result.is_failure()) {
            result.success = false;
            if (options_.stop_on_first_failure) {
                result.error_message = "Build step failed: " + step_name;
                break;
            }
        }

        log_verbose("Step " + step_name + " completed in " + std::to_string(step_result.execution_time.count()) + "ms");
    }

    return result;
}

BuildStepExecutionResult BuildStepExecutor::execute_steps_parallel(
    const std::vector<BuildStep>& build_steps,
    const std::vector<std::string>& execution_order
) {
    BuildStepExecutionResult result;
    result.success = true;

    // Initialize step results
    std::map<std::string, BuildStepResult> step_results_map;
    for (const auto& step : build_steps) {
        BuildStepResult step_result;
        step_result.step_name = step.name;
        step_result.status = BuildStepStatus::NotStarted;
        step_results_map[step.name] = step_result;
    }

    std::set<std::string> remaining_steps;
    std::set<std::string> completed_steps;
    std::set<std::string> running_steps;

    for (const auto& step : build_steps) {
        remaining_steps.insert(step.name);
    }

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::future<BuildStepResult>> futures;

    while (!remaining_steps.empty() || !running_steps.empty()) {
        std::unique_lock<std::mutex> lock(mutex);

        // Get steps ready for execution
        auto ready_steps = get_ready_steps(remaining_steps, completed_steps, build_steps);

        // Start new steps (up to max parallel limit)
        size_t available_slots = options_.max_parallel_steps - running_steps.size();
        size_t steps_to_start = std::min(ready_steps.size(), available_slots);

        for (size_t i = 0; i < steps_to_start; ++i) {
            const std::string& step_name = ready_steps[i];
            const BuildStep* step = find_build_step(build_steps, step_name);

            if (step) {
                remaining_steps.erase(step_name);
                running_steps.insert(step_name);

                step_results_map[step_name].status = BuildStepStatus::Running;
                notify_status(step_name, BuildStepStatus::Running);

                log_verbose("Starting parallel step: " + step_name);

                // Launch async execution
                futures.push_back(std::async(std::launch::async, [this, step]() {
                    return execute_single_step(*step);
                }));
            }
        }

        lock.unlock();

        // Check for completed futures
        for (auto it = futures.begin(); it != futures.end();) {
            if (it->wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
                auto step_result = it->get();

                lock.lock();
                running_steps.erase(step_result.step_name);

                if (step_result.is_success()) {
                    completed_steps.insert(step_result.step_name);
                } else {
                    result.success = false;
                    if (options_.stop_on_first_failure) {
                        result.error_message = "Build step failed: " + step_result.step_name;
                        lock.unlock();

                        // Wait for all running tasks to complete
                        for (auto& future : futures) {
                            if (future.valid()) {
                                future.wait();
                            }
                        }

                        // Copy results
                        for (const auto& [name, res] : step_results_map) {
                            result.step_results.push_back(res);
                        }

                        return result;
                    }
                }

                step_results_map[step_result.step_name] = step_result;
                log_verbose("Parallel step " + step_result.step_name + " completed in " +
                           std::to_string(step_result.execution_time.count()) + "ms");

                lock.unlock();

                it = futures.erase(it);
            } else {
                ++it;
            }
        }

        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Copy results
    for (const auto& [name, res] : step_results_map) {
        result.step_results.push_back(res);
    }

    return result;
}

std::string BuildStepExecutor::validate_build_steps(const std::vector<BuildStep>& build_steps) const {
    std::set<std::string> step_names;
    std::map<std::string, std::vector<std::string>> dependencies;

    // Check for duplicate names and build dependency map
    for (const auto& step : build_steps) {
        if (step.name.empty()) {
            return "Build step with empty name found";
        }

        if (step_names.contains(step.name)) {
            return "Duplicate build step name: " + step.name;
        }

        step_names.insert(step.name);
        dependencies[step.name] = step.dependencies;
    }

    // Check for missing dependencies
    for (const auto& [step_name, deps] : dependencies) {
        for (const auto& dep : deps) {
            if (!step_names.contains(dep)) {
                return "Build step '" + step_name + "' depends on non-existent step: " + dep;
            }
        }
    }

    // Check for circular dependencies
    std::set<std::string> visited;
    std::set<std::string> recursion_stack;

    for (const auto& step_name : step_names) {
        if (!visited.contains(step_name)) {
            if (has_cycle_dfs(step_name, dependencies, visited, recursion_stack)) {
                return "Circular dependency detected involving step: " + step_name;
            }
        }
    }

    return ""; // No errors
}

std::vector<std::string> BuildStepExecutor::create_execution_order(const std::vector<BuildStep>& build_steps) const {
    std::map<std::string, std::vector<std::string>> dependencies;

    // Build dependency map
    for (const auto& step : build_steps) {
        dependencies[step.name] = step.dependencies;
    }

    // Perform topological sort
    std::set<std::string> visited;
    std::vector<std::string> result;

    for (const auto& step : build_steps) {
        if (!visited.contains(step.name)) {
            topological_sort_dfs(step.name, dependencies, visited, result);
        }
    }

    // Reverse to get correct execution order
    std::reverse(result.begin(), result.end());

    return result;
}

BuildStepResult BuildStepExecutor::execute_single_step(const BuildStep& step) {
    BuildStepResult result;
    result.step_name = step.name;
    result.status = BuildStepStatus::Running;

    auto start_time = std::chrono::steady_clock::now();

    try {
        if (step.callback) {
            // Execute with timeout if specified
            if (options_.step_timeout.count() > 0) {
                auto future = std::async(std::launch::async, step.callback);
                if (future.wait_for(options_.step_timeout) == std::future_status::timeout) {
                    result.status = BuildStepStatus::Failed;
                    result.error_message = "Build step timed out after " +
                                         std::to_string(options_.step_timeout.count()) + "ms";
                    notify_status(step.name, BuildStepStatus::Failed);
                    return result;
                }
                future.get(); // This will re-throw any exception
            } else {
                step.callback();
            }

            result.status = BuildStepStatus::Completed;
            notify_status(step.name, BuildStepStatus::Completed);
        } else {
            result.status = BuildStepStatus::Skipped;
            result.error_message = "No callback function provided";
            notify_status(step.name, BuildStepStatus::Skipped);
        }
    } catch (const std::exception& e) {
        result.status = BuildStepStatus::Failed;
        result.error_message = e.what();
        result.exception = std::current_exception();
        notify_status(step.name, BuildStepStatus::Failed);
    } catch (...) {
        result.status = BuildStepStatus::Failed;
        result.error_message = "Unknown exception occurred";
        result.exception = std::current_exception();
        notify_status(step.name, BuildStepStatus::Failed);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return result;
}

bool BuildStepExecutor::are_dependencies_satisfied(
    const std::string& step_name,
    const std::set<std::string>& completed_steps,
    const std::vector<BuildStep>& build_steps
) const {
    const BuildStep* step = find_build_step(build_steps, step_name);
    if (!step) {
        return false;
    }

    for (const auto& dep : step->dependencies) {
        if (!completed_steps.contains(dep)) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> BuildStepExecutor::get_ready_steps(
    const std::set<std::string>& remaining_steps,
    const std::set<std::string>& completed_steps,
    const std::vector<BuildStep>& build_steps
) const {
    std::vector<std::string> ready_steps;

    for (const auto& step_name : remaining_steps) {
        if (are_dependencies_satisfied(step_name, completed_steps, build_steps)) {
            ready_steps.push_back(step_name);
        }
    }

    return ready_steps;
}

const BuildStep* BuildStepExecutor::find_build_step(
    const std::vector<BuildStep>& build_steps,
    const std::string& name
) const {
    for (const auto& step : build_steps) {
        if (step.name == name) {
            return &step;
        }
    }
    return nullptr;
}

bool BuildStepExecutor::has_cycle_dfs(
    const std::string& step_name,
    const std::map<std::string, std::vector<std::string>>& dependencies,
    std::set<std::string>& visited,
    std::set<std::string>& recursion_stack
) const {
    visited.insert(step_name);
    recursion_stack.insert(step_name);

    auto it = dependencies.find(step_name);
    if (it != dependencies.end()) {
        for (const auto& dep : it->second) {
            if (!visited.contains(dep)) {
                if (has_cycle_dfs(dep, dependencies, visited, recursion_stack)) {
                    return true;
                }
            } else if (recursion_stack.contains(dep)) {
                return true; // Back edge found - cycle detected
            }
        }
    }

    recursion_stack.erase(step_name);
    return false;
}

void BuildStepExecutor::topological_sort_dfs(
    const std::string& step_name,
    const std::map<std::string, std::vector<std::string>>& dependencies,
    std::set<std::string>& visited,
    std::vector<std::string>& result
) const {
    visited.insert(step_name);

    auto it = dependencies.find(step_name);
    if (it != dependencies.end()) {
        for (const auto& dep : it->second) {
            if (!visited.contains(dep)) {
                topological_sort_dfs(dep, dependencies, visited, result);
            }
        }
    }

    result.push_back(step_name);
}

void BuildStepExecutor::log_verbose(const std::string& message) const {
    if (options_.verbose_logging) {
        std::cout << "[BuildStepExecutor] " << message << std::endl;
    }
}

void BuildStepExecutor::notify_status(const std::string& step_name, BuildStepStatus status) const {
    if (options_.status_callback) {
        options_.status_callback(step_name, status);
    }
}

} // namespace cppup::configuration