// Example SQLite-based BuildCache implementation
// This shows how the build cache should be implemented with SQLite instead of binary files

#include <sqlite3.h>

#include "cache.hpp"

namespace cppup::build
{

class SQLiteBuildCache : public BuildCache
{
 public:
  SQLiteBuildCache(const std::filesystem::path& cache_dir) :
      cache_dir_(cache_dir), db_path_(cache_dir / "build_cache.db")
  {
    initialize_database();
  }

  ~SQLiteBuildCache()
  {
    if (db_) sqlite3_close(db_);
  }

 private:
  std::filesystem::path cache_dir_;
  std::filesystem::path db_path_;
  sqlite3*              db_ = nullptr;

  void initialize_database()
  {
    sqlite3_open(db_path_.string().c_str(), &db_);

    const char* schema = R"(
      CREATE TABLE IF NOT EXISTS cache_entries (
        target_name TEXT PRIMARY KEY,
        output_file TEXT NOT NULL,
        build_time INTEGER NOT NULL,
        build_checksum TEXT NOT NULL,
        compiler_flags TEXT NOT NULL,
        is_valid INTEGER DEFAULT 1
      );

      CREATE TABLE IF NOT EXISTS file_dependencies (
        id INTEGER PRIMARY KEY,
        target_name TEXT NOT NULL,
        file_path TEXT NOT NULL,
        checksum TEXT NOT NULL,
        FOREIGN KEY (target_name) REFERENCES cache_entries(target_name)
      );
    )";

    sqlite3_exec(db_, schema, nullptr, nullptr, nullptr);
  }

  // Example: Store cache entry
  void store_entry(const CacheEntry& entry)
  {
    const char* sql = R"(
      INSERT OR REPLACE INTO cache_entries
      (target_name, output_file, build_time, build_checksum, compiler_flags)
      VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, entry.target_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.output_file.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(
        stmt, 3,
        std::chrono::duration_cast<std::chrono::seconds>(entry.build_time.time_since_epoch())
            .count());
    sqlite3_bind_text(stmt, 4, entry.build_checksum.c_str(), -1, SQLITE_TRANSIENT);

    std::string flags;
    for (size_t i = 0; i < entry.compiler_flags.size(); ++i)
    {
      if (i > 0) flags += ",";
      flags += entry.compiler_flags[i];
    }
    sqlite3_bind_text(stmt, 5, flags.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  // Example: Retrieve cache entry
  std::optional<CacheEntry> get_entry(const std::string& target_name)
  {
    const char*   sql = "SELECT * FROM cache_entries WHERE target_name = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, target_name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      CacheEntry entry;
      entry.target_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
      entry.output_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
      // ... populate other fields
      sqlite3_finalize(stmt);
      return entry;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
  }
};

}  // namespace cppup::build