#pragma once

#include <string>
#include <variant>

namespace cppup::cli
{

/**
 * Simple Result type to replace std::expected (C++23)
 */
template <typename T, typename E>
class Result
{
 public:
  Result(T value) : data_(std::move(value)) {}
  Result(E error) : data_(std::move(error)) {}

  bool has_value() const
  {
    return std::holds_alternative<T>(data_);
  }
  bool has_error() const
  {
    return std::holds_alternative<E>(data_);
  }

  const T& value() const
  {
    return std::get<T>(data_);
  }
  const E& error() const
  {
    return std::get<E>(data_);
  }

  explicit operator bool() const
  {
    return has_value();
  }

 private:
  std::variant<T, E> data_;
};

// Helper function to create success result
template <typename T>
Result<T, std::string> success(T value)
{
  return Result<T, std::string>(std::move(value));
}

// Helper function to create error result
template <typename T>
Result<T, std::string> error(std::string message)
{
  return Result<T, std::string>(std::move(message));
}

}  // namespace cppup::cli