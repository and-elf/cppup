#include "progress_sink.hpp"

namespace cppup::cli
{

NullProgressSink& null_progress_sink()
{
  static NullProgressSink sink;
  return sink;
}

}  // namespace cppup::cli
