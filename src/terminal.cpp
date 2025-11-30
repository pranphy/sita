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
  cursor_pos = {25.0f, height - LINE_HEIGHT};

  text_renderer = nullptr;
}

void Terminal::set_window_size(float width, float height) {
  win_width = width;
  win_height = height;

  screen_rows = (int)(height / LINE_HEIGHT);
  screen_cols = (int)(width / CELL_WIDTH);
  term.set_window_size(screen_rows, screen_cols);

  cursor_pos = {25.0f, height - LINE_HEIGHT};

  // Resize screen buffer if active (or just always to be safe/ready)
  // We need to ensure it matches screen_rows/cols to avoid out of bounds access
  // when shell sends cursor movements for the new size.
  screen_buffer.resize(screen_rows);
  for (auto &row : screen_buffer) {
    row.resize(screen_cols, Cell{" ", {}});
  }

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
  int lines_per_page = (win_height / LINE_HEIGHT) - 1;
  scroll_offset += lines_per_page;
}

void Terminal::scroll_page_down() {
  int lines_per_page = (win_height / LINE_HEIGHT) - 1;
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
  float start_x = 25.0f;
  int col_index = 0;

  // Render each segment of the line with its own attributes
  for (const auto &segment : line.segments) {
    // Visual FG
    float color[4];
    if (segment.attributes.reverse) {
      get_color(segment.attributes.background, color,
                true); // Use BG definition
    } else {
      get_color_for_attributes(segment.attributes,
                               color); // Normal FG (handles Bold)
    }

    // Visual BG
    float bg_color[4];
    if (segment.attributes.reverse) {
      get_color(segment.attributes.foreground, bg_color,
                false); // Use FG definition
    } else {
      get_color(segment.attributes.background, bg_color, true); // Normal BG
    }

    // Split segment into words/chunks
    for (auto &chunk : utl::split_by_devanagari(segment.content)) {
      // Let's iterate codepoints to track col_index.
      size_t pos = 0;
      std::string current_run;
      int run_start_col = col_index;

      while (pos < chunk.length()) {
        size_t prev_pos = pos;
        unsigned int cp = utl::get_next_codepoint(chunk, pos);
        (void)cp; // Suppress unused variable warning
        std::string char_str = chunk.substr(prev_pos, pos - prev_pos);

        if (col_index >= screen_cols) {
          // Flush current run
          if (!current_run.empty()) {
            float x = start_x + run_start_col * CELL_WIDTH;
            (void)x; // Suppress unused variable warning
            float w = (col_index - run_start_col) * CELL_WIDTH;
            float baseline_offset = LINE_HEIGHT * 0.25f;
            text_renderer->draw_solid_rectangle(
                x, y_pos, w, LINE_HEIGHT, bg_color, win_width, win_height);
            text_renderer->render_text_harfbuzz(
                current_run, {x, y_pos + baseline_offset}, 1.0f, color,
                win_width, win_height);
            current_run.clear();
          }

          y_pos -= LINE_HEIGHT;
          col_index = 0;
          run_start_col = 0;
        }

        current_run += char_str;
        col_index++;
      }

      // Flush remaining
      if (!current_run.empty()) {
        float x = start_x + run_start_col * CELL_WIDTH;
        float w = (col_index - run_start_col) * CELL_WIDTH;
        float baseline_offset = LINE_HEIGHT * 0.25f;
        text_renderer->draw_solid_rectangle(x, y_pos, w, LINE_HEIGHT, bg_color,
                                            win_width, win_height);
        text_renderer->render_text_harfbuzz(current_run,
                                            {x, y_pos + baseline_offset}, 1.0f,
                                            color, win_width, win_height);
      }
    }
  }
  // Move to the next line (hard wrap for the end of the line)
  y_pos -= LINE_HEIGHT;
}

void Terminal::show_buffer() {
  glClear(GL_COLOR_BUFFER_BIT);
  float start_y = win_height - LINE_HEIGHT;
  cursor_pos = {25.0f, start_y};

  if (text_renderer) {
    if (alternate_screen_active) {
      // Render screen buffer
      float y = win_height - LINE_HEIGHT;
      for (const auto &row : screen_buffer) {
        // Group contiguous cells with same attributes
        if (row.empty()) {
          y -= LINE_HEIGHT;
          continue;
        }

        std::string current_segment_text;
        TerminalAttributes current_attrs = row[0].attributes;
        int segment_start_col = 0;

        for (size_t i = 0; i < row.size(); ++i) {
          const auto &cell = row[i];
          // Check if attributes match (simple check)
          bool attrs_match =
              (cell.attributes.foreground.type ==
                   current_attrs.foreground.type &&
               (cell.attributes.foreground.type != TerminalColor::Type::ANSI ||
                cell.attributes.foreground.ansi_color ==
                    current_attrs.foreground.ansi_color) &&
               (cell.attributes.foreground.type !=
                    TerminalColor::Type::INDEXED ||
                cell.attributes.foreground.indexed_color ==
                    current_attrs.foreground.indexed_color) &&
               (cell.attributes.foreground.type != TerminalColor::Type::RGB ||
                (cell.attributes.foreground.r == current_attrs.foreground.r &&
                 cell.attributes.foreground.g == current_attrs.foreground.g &&
                 cell.attributes.foreground.b == current_attrs.foreground.b)) &&

               cell.attributes.background.type ==
                   current_attrs.background.type &&
               (cell.attributes.background.type != TerminalColor::Type::ANSI ||
                cell.attributes.background.ansi_color ==
                    current_attrs.background.ansi_color) &&
               (cell.attributes.background.type !=
                    TerminalColor::Type::INDEXED ||
                cell.attributes.background.indexed_color ==
                    current_attrs.background.indexed_color) &&
               (cell.attributes.background.type != TerminalColor::Type::RGB ||
                (cell.attributes.background.r == current_attrs.background.r &&
                 cell.attributes.background.g == current_attrs.background.g &&
                 cell.attributes.background.b == current_attrs.background.b)) &&

               cell.attributes.bold == current_attrs.bold &&
               cell.attributes.italic == current_attrs.italic &&
               cell.attributes.underline == current_attrs.underline &&
               cell.attributes.blink == current_attrs.blink &&
               cell.attributes.reverse == current_attrs.reverse &&
               cell.attributes.strikethrough == current_attrs.strikethrough);

          if (attrs_match) {
            current_segment_text += cell.content;
          } else {
            // Render previous segment
            if (!current_segment_text.empty()) {

              // Calculate x based on start column to enforce grid alignment
              float current_x = 25.0f + segment_start_col * CELL_WIDTH;

              // Render background rectangle
              glDisable(GL_TEXTURE_2D);
              float bg_color[4];
              if (current_attrs.reverse) {
                get_color(current_attrs.foreground, bg_color,
                          true); // Use FG as BG
              } else {
                get_color(current_attrs.background, bg_color,
                          true); // Normal BG
              }

              float bg_w = (i - segment_start_col) * CELL_WIDTH;
              float bg_h = LINE_HEIGHT;

              text_renderer->draw_solid_rectangle(
                  current_x, y, bg_w, bg_h, bg_color, win_width, win_height);

              // Get FG color
              float fg_color[4];
              if (current_attrs.reverse) {
                get_color(current_attrs.background, fg_color,
                          false); // Use BG as FG
              } else {
                get_color_for_attributes(current_attrs,
                                         fg_color); // Normal FG (handles bold)
              }

              // Split by script to ensure correct font selection
              float baseline_offset =
                  LINE_HEIGHT * 0.25f; // Lift text up from bottom
              for (const auto &chunk :
                   utl::split_by_devanagari(current_segment_text)) {
                size_t temp_pos = 0;
                unsigned int first_cp =
                    utl::get_next_codepoint(chunk, temp_pos);
                if (utl::is_devanagari(first_cp)) {
                  // Devanagari: Render as chunk (trust HarfBuzz for complex
                  // script)
                  auto pos = text_renderer->render_text_harfbuzz(
                      chunk, {current_x, y + baseline_offset}, 1.0f, fg_color,
                      win_width, win_height);
                  current_x = pos.x;
                } else {
                  // ASCII/Latin: Render char by char to enforce grid alignment
                  size_t pos = 0;
                  while (pos < chunk.length()) {
                    size_t prev_pos = pos;
                    utl::get_next_codepoint(chunk, pos);
                    std::string char_str =
                        chunk.substr(prev_pos, pos - prev_pos);
                    text_renderer->render_text_harfbuzz(
                        char_str, {current_x, y + baseline_offset}, 1.0f,
                        fg_color, win_width, win_height);
                    current_x += CELL_WIDTH;
                  }
                }
              }
            }
            // Start new segment
            current_segment_text = cell.content;
            current_attrs = cell.attributes;
            segment_start_col = i;
          }
        }
        // Render last segment
        if (!current_segment_text.empty()) {
          float current_x = 25.0f + segment_start_col * CELL_WIDTH;

          float bg_color[4];
          if (current_attrs.reverse) {
            get_color(current_attrs.foreground, bg_color, true);
          } else {
            get_color(current_attrs.background, bg_color, true);
          }

          float bg_w =
              (row.size() - segment_start_col) * CELL_WIDTH; // Approximation
          float bg_h = LINE_HEIGHT;
          text_renderer->draw_solid_rectangle(current_x, y, bg_w, bg_h,
                                              bg_color, win_width, win_height);

          float fg_color[4];
          if (current_attrs.reverse) {
            get_color(current_attrs.background, fg_color, false);
          } else {
            get_color_for_attributes(current_attrs, fg_color);
          }

          for (const auto &chunk :
               utl::split_by_devanagari(current_segment_text)) {
            float baseline_offset = LINE_HEIGHT * 0.25f;
            size_t temp_pos = 0;
            unsigned int first_cp = utl::get_next_codepoint(chunk, temp_pos);
            if (utl::is_devanagari(first_cp)) {
              auto pos = text_renderer->render_text_harfbuzz(
                  chunk, {current_x, y + baseline_offset}, 1.0f, fg_color,
                  win_width, win_height);
              current_x = pos.x;
            } else {
              size_t pos = 0;
              while (pos < chunk.length()) {
                size_t prev_pos = pos;
                utl::get_next_codepoint(chunk, pos);
                std::string char_str = chunk.substr(prev_pos, pos - prev_pos);
                text_renderer->render_text_harfbuzz(
                    char_str, {current_x, y + baseline_offset}, 1.0f, fg_color,
                    win_width, win_height);
                current_x += CELL_WIDTH;
              }
            }
          }
        }
        y -= LINE_HEIGHT;
      }
      // Update cursor pos for rendering
      cursor_pos.x = 25.0f + screen_cursor_col * CELL_WIDTH; // Approx
      cursor_pos.y = win_height - LINE_HEIGHT - screen_cursor_row * LINE_HEIGHT;
    } else {
      // Determine how many lines we can show
      // We want to show the last N lines of parsed_buffer + active_line

      // active_line is just ONE line (since NEWLINE pushes to parsed_buffer)
      size_t total_lines = parsed_buffer.size() + 1;
      size_t max_lines = (win_height / LINE_HEIGHT) - 1; // Approximate

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
        float current_line_y =
            start_y; // Capture Y before render_line modifies it
        render_line(active_line, start_y);

        // Calculate cursor position for Normal Mode
        float cursor_x = 25.0f;
        for (const auto &segment : active_line.segments) {
          for (const auto &chunk : utl::split_by_devanagari(segment.content)) {
            size_t temp_pos = 0;
            unsigned int first_cp = utl::get_next_codepoint(chunk, temp_pos);
            if (utl::is_devanagari(first_cp)) {
              cursor_x += text_renderer->measure_text_width(chunk, 1.0f);
            } else {
              // ASCII/Latin: Grid alignment
              size_t len = 0;
              size_t pos = 0;
              while (pos < chunk.length()) {
                utl::get_next_codepoint(chunk, pos);
                len++;
              }
              cursor_x += len * CELL_WIDTH;
            }
          }
        }
        cursor_pos.x = cursor_x;
        cursor_pos.y = current_line_y;
      }
    }
  }

  // Render cursor
  if (cursor_visible) {
    render_cursor(cursor_pos.x, cursor_pos.y, 1.0f, 10,
                  20); // Default cursor size
  }
}

void Terminal::render_cursor(float x, float y, float /*scale*/, int /*width*/,
                             int /*height*/) {
  if (text_renderer) {
    // Use a white color for the cursor
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // White cursor

    // Invert color if we are on top of text?
    // For now simple block cursor is fine.
    // Use CELL_WIDTH and LINE_HEIGHT for cursor size to match grid

    text_renderer->draw_solid_rectangle(x, y, CELL_WIDTH, LINE_HEIGHT, color,
                                        win_width, win_height);
  }
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
  update_dimensions();
}

void Terminal::update_dimensions() {
  if (text_renderer) {
    CELL_WIDTH = text_renderer->get_char_width();
    LINE_HEIGHT = text_renderer->get_line_height();

    if (CELL_WIDTH < 1.0f)
      CELL_WIDTH = 15.0f; // Fallback

    std::println("Updated dimensions: CELL_WIDTH={}, LINE_HEIGHT={}",
                 CELL_WIDTH, LINE_HEIGHT);

    // Recalculate screen size
    set_window_size(win_width, win_height);
    std::println("Screen size: {} rows, {} cols", screen_rows, screen_cols);
  }
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
          screen_rows = (int)(win_height / LINE_HEIGHT);
          screen_cols = (int)(win_width / CELL_WIDTH); // Approx char width
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
            // We use a modified check because get_next_codepoint consumes
            // bytes We need to peek or just try and see if it returns valid
            // or 0 (if incomplete) But get_next_codepoint returns 0 for end
            // of string. Let's implement a simple check here or use
            // get_next_codepoint carefully.

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

void Terminal::get_color(const TerminalColor &tc, float *color, bool is_bg) {
  // Initialize alpha
  color[3] = 1.0f;

  if (tc.type == TerminalColor::Type::DEFAULT) {
    if (is_bg) {
      // Default Background: Black
      color[0] = 0.0f;
      color[1] = 0.0f;
      color[2] = 0.0f;
    } else {
      // Default Foreground: White (or Light Gray)
      color[0] = 0.8f;
      color[1] = 0.8f;
      color[2] = 0.8f;
    }
    return;
  }

  TerminalColor fg = tc; // Copy to modify if needed (e.g. bold)

  // Note: Bold logic usually applies to ANSI colors in FG.
  // If we want to support bold for ANSI colors passed here:
  // We don't have 'bold' flag here.
  // But get_color_for_attributes handled it.
  // If we want to support bold, we should pass 'bool bold' or handle it
  // outside. For now, let's assume bold is handled by caller modifying the
  // color or we ignore it for simplicity in this refactor step, OR we keep
  // get_color_for_attributes as a wrapper? Actually, get_color_for_attributes
  // logic for bold was: if (attrs.bold && fg.type == ANSI && fg.ansi_code <=
  // 7) fg.ansi_code += 8; So we can just handle that logic in the caller if
  // needed, or pass 'bold' flag. Let's stick to simple color resolution here.

  if (fg.type == TerminalColor::Type::RGB) {
    color[0] = fg.r / 255.0f;
    color[1] = fg.g / 255.0f;
    color[2] = fg.b / 255.0f;
  } else if (fg.type == TerminalColor::Type::INDEXED) {
    uint8_t idx = fg.indexed_color;
    if (idx < 16) {
      fg.type = TerminalColor::Type::ANSI;
      fg.ansi_color = static_cast<AnsiColor>(idx);
    } else if (idx < 232) {
      idx -= 16;
      int b = idx % 6;
      int g = (idx / 6) % 6;
      int r = (idx / 36) % 6;
      color[0] = (r ? r * 40 + 55 : 0) / 255.0f;
      color[1] = (g ? g * 40 + 55 : 0) / 255.0f;
      color[2] = (b ? b * 40 + 55 : 0) / 255.0f;
      return;
    } else {
      idx -= 232;
      float gray = (idx * 10 + 8) / 255.0f;
      color[0] = gray;
      color[1] = gray;
      color[2] = gray;
      return;
    }
  }

  if (fg.type == TerminalColor::Type::ANSI) {
    switch (fg.ansi_color) {
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
      color[2] = 0.8f;
      break;
    case AnsiColor::BRIGHT_BLACK:
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
    case AnsiColor::RESET:
      if (is_bg) {
        color[0] = 0.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
      } else {
        color[0] = 0.8f;
        color[1] = 0.8f;
        color[2] = 0.8f;
      }
      break;
    default:
      if (is_bg) {
        color[0] = 0.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
      } else {
        color[0] = 0.8f;
        color[1] = 0.8f;
        color[2] = 0.8f;
      }
    }
  }
}

void Terminal::get_color_for_attributes(const TerminalAttributes &attrs,
                                        float *color) {
  TerminalColor fg = attrs.foreground;
  if (attrs.bold && fg.type == TerminalColor::Type::ANSI) {
    int ansi_code = static_cast<int>(fg.ansi_color);
    if (ansi_code >= 0 && ansi_code <= 7) {
      fg.ansi_color = static_cast<AnsiColor>(ansi_code + 8);
    }
  }
  get_color(fg, color, false);
}
