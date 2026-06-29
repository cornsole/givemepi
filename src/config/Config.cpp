#include "config/Config.hpp"

namespace pi::config
{

// Intentionally empty.
//
// Config is currently a simple aggregate that initializes all members
// using the defaults defined in Defaults.hpp.
//
// This translation unit exists to provide a stable location for future
// implementations, such as:
//
// - Configuration validation
// - Normalization
// - Version migration
// - Serialization helpers
// - Debug printing

} // namespace pi::config