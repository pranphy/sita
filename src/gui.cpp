#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef HAVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif

#include <iostream>
#include <print>
#include <unordered_map>
#include <string>

#include "gui.h"

namespace {

// UTF‑8 encode a Unicode codepoint
std::string utf8_encode(unsigned int cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

} // namespace


GLFWApp::GLFWApp()
    : terminal(1920, 1080),
      view(terminal)
{
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_CORE_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

GLFWApp::~GLFWApp() {
    if (window)
        glfwDestroyWindow(window);
    glfwTerminate();
}


void GLFWApp::scroll_callback(GLFWwindow* w, double x, double y) {
    if (auto* app = static_cast<GLFWApp*>(glfwGetWindowUserPointer(w)))
        app->on_scroll(x, y);
}

void GLFWApp::key_callback(GLFWwindow* w, int key, int sc, int act, int mods) {
    if (auto* app = static_cast<GLFWApp*>(glfwGetWindowUserPointer(w)))
        app->on_key_press(key, act, mods);
}

void GLFWApp::char_callback(GLFWwindow* w, unsigned int cp) {
    if (auto* app = static_cast<GLFWApp*>(glfwGetWindowUserPointer(w)))
        app->on_char(cp);
}

void GLFWApp::resize_callback(GLFWwindow* w, int width, int height) {
    if (auto* app = static_cast<GLFWApp*>(glfwGetWindowUserPointer(w)))
        app->on_resize(width, height);
}

void GLFWApp::focus_callback(GLFWwindow* w, int focused) {
#ifdef HAVE_WAYLAND
    if (auto* app = static_cast<GLFWApp*>(glfwGetWindowUserPointer(w))) {
        if (app->wayland_input) {
            focused ? app->wayland_input->focus_in()
                    : app->wayland_input->focus_out();
        }
    }
#endif
}

// ------------------------------------------------------------
// GLFWApp: Create Window
// ------------------------------------------------------------

int GLFWApp::create(int width, int height, const char* title) {
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::println(std::cerr, "ERROR::GLFW: Failed to create window");
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, this);

    // Register static callbacks
    glfwSetScrollCallback(window, &GLFWApp::scroll_callback);
    glfwSetKeyCallback(window, &GLFWApp::key_callback);
    glfwSetCharCallback(window, &GLFWApp::char_callback);
    glfwSetFramebufferSizeCallback(window, &GLFWApp::resize_callback);
    glfwSetWindowFocusCallback(window, &GLFWApp::focus_callback);

    if (glewInit() != GLEW_OK) {
        std::println(std::cerr, "ERROR::GLEW: Failed to initialize GLEW");
        return -1;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 1.f);

#ifdef HAVE_WAYLAND
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        auto* wl_display = glfwGetWaylandDisplay();
        wayland_input = std::make_unique<WaylandTextInput>(wl_display);

        if (wayland_input->is_valid()) {
            std::println("Wayland text-input initialized");

            wayland_input->set_preedit_callback(
                [this](const std::string& text, int cursor) {
                    terminal.set_preedit(text, cursor);
                });

            wayland_input->set_commit_callback(
                [this](const std::string& text) {
                    terminal.send_input(text);
                    terminal.clear_preedit();
                });

            if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
                wayland_input->focus_in();
        }
    }
#endif

    return 0;
}

// ------------------------------------------------------------
// GLFWApp: Main Loop
// ------------------------------------------------------------

void GLFWApp::mainloop() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    text_renderer = std::make_unique<TextRenderer>();
    view.set_renderer(text_renderer.get());
    view.set_window_size(width, height);

    while (!glfwWindowShouldClose(window)) {

#ifdef HAVE_WAYLAND
        if (wayland_input && wayland_input->is_valid())
            wl_display_dispatch_pending(glfwGetWaylandDisplay());
#endif

        if (terminal.poll_output().contains('\x04'))
            glfwSetWindowShouldClose(window, true);

        view.update_cursor_blink();
        view.render();

#ifdef HAVE_WAYLAND
        if (wayland_input && wayland_input->is_valid()) {
            auto cpos = view.get_cursor_pos();
            int win_w, win_h;
            glfwGetWindowSize(window, &win_w, &win_h);
            int y_wayland = win_h - int(cpos.y) - int(view.get_line_height());
            wayland_input->set_cursor_rect(
                int(cpos.x),
                y_wayland,
                int(view.get_char_width()),
                int(view.get_line_height()));
        }
#endif

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

// ------------------------------------------------------------
// GLFWApp: Event Handlers
// ------------------------------------------------------------

void GLFWApp::on_scroll(double, double y) {
    if (y > 0) {
        terminal.scroll_up();
        terminal.scroll_up();
        terminal.scroll_up();
    } else if (y < 0) {
        terminal.scroll_down();
        terminal.scroll_down();
        terminal.scroll_down();
    }
    view.render();
}

void GLFWApp::on_resize(int width, int height) {
    glViewport(0, 0, width, height);
    view.set_window_size(width, height);
    view.render();
}

void GLFWApp::on_char(unsigned int cp) {
    terminal.send_input(utf8_encode(cp));
}

void GLFWApp::on_key_press(int key, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    // Ctrl+A..Z
    if (mods & GLFW_MOD_CONTROL) {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            terminal.send_input(std::string(1, char(key - GLFW_KEY_A + 1)));
            return;
        }
        if (key == GLFW_KEY_LEFT_BRACKET) {
            terminal.send_input("\x1b");
            return;
        }
    }

    // Shift scrolling
    if (mods & GLFW_MOD_SHIFT) {
        switch (key) {
            case GLFW_KEY_UP:        terminal.scroll_up();        return;
            case GLFW_KEY_DOWN:      terminal.scroll_down();      return;
            case GLFW_KEY_PAGE_UP:   terminal.scroll_page_up();   return;
            case GLFW_KEY_PAGE_DOWN: terminal.scroll_page_down(); return;
        }
    }

    // Escape sequences
    static const std::unordered_map<int, std::string> keymap = {
        {GLFW_KEY_ENTER,     "\r"},
        {GLFW_KEY_BACKSPACE, "\x7f"},
        {GLFW_KEY_TAB,       "\t"},
        {GLFW_KEY_ESCAPE,    "\x1b"},
        {GLFW_KEY_UP,        "\x1b[A"},
        {GLFW_KEY_DOWN,      "\x1b[B"},
        {GLFW_KEY_RIGHT,     "\x1b[C"},
        {GLFW_KEY_LEFT,      "\x1b[D"},
        {GLFW_KEY_HOME,      "\x1b[H"},
        {GLFW_KEY_END,       "\x1b[F"},
        {GLFW_KEY_PAGE_UP,   "\x1b[5~"},
        {GLFW_KEY_PAGE_DOWN, "\x1b[6~"},
        {GLFW_KEY_INSERT,    "\x1b[2~"},
        {GLFW_KEY_DELETE,    "\x1b[3~"},
        {GLFW_KEY_F1,        "\x1bOP"},
        {GLFW_KEY_F2,        "\x1bOQ"},
        {GLFW_KEY_F3,        "\x1bOR"},
        {GLFW_KEY_F4,        "\x1bOS"},
        {GLFW_KEY_F5,        "\x1b[15~"},
        {GLFW_KEY_F6,        "\x1b[17~"},
        {GLFW_KEY_F7,        "\x1b[18~"},
        {GLFW_KEY_F8,        "\x1b[19~"},
        {GLFW_KEY_F9,        "\x1b[20~"},
        {GLFW_KEY_F10,       "\x1b[21~"},
        {GLFW_KEY_F11,       "\x1b[23~"},
        {GLFW_KEY_F12,       "\x1b[24~"},
    };

    if (auto it = keymap.find(key); it != keymap.end())
        terminal.send_input(it->second);
}

// ------------------------------------------------------------
// Cleanup
// ------------------------------------------------------------

void GLFWApp::cleanup() {
    std::println("Cleanup");
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
}

