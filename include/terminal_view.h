#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include "terminal.h"
#include "text_renderer.h"
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class TerminalView {
public:
  TerminalView(Terminal &terminal);
  ~TerminalView();

  void set_renderer(TextRenderer *renderer);
  void set_window_size(float width, float height);
  void render();
  void update_cursor_blink();

  // Font metrics
  float get_line_height() const { return LINE_HEIGHT; }
  float get_cell_width() const { return CELL_WIDTH; }

private:
  Terminal &terminal;
  TextRenderer *text_renderer = nullptr;

  float win_width;
  float win_height;

  float LINE_HEIGHT = 50.0f;
  float CELL_WIDTH = 15.0f;

  // Cursor state
  Coord cursor_pos;
  bool cursor_visible = true;
  double last_cursor_time = 0.0;

  // Helper methods
  void update_dimensions();
  void render_line(const ParsedLine &line, float &y_pos);
  void render_cursor(float x, float y);
  void render_preedit(float x, float y);

  // Color helpers
  void get_color(const TerminalColor &color, float *out_color,
                 bool is_bg = false);
  void get_color_for_attributes(const TerminalAttributes &attrs, float *color);
};

#endif // TERMINAL_VIEW_H
