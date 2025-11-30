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

  // Terminal operations
  void set_window_size(int rows, int cols);

  // Scrollback
  void scroll_up();
  void scroll_down();
  void scroll_page_up();
  void scroll_page_down();
  void scroll_to_bottom();

  // Getters
  void key_pressed(char c, int type);
  void send_input(const std::string &input);

  std::string poll_output();
  tty term;

public:
  // Input handling state
  int scroll_offset = 0;

  // Alternate screen buffer for apps like vim
  bool alternate_screen_active = false;
  bool insert_mode = false;
  std::vector<std::vector<Cell>> screen_buffer; // 2D grid
  int screen_cursor_row = 0;
  int screen_cursor_col = 0;
  int screen_rows = 24;
  int screen_cols = 80;
  int scroll_region_top = 0;
  int scroll_region_bottom = -1; // -1 means bottom of screen
  std::string
      pending_utf8; // Buffer for incomplete UTF-8 sequences in alternate screen

  std::vector<ParsedLine> parsed_buffer;
  ParsedLine active_line; // Replaces active_raw_line for better color support
  bool last_char_was_newline = true;
  TerminalParser parser;
};

#endif // TERMINAL_H
