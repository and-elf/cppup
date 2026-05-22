#pragma once

#include <algorithm>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "outputs.hpp"

namespace cppup::configuration
{

/**
 * Resolve the transitive closure of internal library names linked from the
 * given roots, returned in topological order (dependents before their deps).
 *
 * Returns an error if a referenced library name is not present in
 * `all_libraries`, or if a cycle is detected.
 */
inline std::expected<std::vector<std::string>, std::string> resolve_link_set(
    const std::vector<std::string>& roots, const std::vector<Library>& all_libraries)
{
  std::unordered_map<std::string_view, const Library*> by_name;
  by_name.reserve(all_libraries.size());
  for (const auto& lib : all_libraries)
  {
    by_name.emplace(lib.name, &lib);
  }

  enum class Color : std::uint8_t
  {
    White,
    Gray,
    Black
  };
  std::unordered_map<std::string, Color> color;
  std::vector<std::string>               order;  // post-order of DFS

  // Iterative DFS with explicit frame stack. Each frame remembers which
  // dependency index is being visited next so we can resume after recursion.
  struct Frame
  {
    std::string name;
    std::size_t next_dep = 0;
  };

  for (const auto& root : roots)
  {
    if (color[root] == Color::Black)
    {
      continue;
    }

    std::vector<Frame> stack;
    stack.push_back({root, 0});
    color[root] = Color::Gray;

    while (!stack.empty())
    {
      Frame&     frame = stack.back();
      const auto iter  = by_name.find(frame.name);
      if (iter == by_name.end())
      {
        return std::unexpected("unknown library reference: " + frame.name);
      }
      const auto& deps = iter->second->libraries;

      if (frame.next_dep < deps.size())
      {
        const std::string& dep       = deps[frame.next_dep++];
        const auto         dep_color = color[dep];
        if (dep_color == Color::Black)
        {
          continue;
        }
        if (dep_color == Color::Gray)
        {
          return std::unexpected("cycle detected through library: " + dep);
        }
        color[dep] = Color::Gray;
        stack.push_back({dep, 0});
      }
      else
      {
        color[frame.name] = Color::Black;
        order.push_back(std::move(frame.name));
        stack.pop_back();
      }
    }
  }

  // Post-order DFS yields deps before dependents; reverse for link order
  // (dependents first, so unresolved symbols are satisfied by later libs).
  std::ranges::reverse(order);
  return order;
}

/**
 * Aggregate link_flags from the given libraries (in the order they appear in
 * `library_names`), deduplicating while preserving first-occurrence order.
 */
inline std::vector<std::string> aggregate_link_flags(const std::vector<std::string>& library_names,
                                                     const std::vector<Library>&     all_libraries)
{
  std::unordered_map<std::string_view, const Library*> by_name;
  by_name.reserve(all_libraries.size());
  for (const auto& lib : all_libraries)
  {
    by_name.emplace(lib.name, &lib);
  }

  std::vector<std::string>        out;
  std::unordered_set<std::string> seen;
  for (const auto& name : library_names)
  {
    const auto iter = by_name.find(name);
    if (iter == by_name.end())
    {
      continue;
    }
    for (const auto& flag : iter->second->link_flags)
    {
      std::string flag_string{flag.flag};
      if (seen.insert(flag_string).second)
      {
        out.push_back(std::move(flag_string));
      }
    }
  }
  return out;
}

}  // namespace cppup::configuration
