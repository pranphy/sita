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
    // std::println("Setting callback ");
    auto app = static_cast<GLFWApp *>(glfwGetWindowUserPointer(window));
    if (app) {
      // std::println("Set callback");
      app->on_key_press(key, action, mods);
    } else {
      // std::println("Failed to set callback");
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

void GLFWApp::on_key_press(int key, int action, int mods) {
  // Check for Ctrl+D to exit
  if (key == GLFW_KEY_D && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
    glfwSetWindowShouldClose(window, true);
    return;
  }

  // Check for backspace
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    terminal.key_pressed(' ', 32);
  } else if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS) {
    terminal.key_pressed('\b', -1);
  } else if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
    terminal.key_pressed('\n', 13);
  } else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z && action == GLFW_PRESS) {
    terminal.key_pressed((char)(key - GLFW_KEY_A + 'a'), 0);
  }
}

void GLFWApp::cleanup() {
  // Cleanup
  std::println("Done doing things ");
  glfwDestroyWindow(window);
  glfwTerminate();
}
