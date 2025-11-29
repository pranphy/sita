#ifndef TERMINAL_H
#define TERMINAL_H

#include "terminal_parser.h"
#include "text_renderer.h"
#include "tty.h"
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>

// Terminal state management
class Terminal {
public:
  Terminal(int width, int height);
  ~Terminal();
  void render_cursor(float x, float y, float scale, int width = 0,
                     int height = 0);

  // Terminal operations
  void set_renderer(TextRenderer *renderer);
  void render_line(const ParsedLine &line, float &y_pos);
  void set_window_size(float width, float height);

  // Scrollback
  void scroll_up();
  void scroll_down();
  void scroll_page_up();
  void scroll_page_down();
  void scroll_to_bottom();

  // Getters
  void show_buffer();
  void key_pressed(char c, int type);
  void send_input(const std::string &input);

  std::string poll_output();
  tty term;

public:
  float win_width;
  float win_height;

  // Input handling state
  double last_cursor_time;
  bool cursor_visible;
  int scroll_offset = 0;

  // Helper functions
  void update_cursor_blink();

  TextRenderer *text_renderer;
  std::vector<ParsedLine> parsed_buffer;

  std::string active_raw_line;
  bool last_char_was_newline = true;
  Coord cursor_pos;
  TerminalParser parser;

  // Color management
  void get_color_for_line_type(LineType type, float *color);
  void get_color_for_attributes(const TerminalAttributes &attrs, float *color);
};

#endif // TERMINAL_H
