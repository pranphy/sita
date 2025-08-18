#ifndef GUI_H
#define GUI_H

#include <GLFW/glfw3.h>
#include "terminal.h"

class GLFWApp{
    public:

        GLFWApp();
        ~GLFWApp();
        static void init();
        int create(int width=800, int height=600, const char* title="title");
        void mainloop();
        void cleanup();
        void on_key_press(int key, int action);

    private:
        GLFWwindow* window;
        Terminal terminal;
};


#endif
