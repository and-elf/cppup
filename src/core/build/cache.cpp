#include "cache.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sqlite3.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>

#include "../dependency/database.hpp"
#include "../panic.hpp"

namespace cppup::build
{

namespace
{

std::string to_hex(const unsigned char* data, std::size_t len)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i)
  {
    oss << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  return oss.str();
}

// nullopt if the file can't be opened (e.g. doesn't exist yet). OpenSSL
// failures panic — they indicate a broken crypto library, not an expected
// runtime condition.
std::optional<std::string> sha256_file(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary);
  if (!in)
  {
    return std::nullopt;
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  CPPUP_CHECK(ctx != nullptr, "EVP_MD_CTX_new failed");
  CPPUP_CHECK(EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1, "EVP_DigestInit_ex failed");

  std::array<char, 8192> buffer{};
  while (in)
  {
    in.read(buffer.data(), buffer.size());
    auto n = in.gcount();
    if (n > 0)
    {
      CPPUP_CHECK(EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(n)) == 1,
                  "EVP_DigestUpdate failed");
    }
  }

  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int                               digest_len = 0;
  CPPUP_CHECK(EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) == 1,
              "EVP_DigestFinal_ex failed");
  EVP_MD_CTX_free(ctx);

  return to_hex(digest.data(), digest_len);
}

std::string serialize_string_vector(const std::vector<std::string>& v)
{
  std::ostringstream oss;
  for (std::size_t i = 0; i < v.size(); ++i)
  {
    if (i > 0)
    {
      oss << '\x1f';
    }
    oss << v[i];
  }
  return oss.str();
}

std::string target_signature(const BuildTarget& target)
{
  std::ostringstream oss;
  oss << target.name << '\x1e' << target.type << '\x1e' << target.output_path.string() << '\x1e'
      << serialize_string_vector(target.compile_flags) << '\x1e'
      << serialize_string_vector(target.link_flags) << '\x1e'
      << serialize_string_vector(target.definitions);
  for (const auto& p : target.include_paths)
  {
    oss << '\x1f' << p.string();
  }
  for (const auto& p : target.source_files)
  {
    oss << '\x1f' << p.string();
  }
  return oss.str();
}

class SqliteBuildCache final : public BuildCache
{
 public:
  SqliteBuildCache(sqlite3* db, std::unique_ptr<cppup::dependency::DependencyDatabase> dep_db) :
      db_(db), dep_db_(std::move(dep_db))
  {
  }

  ~SqliteBuildCache() override
  {
    if (db_ != nullptr)
    {
      sqlite3_close(db_);
    }
  }

  bool needs_rebuild(const BuildTarget& target) override
  {
    if (!std::filesystem::exists(target.output_path))
    {
      record_miss();
      return true;
    }

    auto stored = load_entry(target.name);
    if (!stored || stored->signature != target_signature(target))
    {
      record_miss();
      return true;
    }

    for (const auto& dep : load_dependencies(target.name))
    {
      if (!std::filesystem::exists(dep.file_path))
      {
        record_miss();
        return true;
      }
      auto checksum = sha256_file(dep.file_path);
      if (!checksum || *checksum != dep.checksum)
      {
        record_miss();
        return true;
      }
    }

    record_hit();
    return false;
  }

  std::optional<std::string> calculate_file_checksum(const std::filesystem::path& file) override
  {
    return sha256_file(file);
  }

  void cache_build_result(const BuildTarget&                 target,
                          const std::vector<FileDependency>& dependencies) override
  {
    exec("BEGIN IMMEDIATE");

    const char*   sql  = R"(
      INSERT OR REPLACE INTO cache_entries
        (target_name, output_path, signature, build_time)
      VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt = nullptr;
    CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
                std::string{"prepare cache_entries failed: "} + sqlite3_errmsg(db_));

    const auto signature  = target_signature(target);
    const auto build_time = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

    sqlite3_bind_text(stmt, 1, target.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target.output_path.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, signature.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, build_time);

    const auto step_rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    CPPUP_CHECK(step_rc == SQLITE_DONE,
                std::string{"insert cache_entries failed: "} + sqlite3_errmsg(db_));

    exec_bound("DELETE FROM file_dependencies WHERE target_name = ?", target.name);

    const char*   dep_sql  = R"(
      INSERT INTO file_dependencies (target_name, file_path, checksum)
      VALUES (?, ?, ?)
    )";
    sqlite3_stmt* dep_stmt = nullptr;
    CPPUP_CHECK(sqlite3_prepare_v2(db_, dep_sql, -1, &dep_stmt, nullptr) == SQLITE_OK,
                std::string{"prepare file_dependencies failed: "} + sqlite3_errmsg(db_));

    const auto bind_and_step = [&](const std::string& path, const std::string& checksum)
    {
      sqlite3_reset(dep_stmt);
      sqlite3_bind_text(dep_stmt, 1, target.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(dep_stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(dep_stmt, 3, checksum.c_str(), -1, SQLITE_TRANSIENT);
      CPPUP_CHECK(sqlite3_step(dep_stmt) == SQLITE_DONE,
                  std::string{"insert file_dependencies failed: "} + sqlite3_errmsg(db_));
    };

    for (const auto& dep : dependencies)
    {
      bind_and_step(dep.file_path.string(), dep.checksum);
      for (const auto& include_path : dep.includes)
      {
        // Header vanished between scan and persist; skip rather than fail the
        // whole cache write. needs_rebuild's existence check will catch a
        // still-missing file on the next lookup.
        if (auto include_checksum = sha256_file(include_path))
        {
          bind_and_step(include_path.string(), *include_checksum);
        }
      }
    }
    sqlite3_finalize(dep_stmt);

    exec("COMMIT");
  }

  CacheStats get_stats() override
  {
    CacheStats stats;
    stats.hits       = hits_;
    stats.misses     = misses_;
    const auto total = hits_ + misses_;
    stats.hit_rate   = total == 0 ? 0.0 : static_cast<double>(hits_) / static_cast<double>(total);
    return stats;
  }

 private:
  struct StoredEntry
  {
    std::filesystem::path output_path;
    std::string           signature;
    std::int64_t          build_time = 0;
  };

  std::optional<StoredEntry> load_entry(const std::string& name)
  {
    const char* sql =
        "SELECT output_path, signature, build_time FROM cache_entries WHERE target_name = ?";
    sqlite3_stmt* stmt = nullptr;
    CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
                std::string{"prepare load_entry failed: "} + sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<StoredEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      StoredEntry entry;
      entry.output_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      entry.signature   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      entry.build_time  = sqlite3_column_int64(stmt, 2);
      result            = std::move(entry);
    }
    sqlite3_finalize(stmt);
    return result;
  }

  std::vector<FileDependency> load_dependencies(const std::string& name)
  {
    const char*   sql  = "SELECT file_path, checksum FROM file_dependencies WHERE target_name = ?";
    sqlite3_stmt* stmt = nullptr;
    CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
                std::string{"prepare load_dependencies failed: "} + sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    std::vector<FileDependency> deps;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      FileDependency dep;
      dep.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      dep.checksum  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      deps.push_back(std::move(dep));
    }
    sqlite3_finalize(stmt);
    return deps;
  }

  void exec(const char* sql)
  {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
      std::string const msg = err != nullptr ? err : "unknown sqlite error";
      sqlite3_free(err);
      ::cppup::panic("sqlite exec failed: " + msg);
    }
  }

  void exec_bound(const char* sql, const std::string& arg)
  {
    sqlite3_stmt* stmt = nullptr;
    CPPUP_CHECK(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK,
                std::string{"prepare failed: "} + sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt, 1, arg.c_str(), -1, SQLITE_TRANSIENT);
    const auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    CPPUP_CHECK(rc == SQLITE_DONE, std::string{"step failed: "} + sqlite3_errmsg(db_));
  }

  void record_hit()
  {
    ++hits_;
  }
  void record_miss()
  {
    ++misses_;
  }

  sqlite3*                                               db_;
  std::unique_ptr<cppup::dependency::DependencyDatabase> dep_db_;
  std::size_t                                            hits_   = 0;
  std::size_t                                            misses_ = 0;
};

// nullptr if sqlite_open fails or schema init fails — caller proceeds without
// caching. Inside SqliteBuildCache, subsequent sqlite failures on this open
// DB are panics (we got past schema init, so further failures indicate
// corruption or disk-full).
sqlite3* open_cache_db(const std::filesystem::path& db_path)
{
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return nullptr;
  }

  const char* schema = R"(
    CREATE TABLE IF NOT EXISTS cache_entries (
      target_name TEXT PRIMARY KEY,
      output_path TEXT NOT NULL,
      signature   TEXT NOT NULL,
      build_time  INTEGER NOT NULL
    );
    CREATE TABLE IF NOT EXISTS file_dependencies (
      id           INTEGER PRIMARY KEY AUTOINCREMENT,
      target_name  TEXT NOT NULL,
      file_path    TEXT NOT NULL,
      checksum     TEXT NOT NULL,
      FOREIGN KEY (target_name) REFERENCES cache_entries(target_name) ON DELETE CASCADE
    );
    CREATE INDEX IF NOT EXISTS idx_file_dependencies_target ON file_dependencies(target_name);
  )";

  char* err = nullptr;
  if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK)
  {
    sqlite3_free(err);
    sqlite3_close(db);
    return nullptr;
  }

  return db;
}

}  // namespace

std::vector<std::string> DependencyScanner::scan_includes(const std::filesystem::path& source_file)
{
  std::ifstream in(source_file);
  if (!in)
  {
    return {};
  }

  static const std::regex  include_re(R"(^\s*#\s*include\s*[<"]([^>"]+)[>"])");
  std::vector<std::string> includes;
  std::string              line;
  while (std::getline(in, line))
  {
    std::smatch m;
    if (std::regex_search(line, m, include_re))
    {
      includes.push_back(m[1].str());
    }
  }
  return includes;
}

std::unique_ptr<BuildCache> create_build_cache(
    const std::filesystem::path&                           cache_dir,
    std::unique_ptr<cppup::dependency::DependencyDatabase> db)
{
  std::error_code ec;
  std::filesystem::create_directories(cache_dir, ec);
  if (ec)
  {
    return nullptr;
  }

  sqlite3* handle = open_cache_db(cache_dir / "build_cache.db");
  if (handle == nullptr)
  {
    return nullptr;
  }

  return std::unique_ptr<BuildCache>(new SqliteBuildCache(handle, std::move(db)));
}

}  // namespace cppup::build
