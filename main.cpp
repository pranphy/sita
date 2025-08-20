#include "gui.h"

int main() {


    GLFWApp window{};
    window.create();
    window.mainloop();
    window.cleanup();

    return 0;
}

