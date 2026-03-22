#pragma once

namespace gv {

class IPlatform;

// Returns the app's concrete platform implementation.
IPlatform* createPlatform();

} // namespace gv