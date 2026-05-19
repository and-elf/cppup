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

std::expected<std::string, std::string> sha256_file(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary);
  if (!in)
  {
    return std::unexpected("cannot open file: " + file.string());
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx)
  {
    return std::unexpected("EVP_MD_CTX_new failed");
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
  {
    EVP_MD_CTX_free(ctx);
    return std::unexpected("EVP_DigestInit_ex failed");
  }

  std::array<char, 8192> buffer{};
  while (in)
  {
    in.read(buffer.data(), buffer.size());
    auto n = in.gcount();
    if (n > 0 && EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(n)) != 1)
    {
      EVP_MD_CTX_free(ctx);
      return std::unexpected("EVP_DigestUpdate failed");
    }
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int  digest_len = 0;
  if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1)
  {
    EVP_MD_CTX_free(ctx);
    return std::unexpected("EVP_DigestFinal_ex failed");
  }
  EVP_MD_CTX_free(ctx);

  return to_hex(digest, digest_len);
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
    if (db_)
    {
      sqlite3_close(db_);
    }
  }

  std::expected<bool, std::string> needs_rebuild(const BuildTarget& target) override
  {
    if (!std::filesystem::exists(target.output_path))
    {
      record_miss();
      return true;
    }

    auto stored = load_entry(target.name);
    if (!stored)
    {
      record_miss();
      return true;
    }
    if (stored->signature != target_signature(target))
    {
      record_miss();
      return true;
    }

    auto deps = load_dependencies(target.name);
    if (!deps)
    {
      record_miss();
      return true;
    }
    for (const auto& dep : *deps)
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

  std::expected<std::string, std::string> calculate_file_checksum(
      const std::filesystem::path& file) override
  {
    return sha256_file(file);
  }

  std::expected<void, std::string> cache_build_result(
      const BuildTarget& target, const std::vector<FileDependency>& dependencies) override
  {
    if (auto r = exec("BEGIN IMMEDIATE"); !r)
    {
      return r;
    }

    const char*   sql  = R"(
      INSERT OR REPLACE INTO cache_entries
        (target_name, output_path, signature, build_time)
      VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
      exec("ROLLBACK");
      return std::unexpected(std::string{"prepare cache_entries failed: "} + sqlite3_errmsg(db_));
    }

    const auto signature  = target_signature(target);
    const auto build_time = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

    sqlite3_bind_text(stmt, 1, target.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target.output_path.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, signature.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, build_time);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
      auto msg = std::string{"insert cache_entries failed: "} + sqlite3_errmsg(db_);
      sqlite3_finalize(stmt);
      exec("ROLLBACK");
      return std::unexpected(msg);
    }
    sqlite3_finalize(stmt);

    if (auto r = exec_bound("DELETE FROM file_dependencies WHERE target_name = ?", target.name); !r)
    {
      exec("ROLLBACK");
      return r;
    }

    const char*   dep_sql  = R"(
      INSERT INTO file_dependencies (target_name, file_path, checksum)
      VALUES (?, ?, ?)
    )";
    sqlite3_stmt* dep_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, dep_sql, -1, &dep_stmt, nullptr) != SQLITE_OK)
    {
      exec("ROLLBACK");
      return std::unexpected(std::string{"prepare file_dependencies failed: "} +
                             sqlite3_errmsg(db_));
    }
    const auto bind_and_step = [&](const std::string& path,
                                   const std::string& checksum) -> std::expected<void, std::string>
    {
      sqlite3_reset(dep_stmt);
      sqlite3_bind_text(dep_stmt, 1, target.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(dep_stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(dep_stmt, 3, checksum.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(dep_stmt) != SQLITE_DONE)
      {
        return std::unexpected(std::string{"insert file_dependencies failed: "} +
                               sqlite3_errmsg(db_));
      }
      return {};
    };

    // Persist the source file and every transitively-found header in the same
    // table; needs_rebuild treats them uniformly via existence + checksum.
    // Headers come in via FileDependency::includes (populated by the build
    // driver from DependencyScanner::scan_includes) but have no precomputed
    // checksum, so hash them here.
    for (const auto& dep : dependencies)
    {
      if (auto r = bind_and_step(dep.file_path.string(), dep.checksum); !r)
      {
        sqlite3_finalize(dep_stmt);
        (void) exec("ROLLBACK");
        return std::unexpected(r.error());
      }
      for (const auto& include_path : dep.includes)
      {
        auto include_checksum = sha256_file(include_path);
        if (!include_checksum)
        {
          // Header vanished between scan and persist; skip rather than fail
          // the whole cache write. needs_rebuild's existence check will
          // catch the missing file on the next lookup if it stays gone.
          continue;
        }
        if (auto r = bind_and_step(include_path.string(), *include_checksum); !r)
        {
          sqlite3_finalize(dep_stmt);
          if (auto rb = exec("ROLLBACK"); !rb)
          { /* best effort */
          }
          return std::unexpected(r.error());
        }
      }
    }
    sqlite3_finalize(dep_stmt);

    return exec("COMMIT");
  }

  std::expected<CacheStats, std::string> get_stats() override
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
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
      return std::nullopt;
    }
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

  std::optional<std::vector<FileDependency>> load_dependencies(const std::string& name)
  {
    const char*   sql  = "SELECT file_path, checksum FROM file_dependencies WHERE target_name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
      return std::nullopt;
    }
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

  std::expected<void, std::string> exec(const char* sql)
  {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
      std::string const msg = err ? err : "unknown sqlite error";
      sqlite3_free(err);
      return std::unexpected(msg);
    }
    return {};
  }

  std::expected<void, std::string> exec_bound(const char* sql, const std::string& arg)
  {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
      return std::unexpected(std::string{"prepare failed: "} + sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, arg.c_str(), -1, SQLITE_TRANSIENT);
    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
    {
      return std::unexpected(std::string{"step failed: "} + sqlite3_errmsg(db_));
    }
    return {};
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

std::expected<sqlite3*, std::string> open_cache_db(const std::filesystem::path& db_path)
{
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK)
  {
    std::string const msg = sqlite3_errmsg(db);
    sqlite3_close(db);
    return std::unexpected("sqlite3_open failed: " + msg);
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
    std::string const msg = err ? err : "unknown sqlite error";
    sqlite3_free(err);
    sqlite3_close(db);
    return std::unexpected("schema init failed: " + msg);
  }

  return db;
}

}  // namespace

std::expected<std::vector<std::string>, std::string> DependencyScanner::scan_includes(
    const std::filesystem::path& source_file)
{
  std::ifstream in(source_file);
  if (!in)
  {
    return std::unexpected("cannot open file: " + source_file.string());
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

std::expected<std::unique_ptr<BuildCache>, std::string> create_build_cache(
    const std::filesystem::path&                           cache_dir,
    std::unique_ptr<cppup::dependency::DependencyDatabase> db)
{
  std::error_code ec;
  std::filesystem::create_directories(cache_dir, ec);
  if (ec)
  {
    return std::unexpected("cannot create cache dir: " + ec.message());
  }

  auto db_handle = open_cache_db(cache_dir / "build_cache.db");
  if (!db_handle)
  {
    return std::unexpected(db_handle.error());
  }

  return std::unique_ptr<BuildCache>(new SqliteBuildCache(*db_handle, std::move(db)));
}

}  // namespace cppup::build
