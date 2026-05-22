#pragma once

#include <cppup/plugin/abi.h>

#include <vector>

#include "../configuration/types.hpp"

namespace cppup::plugin
{

// Builds a C-ABI view of a configuration::PackageInfo for passing
// into a cppup_package_source_vtable_v1::create call. The view
// borrows all string storage from the source PackageInfo; the caller
// must keep the source alive for the lifetime of the view (and not
// mutate it).
//
// build_args is exposed as a NUL-pointer-terminated array of C
// strings to keep the C ABI struct trivially copyable. When the
// source's build_args is empty, the C view's build_args is set to
// nullptr (callers may distinguish empty vs. absent by checking
// against nullptr).
class PackageInfoView
{
 public:
  explicit PackageInfoView(const cppup::configuration::PackageInfo& info);

  PackageInfoView(const PackageInfoView&)            = delete;
  PackageInfoView& operator=(const PackageInfoView&) = delete;
  PackageInfoView(PackageInfoView&&)                 = delete;
  PackageInfoView& operator=(PackageInfoView&&)      = delete;
  ~PackageInfoView()                                 = default;

  [[nodiscard]] const cppup_package_info_v1* get() const
  {
    return &view_;
  }

 private:
  std::vector<const char*> build_arg_ptrs_;  // NULL-pointer-terminated when non-empty
  cppup_package_info_v1    view_{};
};

}  // namespace cppup::plugin
