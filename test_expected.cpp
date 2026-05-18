#include <expected>
#include <string>
int main()
{
  std::expected<int, std::string> e = 42;
  return 0;
}
