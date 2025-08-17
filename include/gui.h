#include <GLFW/glfw3.h>
class GLFWApp{
    public:

        GLFWApp();
        static void init();
        int create(int width=800, int height=600, const char* title="title");
        void mainloop();
        void cleanup();

    private:
        GLFWwindow* window;
};
