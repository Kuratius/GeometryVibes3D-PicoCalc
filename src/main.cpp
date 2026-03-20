#include "app/App.hpp"
#include "platform/IPlatform.hpp"

namespace gv {
    IPlatform* createPlatform();
}

// Keep the app object out of the stack.
// App is fairly large on embedded targets, and putting it on the stack
// can overflow the main thread stack and cause memory corruption.
static gv::App app;

int main() {
    // createPlatform() returns a static platform object
    return app.run(*gv::createPlatform());
}