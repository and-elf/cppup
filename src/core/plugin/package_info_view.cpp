#include "package_info_view.hpp"

#include "../panic.hpp"

namespace cppup::plugin
{

namespace
{

cppup_source_type translate(cppup::configuration::SourceType source_type)
{
  using cppup::configuration::SourceType;
  switch (source_type)
  {
    case SourceType::DIRECTORY:
      return CPPUP_SOURCE_DIRECTORY;
    case SourceType::GIT:
      return CPPUP_SOURCE_GIT;
    case SourceType::TAR:
      return CPPUP_SOURCE_TAR;
    case SourceType::ZIP:
      return CPPUP_SOURCE_ZIP;
    case SourceType::HTTP:
      return CPPUP_SOURCE_HTTP;
    case SourceType::REGISTRY:
      return CPPUP_SOURCE_REGISTRY;
  }
  ::cppup::panic("PackageInfoView: unhandled SourceType");
}

const char* optional_cstr(const std::optional<std::string>& opt)
{
  return opt.has_value() ? opt->c_str() : nullptr;
}

}  // namespace

PackageInfoView::PackageInfoView(const cppup::configuration::PackageInfo& info)
{
  if (!info.build_args.empty())
  {
    build_arg_ptrs_.reserve(info.build_args.size() + 1);
    for (const auto& arg : info.build_args)
    {
      build_arg_ptrs_.push_back(arg.c_str());
    }
    build_arg_ptrs_.push_back(nullptr);
  }

  view_ = cppup_package_info_v1{
      .name             = info.name.c_str(),
      .version          = optional_cstr(info.version),
      .source_directory = optional_cstr(info.source_directory),
      .url              = optional_cstr(info.url),
      .source_type      = translate(info.source_type),
      .git_branch       = optional_cstr(info.git_branch),
      .git_commit       = optional_cstr(info.git_commit),
      .subdirectory     = optional_cstr(info.subdirectory),
      .build_args       = build_arg_ptrs_.empty() ? nullptr : build_arg_ptrs_.data(),
  };
}

}  // namespace cppup::plugin
