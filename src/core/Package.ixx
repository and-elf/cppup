export module core.Package;

#include <string>
#include <vector>

export struct Package
{
  enum class Type uint8_t
  {
    UNKNOWN,
    GIT,
    TAR,
    DIRECTORY
  };
  std::optional<std::string>              name;
  std::optional<std::string>              version;
  std::optional<std::vector<std::string>> tags;
  std::optional<std::string>              platform;
  std::optional<std::string>              path;
  std::optional<std::string>              sourceUrl;
  Type                                    kind;  // e.g., "tar", "git", "directory"

  // Fixed-size script buffer and current script view
  char             script_buffer[256]{};
  std::size_t      script_size = 0;
  std::string_view script;  // points into script_buffer

  virtual ~Package()     = default;
  virtual void fetch()   = 0;
  virtual void install() = 0;
  virtual void remove()  = 0;
};
