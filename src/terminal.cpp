#include "terminal.h"
#include "oglutil.h"
#include "terminal_parser.h"
#include "utils.h"
#include <GL/glew.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <print>
#include <unistd.h>

bool tty::running{true};
termios tty::old_termios;

std::vector<std::string> readlines(std::string filename,
                                   std::vector<std::string> &all_lines,
                                   unsigned int nol = 0) {
  std::string line;
  std::ifstream fileobj(filename);
  while (std::getline(fileobj, line)) {
    all_lines.push_back(std::move(line));
    if (all_lines.size() >= nol)
      break;
  }
  return all_lines;
}

Terminal::Terminal(int width, int height)
    : last_cursor_time(0.0), cursor_visible(true) {
  // Initialize key_pressed array

  win_width = width;
  win_height = height;
  cursor_pos = {25.0f, height - 50.0f};

  text_renderer = nullptr;
}

void Terminal::set_window_size(float width, float height) {
  win_width = width;
  win_height = height;

  cursor_pos = {25.0f, height - 50.0f};
  // std::println("Changed sized of terminal to {}x{}", width, height);
}

void Terminal::scroll_up() {
  scroll_offset++;
  // Clamp to max history? For now just ensure we don't go past size
  // We can scroll back as much as we have lines
  // But we need to know total lines to clamp properly, which is dynamic.
  // Let's clamp in show_buffer or just let it grow and clamp there.
  // Actually better to clamp here if possible, but total lines changes.
  // Let's just increment and handle "too far" in rendering.
}

void Terminal::scroll_down() {
  if (scroll_offset > 0) {
    scroll_offset--;
  }
}

void Terminal::scroll_page_up() {
  int lines_per_page = (win_height / 50.0f) - 1;
  scroll_offset += lines_per_page;
}

void Terminal::scroll_page_down() {
  int lines_per_page = (win_height / 50.0f) - 1;
  scroll_offset -= lines_per_page;
  if (scroll_offset < 0)
    scroll_offset = 0;
}

void Terminal::scroll_to_bottom() { scroll_offset = 0; }

Terminal::~Terminal() {
  // Cleanup if needed
  cursor_pos = {0.0f, 0.0f};
}

void Terminal::render_line(const ParsedLine &line, float &y_pos) {
  float current_x = 25.0f;
  float margin = 25.0f;

  // Render each segment of the line with its own attributes
  for (const auto &segment : line.segments) {
    float color[4];
    get_color_for_attributes(segment.attributes, color);

    // Split segment into words to handle wrapping
    // We use split_by_devanagari which returns chunks of text (words or script
    // runs)
    for (auto &chunk : utl::split_by_devanagari(segment.content)) {
      // Further split by space to ensure word wrapping works for long lines
      for (auto &word : utl::split_by_space(chunk)) {
        float word_width = text_renderer->measure_text_width(word, 1.0f);

        // Check if word fits in current line
        // If it's a space, we might not want to wrap just for it, but let's
        // keep it simple
        if (current_x + word_width > win_width - margin) {
          // Move to next line
          y_pos -= 50.0f;
          current_x = margin;
          cursor_pos.y = y_pos;

          // If the word is a space and we just wrapped, we might want to skip
          // it to avoid leading spaces on new lines. But for now, let's render
          // it. Actually, standard behavior is usually to eat the space if it
          // causes a wrap.
          if (word == " ") {
            continue;
          }
        }

        cursor_pos.x = current_x;
        cursor_pos = text_renderer->render_text_harfbuzz(
            word, cursor_pos, 1.0f, color, win_width, win_height);

        // Update current_x based on where render_text left off
        current_x = cursor_pos.x;
      }
    }
  }
  // Move to the next line (hard wrap for the end of the line)
  y_pos -= 50.0f;
  cursor_pos.y = y_pos;
  cursor_pos.x = 25.0f;
}

void Terminal::show_buffer() {
  glClear(GL_COLOR_BUFFER_BIT);
  float start_y = win_height - 50.0f;
  cursor_pos = {25.0f, start_y};

  if (text_renderer) {
    if (alternate_screen_active) {
      // Render screen buffer
      float y = win_height - 50.0f;
      for (const auto &row : screen_buffer) {
        float x = 25.0f;
        for (const auto &cell : row) {
          float color[4];
          get_color_for_attributes(cell.attributes, color);
          auto pos = text_renderer->render_text_harfbuzz(
              cell.content, {x, y}, 1.0f, color, win_width, win_height);
          x = pos.x;
        }
        y -= 50.0f;
      }
      // Update cursor pos for rendering
      cursor_pos.x = 25.0f + screen_cursor_col * 15.0f; // Approx
      cursor_pos.y = win_height - 50.0f - screen_cursor_row * 50.0f;
    } else {
      // Determine how many lines we can show
      // We want to show the last N lines of parsed_buffer + active_line

      // active_line is just ONE line (since NEWLINE pushes to parsed_buffer)
      size_t total_lines = parsed_buffer.size() + 1;
      size_t max_lines = (win_height / 50.0f) - 1; // Approximate

      // Clamp scroll_offset
      if (scroll_offset > (int)total_lines - (int)max_lines) {
        scroll_offset = (int)total_lines - (int)max_lines;
      }
      if (scroll_offset < 0)
        scroll_offset = 0;

      size_t start_index = 0;
      if (total_lines > max_lines) {
        start_index = total_lines - max_lines - scroll_offset;
      }

      // Render parsed_buffer lines
      for (size_t i = 0; i < parsed_buffer.size(); ++i) {
        if (i >= start_index && i < start_index + max_lines) {
          render_line(parsed_buffer[i], start_y);
        }
      }

      // Render active_line
      if (parsed_buffer.size() >= start_index &&
          parsed_buffer.size() < start_index + max_lines) {
        render_line(active_line, start_y);
      }
    }
  }

  // Render cursor
  if (cursor_visible) {
    render_cursor(cursor_pos.x, cursor_pos.y, 1.0f, 10,
                  20); // Default cursor size
  }
}

void Terminal::render_cursor(float x, float y, float scale, int width,
                             int height) {
  glUseProgram(0); // Unbind any active shader to use fixed-function pipeline
  glDisable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White cursor

  // Adjust cursor size if not provided
  float w = (width > 0 ? width : 10.0f) * scale;
  float h = (height > 0 ? height : 20.0f) * scale;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0f, win_width, 0.0f, win_height, -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glEnable(GL_TEXTURE_2D);
}

void Terminal::update_cursor_blink() {
  double current_time = glfwGetTime();
  if (current_time - last_cursor_time > 0.5) { // Blink every 0.5 seconds
    cursor_visible = !cursor_visible;
    last_cursor_time = current_time;
  }
}

void Terminal::set_renderer(TextRenderer *renderer) {
  text_renderer = renderer;
}

void Terminal::send_input(const std::string &input) {
  for (char c : input) {
    term.write_to_pty(c);
  }
  // No local buffering, rely on PTY echo
}

void Terminal::key_pressed(char c, int /*type*/) {
  // Deprecated, but kept for compatibility if needed
  std::string s(1, c);
  send_input(s);
}

std::string Terminal::poll_output() {
  auto result = term.handle_pty_output();
  if (!result.empty()) {
    auto actions = parser.parse_input(result);

    for (const auto &action : actions) {
      if (action.type == ActionType::SET_ALTERNATE_BUFFER) {
        alternate_screen_active = action.flag;
        if (alternate_screen_active) {
          // Initialize screen buffer if needed
          screen_rows = (int)(win_height / 50.0f);
          screen_cols = (int)(win_width / 15.0f); // Approx char width
          if (screen_buffer.empty() ||
              screen_buffer.size() != (size_t)screen_rows) {
            screen_buffer.resize(screen_rows);
            for (auto &row : screen_buffer)
              row.resize(screen_cols, Cell{" ", {}});
          }
          screen_cursor_row = 0;
          screen_cursor_col = 0;
        }
      } else if (alternate_screen_active) {
        // Handle actions in screen mode
        if (action.type == ActionType::PRINT_TEXT) {
          for (char c : action.text) {
            pending_utf8 += c;
          }

          size_t pos = 0;
          while (pos < pending_utf8.length()) {
            // Check if we have a full codepoint
            // We use a modified check because get_next_codepoint consumes bytes
            // We need to peek or just try and see if it returns valid or 0 (if
            // incomplete) But get_next_codepoint returns 0 for end of string.
            // Let's implement a simple check here or use get_next_codepoint
            // carefully.

            // Simple UTF-8 length check
            unsigned char byte = pending_utf8[pos];
            int needed = 0;
            if ((byte & 0x80) == 0)
              needed = 1;
            else if ((byte & 0xE0) == 0xC0)
              needed = 2;
            else if ((byte & 0xF0) == 0xE0)
              needed = 3;
            else if ((byte & 0xF8) == 0xF0)
              needed = 4;
            else
              needed = 1; // Invalid, treat as 1

            if (pos + needed > pending_utf8.length()) {
              // Incomplete, stop processing and keep remaining in buffer
              break;
            }

            std::string char_str = pending_utf8.substr(pos, needed);
            pos += needed;

            if (screen_cursor_row < screen_rows &&
                screen_cursor_col < screen_cols) {
              screen_buffer[screen_cursor_row][screen_cursor_col] =
                  Cell{char_str, action.attributes};
              screen_cursor_col++;
              if (screen_cursor_col >= screen_cols) {
                screen_cursor_col = 0;
                screen_cursor_row++;
              }
            }
          }

          if (pos > 0) {
            pending_utf8.erase(0, pos);
          }
        } else if (action.type == ActionType::NEWLINE) {
          screen_cursor_row++;
          int bottom = (scroll_region_bottom == -1) ? screen_rows - 1
                                                    : scroll_region_bottom;
          int top = scroll_region_top;

          // Ensure bounds
          if (bottom >= (int)screen_buffer.size())
            bottom = (int)screen_buffer.size() - 1;
          if (top < 0)
            top = 0;
          if (top > bottom)
            top = bottom;

          if (screen_cursor_row > bottom) {
            // Scroll up within region
            screen_cursor_row = bottom;
            if (top < (int)screen_buffer.size() &&
                bottom < (int)screen_buffer.size()) {
              screen_buffer.erase(screen_buffer.begin() + top);
              screen_buffer.insert(
                  screen_buffer.begin() + bottom,
                  std::vector<Cell>(screen_cols, Cell{" ", {}}));
            }
          }
        } else if (action.type == ActionType::CARRIAGE_RETURN) {
          screen_cursor_col = 0;
        } else if (action.type == ActionType::BACKSPACE) {
          if (screen_cursor_col > 0)
            screen_cursor_col--;
        } else if (action.type == ActionType::MOVE_CURSOR) {
          if (action.flag) { // Absolute position (1-based)
            screen_cursor_row = action.row - 1;
            screen_cursor_col = action.col - 1;
          } else { // Relative
            screen_cursor_row += action.row;
            screen_cursor_col += action.col;
          }
          // Clamp
          if (screen_cursor_row < 0)
            screen_cursor_row = 0;
          if (screen_cursor_row >= screen_rows)
            screen_cursor_row = screen_rows - 1;
          if (screen_cursor_col < 0)
            screen_cursor_col = 0;
          if (screen_cursor_col >= screen_cols)
            screen_cursor_col = screen_cols - 1;
        } else if (action.type == ActionType::CLEAR_SCREEN) {
          for (auto &row : screen_buffer)
            std::fill(row.begin(), row.end(), Cell{" ", {}});
          screen_cursor_row = 0;
          screen_cursor_col = 0;
        } else if (action.type == ActionType::CLEAR_LINE) {
          if (screen_cursor_row < (int)screen_buffer.size()) {
            // Clear from cursor to end of line
            auto &row = screen_buffer[screen_cursor_row];
            for (int i = screen_cursor_col; i < (int)row.size(); ++i) {
              row[i] = Cell{" ", {}};
            }
          }
        } else if (action.type == ActionType::INSERT_LINE) {
          int bottom = (scroll_region_bottom == -1) ? screen_rows - 1
                                                    : scroll_region_bottom;
          int top = scroll_region_top;

          if (bottom >= (int)screen_buffer.size())
            bottom = (int)screen_buffer.size() - 1;
          if (top < 0)
            top = 0;

          // IL only works if cursor is within scrolling region
          if (screen_cursor_row >= top && screen_cursor_row <= bottom) {
            for (int i = 0; i < action.row; ++i) {
              if (bottom < (int)screen_buffer.size()) {
                screen_buffer.erase(screen_buffer.begin() + bottom);
                screen_buffer.insert(
                    screen_buffer.begin() + screen_cursor_row,
                    std::vector<Cell>(screen_cols, Cell{" ", {}}));
              }
            }
          }
        } else if (action.type == ActionType::DELETE_LINE) {
          int bottom = (scroll_region_bottom == -1) ? screen_rows - 1
                                                    : scroll_region_bottom;
          int top = scroll_region_top;

          if (bottom >= (int)screen_buffer.size())
            bottom = (int)screen_buffer.size() - 1;
          if (top < 0)
            top = 0;

          if (screen_cursor_row >= top && screen_cursor_row <= bottom) {
            for (int i = 0; i < action.row; ++i) {
              if (screen_cursor_row < (int)screen_buffer.size()) {
                screen_buffer.erase(screen_buffer.begin() + screen_cursor_row);
                screen_buffer.insert(
                    screen_buffer.begin() + bottom,
                    std::vector<Cell>(screen_cols, Cell{" ", {}}));
              }
            }
          }
        } else if (action.type == ActionType::INSERT_CHAR) {
          if (screen_cursor_row < (int)screen_buffer.size()) {
            auto &row = screen_buffer[screen_cursor_row];
            for (int i = 0; i < action.row; ++i) {
              if (screen_cursor_col <=
                  (int)row.size()) { // Allow inserting at end
                row.insert(row.begin() + screen_cursor_col, Cell{" ", {}});
                if (row.size() > (size_t)screen_cols) {
                  row.pop_back();
                }
              }
            }
          }
        } else if (action.type == ActionType::DELETE_CHAR) {
          if (screen_cursor_row < (int)screen_buffer.size()) {
            auto &row = screen_buffer[screen_cursor_row];
            for (int i = 0; i < action.row; ++i) {
              if (screen_cursor_col < (int)row.size()) {
                row.erase(row.begin() + screen_cursor_col);
                row.push_back(Cell{" ", {}});
              }
            }
          }
        } else if (action.type == ActionType::SET_SCROLL_REGION) {
          scroll_region_top = action.row - 1;
          scroll_region_bottom = action.col - 1;
          if (scroll_region_top < 0)
            scroll_region_top = 0;
          if (scroll_region_bottom < 0)
            scroll_region_bottom = -1;
          screen_cursor_row = 0; // Usually resets cursor to home
          screen_cursor_col = 0;
        } else if (action.type == ActionType::REPORT_CURSOR_POSITION) {
          std::string response = "\033[" +
                                 std::to_string(screen_cursor_row + 1) + ";" +
                                 std::to_string(screen_cursor_col + 1) + "R";
          send_input(response);
        } else if (action.type == ActionType::REPORT_DEVICE_STATUS) {
          send_input("\033[0n");
        }
      } else {
        // Normal mode (History)
        if (action.type == ActionType::PRINT_TEXT) {
          if (active_line.segments.empty()) {
            active_line.segments.push_back({action.text, action.attributes});
          } else {
            auto &last = active_line.segments.back();
            // Compare attributes (simple comparison)
            bool same_attrs =
                (last.attributes.foreground == action.attributes.foreground &&
                 last.attributes.background == action.attributes.background &&
                 last.attributes.bold ==
                     action.attributes.bold); // Add others if needed
            if (same_attrs) {
              last.content += action.text;
            } else {
              active_line.segments.push_back({action.text, action.attributes});
            }
          }
        } else if (action.type == ActionType::NEWLINE) {
          // Push current line to history
          parsed_buffer.push_back(active_line);
          active_line = ParsedLine(); // Reset
          scroll_offset = 0;
        } else if (action.type == ActionType::CLEAR_SCREEN) {
          parsed_buffer.clear();
          scroll_offset = 0;
          active_line = ParsedLine();
        } else if (action.type == ActionType::CARRIAGE_RETURN) {
          // For now, ignore or maybe we should clear line?
          // In simple terminal, CR just moves cursor to start.
          // But we are appending.
          // If we want to support overwrite, we need a cursor index in
          // active_line. For now, let's just append \r if we were storing raw
          // string, but here we are storing segments. Let's ignore CR for now
          // as it complicates append-only model.
        } else if (action.type == ActionType::BACKSPACE) {
          if (!active_line.segments.empty()) {
            auto &last = active_line.segments.back();
            if (!last.content.empty()) {
              last.content.pop_back();
              if (last.content.empty()) {
                active_line.segments.pop_back();
              }
            }
          }
        }
      }
    }
  }
  return result;
}

void Terminal::get_color_for_line_type(LineType type, float *color) {
  switch (type) {
  case LineType::PROMPT:
    color[0] = 0.0f; // R
    color[1] = 1.0f; // G
    color[2] = 0.0f; // B
    color[3] = 1.0f; // A
    break;
  case LineType::COMMAND_OUTPUT:
    color[0] = 1.0f; // R
    color[1] = 1.0f; // G
    color[2] = 1.0f; // B
    color[3] = 1.0f; // A
    break;
  case LineType::ERROR_OUTPUT:
    color[0] = 1.0f; // R
    color[1] = 0.0f; // G
    color[2] = 0.0f; // B
    color[3] = 1.0f; // A
    break;
  case LineType::USER_INPUT:
    color[0] = 0.2f; // R
    color[1] = 1.0f; // G
    color[2] = 1.0f; // B
    color[3] = 1.0f; // A
    break;
  default:
    color[0] = 0.8f; // R
    color[1] = 0.8f; // G
    color[2] = 0.8f; // B
    color[3] = 1.0f; // A
    break;
  }
}

void Terminal::get_color_for_attributes(const TerminalAttributes &attrs,
                                        float *color) {
  // Convert ANSI colors to RGB
  AnsiColor fg = attrs.foreground;

  // If bold is set and color is a standard color (0-7), switch to bright
  // version (8-15)
  if (attrs.bold) {
    switch (fg) {
    case AnsiColor::BLACK:
      fg = AnsiColor::BRIGHT_BLACK;
      break;
    case AnsiColor::RED:
      fg = AnsiColor::BRIGHT_RED;
      break;
    case AnsiColor::GREEN:
      fg = AnsiColor::BRIGHT_GREEN;
      break;
    case AnsiColor::YELLOW:
      fg = AnsiColor::BRIGHT_YELLOW;
      break;
    case AnsiColor::BLUE:
      fg = AnsiColor::BRIGHT_BLUE;
      break;
    case AnsiColor::MAGENTA:
      fg = AnsiColor::BRIGHT_MAGENTA;
      break;
    case AnsiColor::CYAN:
      fg = AnsiColor::BRIGHT_CYAN;
      break;
    case AnsiColor::WHITE:
      fg = AnsiColor::BRIGHT_WHITE;
      break;
    default:
      break;
    }
  }

  switch (fg) {
  case AnsiColor::BLACK:
    color[0] = 0.0f;
    color[1] = 0.0f;
    color[2] = 0.0f;
    break;
  case AnsiColor::RED:
    color[0] = 1.0f;
    color[1] = 0.0f;
    color[2] = 0.0f;
    break;
  case AnsiColor::GREEN:
    color[0] = 0.0f;
    color[1] = 1.0f;
    color[2] = 0.0f;
    break;
  case AnsiColor::YELLOW:
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 0.0f;
    break;
  case AnsiColor::BLUE:
    color[0] = 0.0f;
    color[1] = 0.0f;
    color[2] = 1.0f;
    break;
  case AnsiColor::MAGENTA:
    color[0] = 1.0f;
    color[1] = 0.0f;
    color[2] = 1.0f;
    break;
  case AnsiColor::CYAN:
    color[0] = 0.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    break;
  case AnsiColor::WHITE:
    color[0] = 0.8f;
    color[1] = 0.8f;
    color[2] = 0.8f; // Dim white for standard
    break;
  case AnsiColor::BRIGHT_BLACK: // Gray
    color[0] = 0.5f;
    color[1] = 0.5f;
    color[2] = 0.5f;
    break;
  case AnsiColor::BRIGHT_RED:
    color[0] = 1.0f;
    color[1] = 0.5f;
    color[2] = 0.5f;
    break;
  case AnsiColor::BRIGHT_GREEN:
    color[0] = 0.5f;
    color[1] = 1.0f;
    color[2] = 0.5f;
    break;
  case AnsiColor::BRIGHT_YELLOW:
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 0.5f;
    break;
  case AnsiColor::BRIGHT_BLUE:
    color[0] = 0.5f;
    color[1] = 0.5f;
    color[2] = 1.0f;
    break;
  case AnsiColor::BRIGHT_MAGENTA:
    color[0] = 1.0f;
    color[1] = 0.5f;
    color[2] = 1.0f;
    break;
  case AnsiColor::BRIGHT_CYAN:
    color[0] = 0.5f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    break;
  case AnsiColor::BRIGHT_WHITE:
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    break;
  default:
    color[0] = 1.0f;
  }
}
