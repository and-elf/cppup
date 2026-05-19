#include "build_step_executor.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "build_configuration.hpp"

namespace cppup::configuration
{
namespace
{

// Thread-safe build step status tracker
class BuildStepStatusTracker
{
 public:
  explicit BuildStepStatusTracker(size_t num_steps) :
      statuses_(num_steps, BuildStepStatus::NotStarted)
  {
  }

  void set_status(size_t index, BuildStepStatus status)
  {
    std::scoped_lock const lock(mutex_);
    statuses_[index] = status;
    cv_.notify_all();
  }

  BuildStepStatus get_status(size_t index) const
  {
    std::scoped_lock const lock(mutex_);
    return statuses_[index];
  }

  bool are_dependencies_complete(const std::vector<size_t>& dependency_indices) const
  {
    std::scoped_lock const lock(mutex_);
    return std::ranges::all_of(dependency_indices.begin(), dependency_indices.end(),
                               [this](size_t idx)
                               { return statuses_[idx] == BuildStepStatus::Completed; });
  }

  void wait_for_dependencies(size_t /*step_index*/, const std::vector<size_t>& dependency_indices)
  {
    if (dependency_indices.empty())
    {
      return;  // No dependencies, can proceed immediately
    }

    std::unique_lock lock(mutex_);
    cv_.wait(lock,
             [this, &dependency_indices]()
             {
               return std::ranges::all_of(dependency_indices.begin(), dependency_indices.end(),
                                          [this](size_t idx)
                                          {
                                            return statuses_[idx] == BuildStepStatus::Completed ||
                                                   statuses_[idx] == BuildStepStatus::Failed;
                                          });
             });
  }

 private:
  mutable std::mutex              mutex_;
  mutable std::condition_variable cv_;
  std::vector<BuildStepStatus>    statuses_;
};

// Dependency graph node
struct DependencyNode
{
  size_t              index;
  std::string         name;
  std::vector<size_t> dependencies;
  std::vector<size_t> dependents;    // Steps that depend on this one
  int                 indegree = 0;  // For topological sort
};

// Build dependency graph from build steps
std::vector<DependencyNode> build_dependency_graph(const std::vector<BuildStep>& steps)
{
  std::vector<DependencyNode>             graph;
  std::unordered_map<std::string, size_t> name_to_index;

  // Create nodes
  for (size_t i = 0; i < steps.size(); ++i)
  {
    graph.push_back({i, steps[i].name, {}, {}, 0});
    name_to_index[steps[i].name] = i;
  }

  // Build dependencies
  for (size_t i = 0; i < steps.size(); ++i)
  {
    for (const auto& dep_name : steps[i].dependencies)
    {
      auto it = name_to_index.find(dep_name);
      if (it != name_to_index.end())
      {
        size_t const dep_index = it->second;
        graph[i].dependencies.push_back(dep_index);
        graph[dep_index].dependents.push_back(i);
        graph[i].indegree++;
      }
    }
  }

  return graph;
}

// Topological sort using Kahn's algorithm
std::vector<size_t> topological_sort(const std::vector<DependencyNode>& graph)
{
  std::vector<size_t> result;
  std::queue<size_t>  queue;
  std::vector<int> indegree = graph.empty() ? std::vector<int>{} : std::vector<int>(graph.size());

  // Copy indegrees
  for (size_t i = 0; i < graph.size(); ++i)
  {
    indegree[i] = graph[i].indegree;
  }

  // Find nodes with no dependencies
  for (size_t i = 0; i < graph.size(); ++i)
  {
    if (indegree[i] == 0)
    {
      queue.push(i);
    }
  }

  while (!queue.empty())
  {
    size_t const current = queue.front();
    queue.pop();
    result.push_back(current);

    // Reduce indegree of dependents
    for (size_t const dependent : graph[current].dependents)
    {
      if (--indegree[dependent] == 0)
      {
        queue.push(dependent);
      }
    }
  }

  // Check for cycles
  if (result.size() != graph.size())
  {
    // Cycle detected, return empty vector
    return {};
  }

  return result;
}

// Execute a single build step with status tracking
void execute_build_step(const BuildStep& step, size_t step_index, BuildStepStatusTracker& tracker,
                        std::vector<BuildStepResult>& results, std::mutex& results_mutex)
{
  try
  {
    // Mark as running
    tracker.set_status(step_index, BuildStepStatus::Running);

    // Execute the step
    step.callback();

    // Mark as completed
    tracker.set_status(step_index, BuildStepStatus::Completed);

    // Update result
    std::scoped_lock const lock(results_mutex);
    results[step_index].status = BuildStepStatus::Completed;
  }
  catch (const std::exception& e)
  {
    // Mark as failed
    tracker.set_status(step_index, BuildStepStatus::Failed);

    // Update result with error
    std::scoped_lock const lock(results_mutex);
    results[step_index].status        = BuildStepStatus::Failed;
    results[step_index].error_message = e.what();
  }
}

}  // namespace

// Simplified bootstrap implementation
BuildStepExecutionResult BuildStepExecutor::execute_build_steps(
    const BuildConfiguration& config) const
{
  return execute_steps_parallel(config.build_steps);
}

BuildStepExecutionResult BuildStepExecutor::execute_steps_parallel(
    const std::vector<BuildStep>& steps, const std::vector<std::string>& /*execution_order*/) const
{
  BuildStepExecutionResult result;
  result.success = true;

  if (steps.empty())
  {
    return result;
  }

  // Build dependency graph
  auto graph = build_dependency_graph(steps);

  // Perform topological sort to check for cycles
  auto execution_order = topological_sort(graph);
  if (execution_order.empty() && !steps.empty())
  {
    // Cycle detected
    result.success       = false;
    result.error_message = "Circular dependency detected in build steps";
    return result;
  }

  // Initialize results
  result.step_results.resize(steps.size());
  for (size_t i = 0; i < steps.size(); ++i)
  {
    result.step_results[i].step_name = steps[i].name;
    result.step_results[i].status    = BuildStepStatus::NotStarted;
  }

  // Status tracker for thread synchronization
  BuildStepStatusTracker tracker(steps.size());

  // Results mutex for thread-safe updates
  std::mutex results_mutex;

  // Track completed steps for termination
  std::atomic<size_t> completed_steps{0};
  size_t              total_steps = steps.size();

  // Queue for steps ready to execute (no dependencies or dependencies completed)
  std::queue<size_t>      ready_queue;
  std::mutex              queue_mutex;
  std::condition_variable queue_cv;

  // Track remaining dependencies for each step
  std::vector<std::atomic<int>> remaining_deps(steps.size());
  for (size_t i = 0; i < steps.size(); ++i)
  {
    remaining_deps[i] = static_cast<int>(graph[i].dependencies.size());
  }

  // Start with steps that have no dependencies
  {
    std::scoped_lock const lock(queue_mutex);
    for (size_t i = 0; i < steps.size(); ++i)
    {
      if (remaining_deps[i] == 0)
      {
        ready_queue.push(i);
      }
    }
  }
  queue_cv.notify_all();

  // Worker function for executing steps
  auto worker = [&]()
  {
    while (true)
    {
      size_t step_idx{};
      bool   has_work = false;
      {
        std::unique_lock lock(queue_mutex);
        queue_cv.wait(lock,
                      [&]() { return !ready_queue.empty() || completed_steps == total_steps; });

        if (ready_queue.empty() && completed_steps == total_steps)
        {
          // All work is done
          break;
        }

        if (!ready_queue.empty())
        {
          step_idx = ready_queue.front();
          ready_queue.pop();
          has_work = true;
        }
      }

      if (!has_work)
      {
        break;
      }

      // Execute the step
      execute_build_step(steps[step_idx], step_idx, tracker, result.step_results, results_mutex);
      size_t const current_completed = ++completed_steps;

      // Notify dependents that this step is complete
      for (size_t const dependent : graph[step_idx].dependents)
      {
        if (--remaining_deps[dependent] == 0)
        {
          std::scoped_lock const lock(queue_mutex);
          ready_queue.push(dependent);
          queue_cv.notify_one();
        }
      }

      // If all steps are complete, notify all workers to terminate
      if (current_completed == total_steps)
      {
        queue_cv.notify_all();
      }
    }
  };

  // Start worker threads
  std::vector<std::thread> workers;
  unsigned int const       num_threads = std::max(1U, std::thread::hardware_concurrency());
  workers.reserve(num_threads);
  for (unsigned int i = 0; i < num_threads; ++i)
  {
    workers.emplace_back(worker);
  }

  // Wait for all workers to complete
  for (auto& worker_thread : workers)
  {
    if (worker_thread.joinable())
    {
      worker_thread.join();
    }
  }

  // Check final results
  for (const auto& step_result : result.step_results)
  {
    if (step_result.status == BuildStepStatus::Failed)
    {
      result.success = false;
      if (result.error_message.empty())
      {
        result.error_message =
            "Build step '" + step_result.step_name + "' failed: " + step_result.error_message;
      }
    }
  }

  return result;
}

}  // namespace cppup::configuration
