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

cppup::configuration::SourceType reverse_translate(cppup_source_type source_type)
{
  using cppup::configuration::SourceType;
  switch (source_type)
  {
    case CPPUP_SOURCE_DIRECTORY:
      return SourceType::DIRECTORY;
    case CPPUP_SOURCE_GIT:
      return SourceType::GIT;
    case CPPUP_SOURCE_TAR:
      return SourceType::TAR;
    case CPPUP_SOURCE_ZIP:
      return SourceType::ZIP;
    case CPPUP_SOURCE_HTTP:
      return SourceType::HTTP;
    case CPPUP_SOURCE_REGISTRY:
      return SourceType::REGISTRY;
  }
  ::cppup::panic("from_c_view: unknown cppup_source_type");
}

}  // namespace

cppup::configuration::PackageInfo from_c_view(const cppup_package_info_v1& view)
{
  cppup::configuration::PackageInfo info;
  info.name = (view.name != nullptr) ? view.name : "";
  if (view.version != nullptr)
  {
    info.version = view.version;
  }
  if (view.source_directory != nullptr)
  {
    info.source_directory = view.source_directory;
  }
  if (view.url != nullptr)
  {
    info.url = view.url;
  }
  info.source_type = reverse_translate(view.source_type);
  if (view.git_branch != nullptr)
  {
    info.git_branch = view.git_branch;
  }
  if (view.git_commit != nullptr)
  {
    info.git_commit = view.git_commit;
  }
  if (view.subdirectory != nullptr)
  {
    info.subdirectory = view.subdirectory;
  }
  if (view.build_args != nullptr)
  {
    for (std::size_t i = 0; view.build_args[i] != nullptr; ++i)
    {
      info.build_args.emplace_back(view.build_args[i]);
    }
  }
  return info;
}

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
