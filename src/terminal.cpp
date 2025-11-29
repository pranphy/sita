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

Terminal::~Terminal() {
  // Cleanup if needed
  cursor_pos = {0.0f, 0.0f};
}

void Terminal::render_line(const ParsedLine &line, float &y_pos) {
  float current_x = 25.0f;
  float margin = 25.0f;
  float max_width = win_width - (margin * 2);

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
    // Determine how many lines we can show
    // We want to show the last N lines of parsed_buffer + active_raw_line

    // First, parse active_raw_line to see how many lines it takes
    std::vector<ParsedLine> active_parsed;
    if (!active_raw_line.empty()) {
      active_parsed = parser.parse_output(active_raw_line);
    }

    size_t total_lines = parsed_buffer.size() + active_parsed.size();
    size_t max_lines = (win_height / 50.0f) - 1; // Approximate

    size_t start_index = 0;
    if (total_lines > max_lines) {
      start_index = total_lines - max_lines;
    }

    // Render parsed_buffer lines
    for (size_t i = 0; i < parsed_buffer.size(); ++i) {
      if (i >= start_index) {
        render_line(parsed_buffer[i], start_y);
      }
    }

    // Render active_raw_line lines
    for (size_t i = 0; i < active_parsed.size(); ++i) {
      if (parsed_buffer.size() + i >= start_index) {
        render_line(active_parsed[i], start_y);
      }
    }
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
}

void Terminal::send_input(const std::string &input) {
  for (char c : input) {
    term.write_to_pty(c);
  }
  // No local buffering, rely on PTY echo
}

void Terminal::key_pressed(char c, int type) {
  // Deprecated, but kept for compatibility if needed
  std::string s(1, c);
  send_input(s);
}

std::string Terminal::poll_output() {
  auto result = term.handle_pty_output();
  if (!result.empty()) {
    // Append new output to the active raw line
    active_raw_line += result;

    // Check for newlines to commit complete lines
    size_t last_nl = active_raw_line.rfind('\n');
    if (last_nl != std::string::npos) {
      std::string complete_chunk =
          active_raw_line.substr(0, last_nl); // Up to last \n
      std::string remainder = active_raw_line.substr(last_nl + 1);

      // Parse complete chunk
      // Note: parse_output splits by newline, so it handles multiple lines in
      // complete_chunk
      auto complete_parsed = parser.parse_output(complete_chunk);

      // Append to parsed_buffer
      parsed_buffer.insert(parsed_buffer.end(), complete_parsed.begin(),
                           complete_parsed.end());

      // Update active_raw_line to be the remainder
      active_raw_line = remainder;
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
