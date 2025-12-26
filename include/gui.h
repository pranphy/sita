#ifndef GUI_H
#define GUI_H

#include "terminal.h"
#include "terminal_view.h"
#include <GLFW/glfw3.h>

#ifdef HAVE_WAYLAND
#include "wayland_text_input.h"
#include <memory>
#endif

class GLFWApp {
public:
    GLFWApp();
    ~GLFWApp();

    int create(int width=1920, int height=1080, const char* title="title");
    void mainloop();
    void cleanup();

    // Instance event handlers
    void on_scroll(double x, double y);
    void on_key_press(int key, int action, int mods);
    void on_char(unsigned int codepoint);
    void on_resize(int width, int height);

    // Static GLFW callbacks
    static void scroll_callback(GLFWwindow* w, double x, double y);
    static void key_callback(GLFWwindow* w, int key, int sc, int act, int mods);
    static void char_callback(GLFWwindow* w, unsigned int cp);
    static void resize_callback(GLFWwindow* w, int width, int height);
    static void focus_callback(GLFWwindow* w, int focused);

private:
    GLFWwindow* window = nullptr;

    Terminal terminal;
    TerminalView view;
    std::unique_ptr<TextRenderer> text_renderer;

#ifdef HAVE_WAYLAND
    std::unique_ptr<WaylandTextInput> wayland_input;
#endif
};

#endif
