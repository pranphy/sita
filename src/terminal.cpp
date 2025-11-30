#include "terminal.h"
#include "terminal_parser.h"
#include "utils.h"
#include <unistd.h>

bool tty::running{true};
termios tty::old_termios;

Terminal::Terminal(int width, int height) {
  // Initialize key_pressed array
  screen_rows = height; // Assuming these are rows/cols passed in
  screen_cols = width;

  // Initialize screen buffer
  screen_buffer.resize(screen_rows);
  for (auto &row : screen_buffer) {
    row.resize(screen_cols, Cell{" ", {}});
  }
}

void Terminal::set_window_size(int rows, int cols) {
  screen_rows = rows;
  screen_cols = cols;
  term.set_window_size(rows, cols);

  // Resize screen buffer if active (or just always to be safe/ready)
  screen_buffer.resize(screen_rows);
  for (auto &row : screen_buffer) {
    row.resize(screen_cols, Cell{" ", {}});
  }
}

void Terminal::scroll_up() {
  if (scroll_offset < (int)parsed_buffer.size()) {
    scroll_offset++;
  }
}

void Terminal::scroll_down() {
  if (scroll_offset > 0) {
    scroll_offset--;
  }
}

void Terminal::scroll_page_up() {
  scroll_offset += screen_rows;
  if (scroll_offset > (int)parsed_buffer.size()) {
    scroll_offset = (int)parsed_buffer.size();
  }
}

void Terminal::scroll_page_down() {
  scroll_offset -= screen_rows;
  if (scroll_offset < 0) {
    scroll_offset = 0;
  }
}

void Terminal::scroll_to_bottom() { scroll_offset = 0; }

Terminal::~Terminal() {
  // Cleanup if needed
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
          // screen_rows/cols are already set
          if (screen_buffer.empty() ||
              screen_buffer.size() != (size_t)screen_rows) {
            screen_buffer.resize(screen_rows);
            for (auto &row : screen_buffer)
              row.resize(screen_cols, Cell{" ", {}});
          }
          screen_cursor_row = 0;
          screen_cursor_col = 0;
        }
      } else if (action.type == ActionType::SET_INSERT_MODE) {
        insert_mode = action.flag;
      } else if (alternate_screen_active) {
        // Handle actions in screen mode
        if (action.type == ActionType::PRINT_TEXT) {
          for (char c : action.text) {
            pending_utf8 += c;
          }

          size_t pos = 0;
          while (pos < pending_utf8.length()) {
            // Check if we have a full codepoint
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
              if (insert_mode) {
                // Insert mode: shift characters right
                if (screen_cursor_row < (int)screen_buffer.size()) {
                  auto &row = screen_buffer[screen_cursor_row];
                  if (screen_cursor_col < (int)row.size()) {
                    row.insert(row.begin() + screen_cursor_col,
                               Cell{char_str, action.attributes});
                    if (row.size() > (size_t)screen_cols) {
                      row.pop_back();
                    }
                  } else {
                    // Just append if at end
                    screen_buffer[screen_cursor_row][screen_cursor_col] =
                        Cell{char_str, action.attributes};
                  }
                }
              } else {
                // Overwrite mode
                screen_buffer[screen_cursor_row][screen_cursor_col] =
                    Cell{char_str, action.attributes};
              }
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
          int mode = action.row;
          if (mode == 2 || mode == 3) {
            for (auto &row : screen_buffer)
              std::fill(row.begin(), row.end(), Cell{" ", action.attributes});
            // Don't reset cursor for J2/J3, usually followed by H if needed
            // But if it was a full clear, maybe we should?
            // Let's trust the sequence.
            if (mode == 3) {
              // Clear scrollback too (not implemented yet for alternate
              // screen)
            }
          } else if (mode == 0) {
            // Cursor to end
            if (screen_cursor_row < (int)screen_buffer.size()) {
              auto &row = screen_buffer[screen_cursor_row];
              for (int i = screen_cursor_col; i < (int)row.size(); ++i)
                row[i] = Cell{" ", action.attributes};
            }
            for (int i = screen_cursor_row + 1; i < (int)screen_buffer.size();
                 ++i) {
              std::fill(screen_buffer[i].begin(), screen_buffer[i].end(),
                        Cell{" ", action.attributes});
            }
          } else if (mode == 1) {
            // Start to cursor
            for (int i = 0; i < screen_cursor_row; ++i) {
              std::fill(screen_buffer[i].begin(), screen_buffer[i].end(),
                        Cell{" ", action.attributes});
            }
            if (screen_cursor_row < (int)screen_buffer.size()) {
              auto &row = screen_buffer[screen_cursor_row];
              for (int i = 0; i <= screen_cursor_col && i < (int)row.size();
                   ++i)
                row[i] = Cell{" ", action.attributes};
            }
          }
        } else if (action.type == ActionType::CLEAR_LINE) {
          int mode = action.row;
          if (screen_cursor_row < (int)screen_buffer.size()) {
            auto &row = screen_buffer[screen_cursor_row];
            if (mode == 0) {
              for (int i = screen_cursor_col; i < (int)row.size(); ++i)
                row[i] = Cell{" ", action.attributes};
            } else if (mode == 1) {
              for (int i = 0; i <= screen_cursor_col && i < (int)row.size();
                   ++i)
                row[i] = Cell{" ", action.attributes};
            } else if (mode == 2) {
              std::fill(row.begin(), row.end(), Cell{" ", action.attributes});
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
                    std::vector<Cell>(screen_cols,
                                      Cell{" ", action.attributes}));
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
                    std::vector<Cell>(screen_cols,
                                      Cell{" ", action.attributes}));
              }
            }
          }
        } else if (action.type == ActionType::INSERT_CHAR) {
          if (screen_cursor_row < (int)screen_buffer.size()) {
            auto &row = screen_buffer[screen_cursor_row];
            for (int i = 0; i < action.row; ++i) {
              if (screen_cursor_col <=
                  (int)row.size()) { // Allow inserting at end
                row.insert(row.begin() + screen_cursor_col,
                           Cell{" ", action.attributes});
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
                row.push_back(Cell{" ", action.attributes});
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
        } else if (action.type == ActionType::TAB) {
          int tab_width = 8;
          screen_cursor_col = (screen_cursor_col / tab_width + 1) * tab_width;
          if (screen_cursor_col >= screen_cols) {
            screen_cursor_col = screen_cols - 1;
          }
        } else if (action.type == ActionType::ERASE_CHAR) {
          if (screen_cursor_row < (int)screen_buffer.size()) {
            auto &row = screen_buffer[screen_cursor_row];
            for (int i = 0; i < action.row; ++i) {
              if (screen_cursor_col + i < (int)row.size()) {
                row[screen_cursor_col + i] = Cell{" ", action.attributes};
              }
            }
          }
        }
      } else {
        // Normal mode (History)
        if (action.type == ActionType::PRINT_TEXT) {
          // std::println("Action: PRINT_TEXT '{}'", action.text);
          if (active_line.segments.empty()) {
            active_line.segments.push_back({action.text, action.attributes});
          } else {
            auto &last = active_line.segments.back();
            // Compare attributes (simple comparison)
            bool same_attrs =
                (last.attributes.foreground.type ==
                     action.attributes.foreground.type &&
                 (last.attributes.foreground.type !=
                      TerminalColor::Type::ANSI ||
                  last.attributes.foreground.ansi_color ==
                      action.attributes.foreground.ansi_color) &&
                 (last.attributes.foreground.type !=
                      TerminalColor::Type::INDEXED ||
                  last.attributes.foreground.indexed_color ==
                      action.attributes.foreground.indexed_color) &&
                 (last.attributes.foreground.type != TerminalColor::Type::RGB ||
                  (last.attributes.foreground.r ==
                       action.attributes.foreground.r &&
                   last.attributes.foreground.g ==
                       action.attributes.foreground.g &&
                   last.attributes.foreground.b ==
                       action.attributes.foreground.b)) &&

                 last.attributes.background.type ==
                     action.attributes.background.type &&
                 (last.attributes.background.type !=
                      TerminalColor::Type::ANSI ||
                  last.attributes.background.ansi_color ==
                      action.attributes.background.ansi_color) &&
                 (last.attributes.background.type !=
                      TerminalColor::Type::INDEXED ||
                  last.attributes.background.indexed_color ==
                      action.attributes.background.indexed_color) &&
                 (last.attributes.background.type != TerminalColor::Type::RGB ||
                  (last.attributes.background.r ==
                       action.attributes.background.r &&
                   last.attributes.background.g ==
                       action.attributes.background.g &&
                   last.attributes.background.b ==
                       action.attributes.background.b)) &&

                 last.attributes.bold == action.attributes.bold);
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
          // Handle CR
          // active_line = ParsedLine(); // DISABLED: This deletes output like
          // 'ls' which sends line\r\n
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
