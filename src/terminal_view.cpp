#include "terminal_view.h"
#include "oglutil.h"
#include "utils.h"
#include <cmath>
#include <iostream>

TerminalView::TerminalView(Terminal &term) : terminal(term) {
  last_cursor_time = glfwGetTime();
}

TerminalView::~TerminalView() {}

void TerminalView::set_renderer(TextRenderer *renderer) {
  text_renderer = renderer;
  update_dimensions();
}

void TerminalView::set_window_size(float width, float height) {
  win_width = width;
  win_height = height;

  // Update terminal logic with new size (rows/cols)
  int rows = (int)(height / LINE_HEIGHT);
  int cols = (int)(width / CELL_WIDTH);
  terminal.set_window_size(rows, cols);
}

void TerminalView::update_dimensions() {
  if (text_renderer) {
    CELL_WIDTH = text_renderer->get_char_width();
    LINE_HEIGHT = text_renderer->get_line_height();
    // Re-calculate window size to update terminal rows/cols if needed
    // But usually set_window_size is called by GUI on resize
  }
}

void TerminalView::update_cursor_blink() {
  double current_time = glfwGetTime();
  if (current_time - last_cursor_time >= 0.5) {
    cursor_visible = !cursor_visible;
    last_cursor_time = current_time;
  }
}

void TerminalView::render() {
  glClear(GL_COLOR_BUFFER_BIT);
  float start_y = win_height - LINE_HEIGHT;
  cursor_pos = {25.0f, start_y};

  if (text_renderer) {
    if (terminal.alternate_screen_active) {
      // Render screen buffer
      float y = win_height - LINE_HEIGHT;
      for (const auto &row : terminal.screen_buffer) {
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
                 cell.attributes.foreground.b ==
                     current_attrs.foreground.b))) &&

              (cell.attributes.background.type ==
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
                 cell.attributes.background.b == current_attrs.background.b)));

          if (!attrs_match || i == row.size() - 1) {
            // Render previous segment
            float current_x = 25.0f + segment_start_col * CELL_WIDTH;
            float bg_color[4];
            if (current_attrs.reverse) {
              get_color(current_attrs.foreground, bg_color,
                        false); // Use FG as BG (Reverse)
            } else {
              get_color(current_attrs.background, bg_color, true);
            }

            float bg_w = (i - segment_start_col) * CELL_WIDTH;
            if (i == row.size() - 1 && attrs_match) {
              bg_w += CELL_WIDTH; // Include last char if it matches
              current_segment_text += cell.content;
            }

            float bg_h = LINE_HEIGHT;
            text_renderer->draw_solid_rectangle(
                current_x, y, bg_w, bg_h, bg_color, win_width, win_height);

            float fg_color[4];
            if (current_attrs.reverse) {
              get_color(current_attrs.background, fg_color,
                        true); // Use BG as FG (Reverse)
            } else {
              get_color_for_attributes(current_attrs,
                                       fg_color); // Normal FG (handles Bold)
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
                // ASCII/Latin: Render char by char to enforce grid alignment
                size_t pos = 0;
                while (pos < chunk.length()) {
                  size_t prev_pos = pos;
                  utl::get_next_codepoint(chunk, pos);
                  std::string char_str = chunk.substr(prev_pos, pos - prev_pos);
                  text_renderer->render_text_harfbuzz(
                      char_str, {current_x, y + baseline_offset}, 1.0f,
                      fg_color, win_width, win_height);
                  current_x += CELL_WIDTH;
                }
              }
            }

            // Start new segment
            current_segment_text = cell.content;
            current_attrs = cell.attributes;
            segment_start_col = i;
          } else {
            current_segment_text += cell.content;
          }
        }
        y -= LINE_HEIGHT;
      }

      // Cursor for Alternate Screen
      cursor_pos.x = 25.0f + terminal.screen_cursor_col * CELL_WIDTH;
      cursor_pos.y =
          win_height - LINE_HEIGHT - terminal.screen_cursor_row * LINE_HEIGHT;

    } else {
      // Normal Mode
      size_t total_lines = terminal.parsed_buffer.size() + 1;
      size_t max_lines = (win_height / LINE_HEIGHT) - 1; // Approximate

      // Clamp scroll_offset
      if (terminal.scroll_offset > (int)total_lines - (int)max_lines) {
        terminal.scroll_offset = (int)total_lines - (int)max_lines;
      }
      if (terminal.scroll_offset < 0)
        terminal.scroll_offset = 0;

      size_t start_index = 0;
      if (total_lines > max_lines) {
        start_index = total_lines - max_lines - terminal.scroll_offset;
      }

      // Render parsed_buffer lines
      for (size_t i = 0; i < terminal.parsed_buffer.size(); ++i) {
        if (i >= start_index && i < start_index + max_lines) {
          render_line(terminal.parsed_buffer[i], start_y);
        }
      }

      // Render active_line
      if (terminal.parsed_buffer.size() >= start_index &&
          terminal.parsed_buffer.size() < start_index + max_lines) {
        float current_line_y =
            start_y; // Capture Y before render_line modifies it
        render_line(terminal.active_line, start_y);

        // Calculate cursor position for Normal Mode
        float cursor_x = 25.0f;
        float cursor_y = current_line_y;
        float limit_x = 25.0f + terminal.screen_cols * CELL_WIDTH;

        for (const auto &segment : terminal.active_line.segments) {
          for (const auto &chunk : utl::split_by_devanagari(segment.content)) {
            size_t temp_pos = 0;
            unsigned int first_cp = utl::get_next_codepoint(chunk, temp_pos);
            if (utl::is_devanagari(first_cp)) {
              float w = text_renderer->measure_text_width(chunk, 1.0f);
              if (cursor_x + w > limit_x) {
                cursor_y -= LINE_HEIGHT;
                cursor_x = 25.0f;
              }
              cursor_x += w;
            } else {
              // ASCII/Latin: Grid alignment
              size_t len = 0;
              size_t pos = 0;
              while (pos < chunk.length()) {
                utl::get_next_codepoint(chunk, pos);
                len++;
              }
              for (size_t i = 0; i < len; ++i) {
                if (cursor_x + CELL_WIDTH > limit_x) {
                  cursor_y -= LINE_HEIGHT;
                  cursor_x = 25.0f;
                }
                cursor_x += CELL_WIDTH;
              }
            }
          }
        }
        cursor_pos.x = cursor_x;
        cursor_pos.y = cursor_y;
      }
    }
  }

  // Render cursor
  if (cursor_visible) {
    render_cursor(cursor_pos.x, cursor_pos.y);
  }
}

void TerminalView::render_line(const ParsedLine &line, float &y_pos) {
  float start_x = 25.0f;
  float current_x = start_x;
  float limit_x = start_x + terminal.screen_cols * CELL_WIDTH;

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
      size_t temp_pos = 0;
      unsigned int first_cp = utl::get_next_codepoint(chunk, temp_pos);
      float baseline_offset = LINE_HEIGHT * 0.25f;

      if (utl::is_devanagari(first_cp)) {
        float w = text_renderer->measure_text_width(chunk, 1.0f);
        if (current_x + w > limit_x) {
          y_pos -= LINE_HEIGHT;
          current_x = start_x;
        }
        // Render BG
        text_renderer->draw_solid_rectangle(current_x, y_pos, w, LINE_HEIGHT,
                                            bg_color, win_width, win_height);
        // Render Text
        text_renderer->render_text_harfbuzz(
            chunk, {current_x, y_pos + baseline_offset}, 1.0f, color, win_width,
            win_height);
        current_x += w;
      } else {
        // ASCII/Latin: Render char by char for grid alignment
        size_t pos = 0;
        while (pos < chunk.length()) {
          size_t prev_pos = pos;
          utl::get_next_codepoint(chunk, pos);
          std::string char_str = chunk.substr(prev_pos, pos - prev_pos);

          if (current_x + CELL_WIDTH > limit_x) {
            y_pos -= LINE_HEIGHT;
            current_x = start_x;
          }

          // Render BG
          text_renderer->draw_solid_rectangle(current_x, y_pos, CELL_WIDTH,
                                              LINE_HEIGHT, bg_color, win_width,
                                              win_height);
          // Render Text
          text_renderer->render_text_harfbuzz(
              char_str, {current_x, y_pos + baseline_offset}, 1.0f, color,
              win_width, win_height);
          current_x += CELL_WIDTH;
        }
      }
    }
  }
  // Move to the next line (hard wrap for the end of the line)
  y_pos -= LINE_HEIGHT;
}

void TerminalView::render_cursor(float x, float y) {
  if (text_renderer) {
    // Use a white color for the cursor
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // White cursor

    text_renderer->draw_solid_rectangle(x, y, CELL_WIDTH, LINE_HEIGHT, color,
                                        win_width, win_height);
  }
}

void TerminalView::get_color(const TerminalColor &color, float *out_color,
                             bool is_bg) {
  if (color.type == TerminalColor::Type::RGB) {
    out_color[0] = color.r / 255.0f;
    out_color[1] = color.g / 255.0f;
    out_color[2] = color.b / 255.0f;
    out_color[3] = 1.0f;
  } else if (color.type == TerminalColor::Type::INDEXED) {
    // 256 color lookup
    // For now, fallback to white/black
    if (is_bg) {
      out_color[0] = 0.0f;
      out_color[1] = 0.0f;
      out_color[2] = 0.0f;
      out_color[3] = 1.0f;
    } else {
      out_color[0] = 1.0f;
      out_color[1] = 1.0f;
      out_color[2] = 1.0f;
      out_color[3] = 1.0f;
    }
  } else if (color.type == TerminalColor::Type::ANSI) {
    // Simple ANSI mapping
    switch (color.ansi_color) {
    case AnsiColor::BLACK:
      out_color[0] = 0.0f;
      out_color[1] = 0.0f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::RED:
      out_color[0] = 0.8f;
      out_color[1] = 0.0f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::GREEN:
      out_color[0] = 0.0f;
      out_color[1] = 0.8f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::YELLOW:
      out_color[0] = 0.8f;
      out_color[1] = 0.8f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::BLUE:
      out_color[0] = 0.0f;
      out_color[1] = 0.0f;
      out_color[2] = 0.8f;
      break;
    case AnsiColor::MAGENTA:
      out_color[0] = 0.8f;
      out_color[1] = 0.0f;
      out_color[2] = 0.8f;
      break;
    case AnsiColor::CYAN:
      out_color[0] = 0.0f;
      out_color[1] = 0.8f;
      out_color[2] = 0.8f;
      break;
    case AnsiColor::WHITE:
      out_color[0] = 0.9f;
      out_color[1] = 0.9f;
      out_color[2] = 0.9f;
      break;
    case AnsiColor::BRIGHT_BLACK:
      out_color[0] = 0.5f;
      out_color[1] = 0.5f;
      out_color[2] = 0.5f;
      break;
    case AnsiColor::BRIGHT_RED:
      out_color[0] = 1.0f;
      out_color[1] = 0.0f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::BRIGHT_GREEN:
      out_color[0] = 0.0f;
      out_color[1] = 1.0f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::BRIGHT_YELLOW:
      out_color[0] = 1.0f;
      out_color[1] = 1.0f;
      out_color[2] = 0.0f;
      break;
    case AnsiColor::BRIGHT_BLUE:
      out_color[0] = 0.0f;
      out_color[1] = 0.0f;
      out_color[2] = 1.0f;
      break;
    case AnsiColor::BRIGHT_MAGENTA:
      out_color[0] = 1.0f;
      out_color[1] = 0.0f;
      out_color[2] = 1.0f;
      break;
    case AnsiColor::BRIGHT_CYAN:
      out_color[0] = 0.0f;
      out_color[1] = 1.0f;
      out_color[2] = 1.0f;
      break;
    case AnsiColor::BRIGHT_WHITE:
      out_color[0] = 1.0f;
      out_color[1] = 1.0f;
      out_color[2] = 1.0f;
      break;
    default:
      out_color[0] = 1.0f;
      out_color[1] = 1.0f;
      out_color[2] = 1.0f;
      break;
    }
    out_color[3] = 1.0f;
  } else {
    // DEFAULT
    if (is_bg) {
      out_color[0] = 0.0f;
      out_color[1] = 0.0f;
      out_color[2] = 0.0f;
      out_color[3] = 1.0f; // Black
    } else {
      out_color[0] = 1.0f;
      out_color[1] = 1.0f;
      out_color[2] = 1.0f;
      out_color[3] = 1.0f; // White
    }
  }
}

void TerminalView::get_color_for_attributes(const TerminalAttributes &attrs,
                                            float *color) {
  get_color(attrs.foreground, color, false);
  if (attrs.bold && attrs.foreground.type == TerminalColor::Type::ANSI &&
      attrs.foreground.ansi_color < AnsiColor::BRIGHT_BLACK) {
    // Convert to bright version if using standard ANSI colors
    // This is a simple approximation
    for (int i = 0; i < 3; ++i) {
      color[i] = std::min(1.0f, color[i] + 0.5f);
    }
  }
}
