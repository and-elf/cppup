#pragma once

// During bootstrap build, use the traditional headers instead of modules
#ifdef IS_BOOTSTRAP_BUILD
#include "../src/core/configuration/build_configuration.hpp"
#endif