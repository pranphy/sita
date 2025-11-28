#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <print>
#include <thread>

#include "gui.h"
// Terminal GLFWApp::terminal = Terminal(0,0);

GLFWApp::GLFWApp() : terminal(1920, 1080) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

GLFWApp::~GLFWApp() { glfwTerminate(); }

int GLFWApp::create(int width, int height, const char *title) {

  window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!window) {
    std::println(std::cerr, "ERROR::GLFW: Failed to create GLFW window");
    ;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode,
                                int action, int mods) {
    auto app = static_cast<GLFWApp *>(glfwGetWindowUserPointer(window));
    if (app) {
      app->on_key_press(key, action, mods);
    }
  });

  glfwSetCharCallback(window, [](GLFWwindow *window, unsigned int codepoint) {
    auto app = static_cast<GLFWApp *>(glfwGetWindowUserPointer(window));
    if (app) {
      app->on_char(codepoint);
    }
  });

  glfwSetFramebufferSizeCallback(
      window, [](GLFWwindow *window, int width, int height) {
        // std::println("Setting callback ");
        auto app = static_cast<GLFWApp *>(glfwGetWindowUserPointer(window));
        if (app) {
          std::println("Changed size to {}x{}", width, height);
          app->on_resize(width, height);
        } else {
          // std::println("Failed to set callback");
        }
      });

  // Initialize GLEW
  if (glewInit() != GLEW_OK) {
    std::println(std::cerr, "ERROR::GLEW: Failed to initialize GLEW");
    return -1;
  }

  // Set viewport
  glViewport(0, 0, width, height);

  // Set clear color
  glClearColor(0.0f, 0.0f, 0.1f, 1.0f);

  return 0;
}

void GLFWApp::mainloop() {
  // Use IosevkaTerm as primary font and Laila-Regular as fallback for
  // Devanagari
  // TextRenderer text_renderer;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);
  auto renderer = new TextRenderer();
  terminal.set_renderer(renderer);
  terminal.set_window_size(width, height);

  std::println("Main looping ");

  while (!glfwWindowShouldClose(window)) {
    // lets just slow things downa a bit, shall we?
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto output = terminal.poll_output();
    if (output.find('\x04') != std::string::npos) {
      glfwSetWindowShouldClose(window, true);
    }
    terminal.show_buffer();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void GLFWApp::on_resize(int width, int height) {
  glViewport(0, 0, width, height);
  terminal.set_window_size(width, height);
  // glClear(GL_COLOR_BUFFER_BIT);
  terminal.show_buffer();
  // glfwSwapBuffers(window);
}

void GLFWApp::on_char(unsigned int codepoint) {
  std::string encoded;
  if (codepoint <= 0x7F) {
    encoded += (char)codepoint;
  } else if (codepoint <= 0x7FF) {
    encoded += (char)(0xC0 | (codepoint >> 6));
    encoded += (char)(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    encoded += (char)(0xE0 | (codepoint >> 12));
    encoded += (char)(0x80 | ((codepoint >> 6) & 0x3F));
    encoded += (char)(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0x10FFFF) {
    encoded += (char)(0xF0 | (codepoint >> 18));
    encoded += (char)(0x80 | ((codepoint >> 12) & 0x3F));
    encoded += (char)(0x80 | ((codepoint >> 6) & 0x3F));
    encoded += (char)(0x80 | (codepoint & 0x3F));
  }
  terminal.send_input(encoded);
}

void GLFWApp::on_key_press(int key, int action, int mods) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return;

  // Ctrl+Key handling
  if (mods & GLFW_MOD_CONTROL) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
      char c = key - GLFW_KEY_A + 1;
      terminal.send_input(std::string(1, c));
      return;
    }
    if (key == GLFW_KEY_LEFT_BRACKET) {
      terminal.send_input("\x1b");
      return;
    }
  }

  // Special keys
  switch (key) {
  case GLFW_KEY_ENTER:
    terminal.send_input("\r");
    break;
  case GLFW_KEY_BACKSPACE:
    terminal.send_input("\x7f");
    break;
  case GLFW_KEY_TAB:
    terminal.send_input("\t");
    break;
  case GLFW_KEY_ESCAPE:
    terminal.send_input("\x1b");
    break;
  case GLFW_KEY_UP:
    terminal.send_input("\x1b[A");
    break;
  case GLFW_KEY_DOWN:
    terminal.send_input("\x1b[B");
    break;
  case GLFW_KEY_RIGHT:
    terminal.send_input("\x1b[C");
    break;
  case GLFW_KEY_LEFT:
    terminal.send_input("\x1b[D");
    break;
  case GLFW_KEY_HOME:
    terminal.send_input("\x1b[H");
    break;
  case GLFW_KEY_END:
    terminal.send_input("\x1b[F");
    break;
  case GLFW_KEY_PAGE_UP:
    terminal.send_input("\x1b[5~");
    break;
  case GLFW_KEY_PAGE_DOWN:
    terminal.send_input("\x1b[6~");
    break;
  case GLFW_KEY_INSERT:
    terminal.send_input("\x1b[2~");
    break;
  case GLFW_KEY_DELETE:
    terminal.send_input("\x1b[3~");
    break;
  case GLFW_KEY_F1:
    terminal.send_input("\x1bOP");
    break;
  case GLFW_KEY_F2:
    terminal.send_input("\x1bOQ");
    break;
  case GLFW_KEY_F3:
    terminal.send_input("\x1bOR");
    break;
  case GLFW_KEY_F4:
    terminal.send_input("\x1bOS");
    break;
  case GLFW_KEY_F5:
    terminal.send_input("\x1b[15~");
    break;
  case GLFW_KEY_F6:
    terminal.send_input("\x1b[17~");
    break;
  case GLFW_KEY_F7:
    terminal.send_input("\x1b[18~");
    break;
  case GLFW_KEY_F8:
    terminal.send_input("\x1b[19~");
    break;
  case GLFW_KEY_F9:
    terminal.send_input("\x1b[20~");
    break;
  case GLFW_KEY_F10:
    terminal.send_input("\x1b[21~");
    break;
  case GLFW_KEY_F11:
    terminal.send_input("\x1b[23~");
    break;
  case GLFW_KEY_F12:
    terminal.send_input("\x1b[24~");
    break;
  }
}

void GLFWApp::cleanup() {
  // Cleanup
  std::println("Done doing things ");
  glfwDestroyWindow(window);
  glfwTerminate();
}
