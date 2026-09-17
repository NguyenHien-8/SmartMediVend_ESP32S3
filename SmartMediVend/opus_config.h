#pragma once

// Arduino compiles recursively vendored C sources with the sketch root as an
// include directory. Forward the codec's generated fixed-point configuration
// without requiring a global Arduino library installation.
#include "src/vendor/arduino-libopus/src/opus_config.h"
