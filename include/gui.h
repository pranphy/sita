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
  static void init();
  int create(int width = 1920, int height = 1080, const char *title = "title");
  void mainloop();
  void cleanup();
  void on_key_press(int key, int action, int mods);
  void on_char(unsigned int codepoint);
  void on_resize(int width, int height);
  void on_scroll(double xoffset, double yoffset);

private:
  GLFWwindow *window;
  Terminal terminal;
  TerminalView view;

#ifdef HAVE_WAYLAND
  std::unique_ptr<WaylandTextInput> wayland_input;
#endif
};

#endif
