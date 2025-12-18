#include "terminal_parser.h"
#include "utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

TerminalParser::TerminalParser() {
  // Initialize prompt patterns
  prompt_patterns = {
      std::regex(R"(\$ $)"),                        // Basic prompt
      std::regex(R"(# $)"),                         // Root prompt
      std::regex(R"(\w+@\w+:\S+[#$] $)"),           // user@host:path$
      std::regex(R"(\w+@\w+:\S+> $)"),              // user@host:path>
      std::regex(R"(\w+@\w+:\S+\) $)"),             // user@host:path)
      std::regex(R"(\w+@\w+:\S+\[.*\]\$ $)"),       // user@host:path[git]$
      std::regex(R"(\w+@\w+:\S+\(.*\)\$ $)"),       // user@host:path(branch)$
      std::regex(R"(\w+@\w+:\S+\(.*\)\) $)"),       // user@host:path(branch))
      std::regex(R"(\w+@\w+:\S+\(.*\)> $)"),        // user@host:path(branch)>
      std::regex(R"(\w+@\w+:\S+\(.*\)\[.*\]\$ $)"), // Complex git prompt
      std::regex(R"([^
]*[$#>] $)")};

  // Initialize escape sequence patterns
  escape_sequence_regex =
      std::regex(R"(\x1b(\[[0-9;? ]*[a-zA-Z]|].*?(\x07|\x1b\\)))");
  color_escape_regex = std::regex(R"(\x1b\[([0-9;]+)m)");
  cursor_escape_regex = std::regex(R"(\x1b\[(\d+);(\d+)H)");
}

std::vector<TerminalAction>
TerminalParser::parse_input(const std::string &input) {
  std::vector<TerminalAction> actions;
  for (char c : input) {
    process_char(c, actions);
  }
  return actions;
}

void TerminalParser::process_char(char c,
                                  std::vector<TerminalAction> &actions) {
  if (state == State::NORMAL) {
    if (c == '\033') { // ESC
      state = State::ESCAPE;
      escape_buf.clear();
      csi_args.clear();
      csi_priv = false;
    } else if (c == '\n') {
      actions.push_back({ActionType::NEWLINE});
    } else if (c == '\r') {
      actions.push_back({ActionType::CARRIAGE_RETURN});
    } else if (c == '\b') {
      actions.push_back({ActionType::BACKSPACE});
    } else if (c == '\t') {
      actions.push_back({ActionType::TAB});
    } else if ((unsigned char)c >= 32) {
      actions.push_back(
          {ActionType::PRINT_TEXT, std::string(1, c), current_attributes});
    }
  } else if (state == State::ESCAPE) {
    handle_escape(c, actions);
  } else if (state == State::CSI) {
    handle_csi(c, actions);
  } else if (state == State::STR) {
    handle_str(c);
  } else if (state == State::ALT_CHARSET) {
    // Just consume one character (the charset designator) and return to normal
    state = State::NORMAL;
  }
}

void TerminalParser::handle_escape(char c,
                                   std::vector<TerminalAction> &actions) {
  if (c == '[') {
    state = State::CSI;
    csi_args.clear();
    csi_priv = false;
    escape_buf.clear();
  } else if (c == ']' || c == 'P' || c == '_' || c == '^' || c == 'X') {
    // OSC, DCS, APC, PM, SOS - Start string sequence
    state = State::STR;
    str_buf.clear();
  } else if (c == '(' || c == ')') {
    state = State::ALT_CHARSET;
  } else if (c == 'M') {
    actions.push_back({ActionType::REVERSE_INDEX});
    state = State::NORMAL;
  } else if (c == 'E') {
    actions.push_back({ActionType::NEXT_LINE});
    state = State::NORMAL;
  } else if (c == 'D') {
    actions.push_back({ActionType::SCROLL_UP}); // Index is mostly scroll up
    state = State::NORMAL;
  } else if (c == '7') {
    actions.push_back({ActionType::SAVE_CURSOR});
    state = State::NORMAL;
  } else if (c == '8') {
    actions.push_back({ActionType::RESTORE_CURSOR});
    state = State::NORMAL;
  } else {
    state = State::NORMAL;
  }
}

void TerminalParser::handle_csi(char c, std::vector<TerminalAction> &actions) {
  if (isdigit(c)) {
    escape_buf += c;
  } else if (c == ';') {
    if (!escape_buf.empty()) {
      try {
        csi_args.push_back(std::stoi(escape_buf));
      } catch (...) {
        csi_args.push_back(0);
      }
      escape_buf.clear();
    } else {
      csi_args.push_back(0); // Default to 0 if empty
    }
  } else if (c == '>' || c == '=') {
    // CSI Private Mode Formatters (like CSI > Ps) or CSI = Ps
    // Simplification: treat as part of control flow or ignore for now?
    // Actually, CSI > 0 c is often modifying resource.
    // Let's ignore logic for special CSI prefixes until needed.
    // BUT we need to not break parsing.
    // If it's a parameter byte (30-3F), it should be allowed in args?
    // ECMA-48 says 0x30-0x3F are parameter bytes.
    // '>' is 0x3E. '=' is 0x3D.
    // '?' is 0x3F.
  } else if (c == '?') {
    csi_priv = true;
  } else if (c == ' ') {
    // Ignore intermediate space
  } else {
    // Final character of CSI sequence
    if (!escape_buf.empty()) {
      try {
        csi_args.push_back(std::stoi(escape_buf));
      } catch (...) {
        csi_args.push_back(0);
      }
    }

    // Process the CSI command
    switch (c) {
    case 'm': // SGR - Select Graphic Rendition
      update_attributes(csi_args);
      break;
    case 'J': // ED - Erase in Display
    {
      int mode = csi_args.empty() ? 0 : csi_args[0];
      actions.push_back(
          {ActionType::CLEAR_SCREEN, "", current_attributes, mode});
    } break;
    case 'K': // EL - Erase in Line
    {
      int mode = csi_args.empty() ? 0 : csi_args[0];
      actions.push_back({ActionType::CLEAR_LINE, "", current_attributes, mode});
    } break;
    case 'A': // CUU - Cursor Up
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::MOVE_CURSOR, "", {}, -n, 0});
    } break;
    case 'B': // CUD - Cursor Down
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::MOVE_CURSOR, "", {}, n, 0});
    } break;
    case 'C': // CUF - Cursor Forward
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::MOVE_CURSOR, "", {}, 0, n});
    } break;
    case 'D': // CUB - Cursor Backward
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::MOVE_CURSOR, "", {}, 0, -n});
    } break;
    case 'H': // CUP - Cursor Position
    case 'f': // HVP - Horizontal and Vertical Position
    {
      int row = (csi_args.size() > 0) ? csi_args[0] : 1;
      int col = (csi_args.size() > 1) ? csi_args[1] : 1;
      actions.push_back({ActionType::MOVE_CURSOR,
                         "",
                         {},
                         row,
                         col,
                         true}); // flag true for absolute
    } break;
    case 'h': // SM - Set Mode
      if (csi_priv) {
        if (csi_args.size() > 0 && csi_args[0] == 1049) {
          actions.push_back(
              {ActionType::SET_ALTERNATE_BUFFER, "", {}, 0, 0, true});
        } else if (csi_args.size() > 0 && csi_args[0] == 25) {
          actions.push_back(
              {ActionType::SET_CURSOR_VISIBLE, "", {}, 0, 0, true});
        } else if (csi_args.size() > 0 && csi_args[0] == 7) {
          actions.push_back(
              {ActionType::SET_AUTO_WRAP_MODE, "", {}, 0, 0, true});
        } else if (csi_args.size() > 0 && csi_args[0] == 1) {
          actions.push_back(
              {ActionType::SET_APPLICATION_CURSOR_KEYS, "", {}, 0, 0, true});
        }
      } else {
        if (csi_args.size() > 0 && csi_args[0] == 4) {
          actions.push_back({ActionType::SET_INSERT_MODE, "", {}, 0, 0, true});
        }
      }
      break;
    case 'l': // RM - Reset Mode
      if (csi_priv) {
        if (csi_args.size() > 0 && csi_args[0] == 1049) {
          actions.push_back(
              {ActionType::SET_ALTERNATE_BUFFER, "", {}, 0, 0, false});
        } else if (csi_args.size() > 0 && csi_args[0] == 25) {
          actions.push_back(
              {ActionType::SET_CURSOR_VISIBLE, "", {}, 0, 0, false});
        } else if (csi_args.size() > 0 && csi_args[0] == 7) {
          actions.push_back(
              {ActionType::SET_AUTO_WRAP_MODE, "", {}, 0, 0, false});
        } else if (csi_args.size() > 0 && csi_args[0] == 1) {
          actions.push_back(
              {ActionType::SET_APPLICATION_CURSOR_KEYS, "", {}, 0, 0, false});
        }
      } else {
        if (csi_args.size() > 0 && csi_args[0] == 4) {
          actions.push_back({ActionType::SET_INSERT_MODE, "", {}, 0, 0, false});
        }
      }
      break;
    case 'L': // IL - Insert Line
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::INSERT_LINE, "", current_attributes, n});
    } break;
    case 'M': // DL - Delete Line
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::DELETE_LINE, "", current_attributes, n});
    } break;
    case '@': // ICH - Insert Character
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::INSERT_CHAR, "", current_attributes, n});
    } break;
    case 'P': // DCH - Delete Character
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::DELETE_CHAR, "", current_attributes, n});
    } break;
    case 'r': // DECSTBM - Set Scrolling Region
    {
      int top = (csi_args.size() > 0) ? csi_args[0] : 1;
      int bottom = (csi_args.size() > 1) ? csi_args[1] : 0; // 0 means end
      actions.push_back({ActionType::SET_SCROLL_REGION, "", {}, top, bottom});
    } break;
    case 'n': // DSR - Device Status Report
    {
      int arg = csi_args.empty() ? 0 : csi_args[0];
      if (arg == 6) {
        actions.push_back({ActionType::REPORT_CURSOR_POSITION});
      } else if (arg == 5) {
        actions.push_back({ActionType::REPORT_DEVICE_STATUS});
      }
    } break;
    case 'S': // SU - Scroll Up
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::SCROLL_TEXT_UP, "", {}, n});
    } break;
    case 'T': // SD - Scroll Down
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::SCROLL_TEXT_DOWN, "", {}, n});
    } break;
    case 'X': // ECH - Erase Character
    {
      int n = csi_args.empty() ? 1 : csi_args[0];
      actions.push_back({ActionType::ERASE_CHAR, "", current_attributes, n});
    } break;
    case 's':
      actions.push_back({ActionType::SAVE_CURSOR});
      break;
    case 'u':
      actions.push_back({ActionType::RESTORE_CURSOR});
      break;
    }
    state = State::NORMAL;
  }
}

void TerminalParser::handle_str(char c) {
  if (c == '\007' ||
      (c == '\\' && !str_buf.empty() && str_buf.back() == '\033')) {
    // End of string sequence (BEL or ST)
    state = State::NORMAL;
  } else {
    str_buf += c;
  }
}

void TerminalParser::update_attributes(const std::vector<int> &params) {
  if (params.empty()) {
    update_attributes_from_code(0);
    return;
  }
  for (size_t i = 0; i < params.size(); ++i) {
    int code = params[i];
    if (code == 38 || code == 48) {
      // Extended color
      bool is_fg = (code == 38);
      if (i + 1 < params.size()) {
        int mode = params[i + 1];
        if (mode == 5) { // 256 color
          if (i + 2 < params.size()) {
            int color_idx = params[i + 2];
            TerminalColor &color = is_fg ? current_attributes.foreground
                                         : current_attributes.background;
            color.type = TerminalColor::Type::INDEXED;
            color.indexed_color = static_cast<uint8_t>(color_idx);
            i += 2;
          }
        } else if (mode == 2) { // TrueColor
          if (i + 4 < params.size()) {
            int r = params[i + 2];
            int g = params[i + 3];
            int b = params[i + 4];
            TerminalColor &color = is_fg ? current_attributes.foreground
                                         : current_attributes.background;
            color.type = TerminalColor::Type::RGB;
            color.r = static_cast<uint8_t>(r);
            color.g = static_cast<uint8_t>(g);
            color.b = static_cast<uint8_t>(b);
            i += 4;
          }
        }
      }
    } else {
      update_attributes_from_code(code);
    }
  }
}

void TerminalParser::update_attributes_from_code(int code) {
  switch (code) {
  case 0:
    current_attributes = TerminalAttributes{};
    break;
  case 1:
    current_attributes.bold = true;
    break;
  case 3:
    current_attributes.italic = true;
    break;
  case 4:
    current_attributes.underline = true;
    break;
  case 5:
    current_attributes.blink = true;
    break;
  case 7:
    current_attributes.reverse = true;
    break;
  case 9:
    current_attributes.strikethrough = true;
    break;
  default:
    if (code >= 30 && code <= 37) {
      current_attributes.foreground.type = TerminalColor::Type::ANSI;
      current_attributes.foreground.ansi_color = parse_color_code(code);
    } else if (code >= 40 && code <= 47) {
      current_attributes.background.type = TerminalColor::Type::ANSI;
      current_attributes.background.ansi_color = parse_color_code(code - 10);
    } else if (code >= 90 && code <= 97) {
      current_attributes.foreground.type = TerminalColor::Type::ANSI;
      current_attributes.foreground.ansi_color = parse_color_code(code);
    } else if (code >= 100 && code <= 107) {
      current_attributes.background.type = TerminalColor::Type::ANSI;
      current_attributes.background.ansi_color = parse_color_code(code - 10);
    } else if (code == 39) {                           // Reset FG
      current_attributes.foreground = TerminalColor{}; // Default
    } else if (code == 49) {                           // Reset BG
      current_attributes.background = TerminalColor{}; // Default
    }
    break;
  }
}

AnsiColor TerminalParser::parse_color_code(int code) {
  switch (code) {
  case 30:
    return AnsiColor::BLACK;
  case 31:
    return AnsiColor::RED;
  case 32:
    return AnsiColor::GREEN;
  case 33:
    return AnsiColor::YELLOW;
  case 34:
    return AnsiColor::BLUE;
  case 35:
    return AnsiColor::MAGENTA;
  case 36:
    return AnsiColor::CYAN;
  case 37:
    return AnsiColor::WHITE;
  case 90:
    return AnsiColor::BRIGHT_BLACK;
  case 91:
    return AnsiColor::BRIGHT_RED;
  case 92:
    return AnsiColor::BRIGHT_GREEN;
  case 93:
    return AnsiColor::BRIGHT_YELLOW;
  case 94:
    return AnsiColor::BRIGHT_BLUE;
  case 95:
    return AnsiColor::BRIGHT_MAGENTA;
  case 96:
    return AnsiColor::BRIGHT_CYAN;
  case 97:
    return AnsiColor::BRIGHT_WHITE;
  case 0:
    return AnsiColor::RESET;
  default:
    return AnsiColor::WHITE;
  }
}

// Stubs for deprecated/unused methods
std::vector<ParsedLine>
TerminalParser::parse_output(const std::string &output) {
  return {};
}
std::string TerminalParser::strip_escape_sequences(const std::string &text) {
  return text;
}
TerminalAttributes
TerminalParser::parse_escape_sequence(const std::string &escape_seq) {
  return {};
}
bool TerminalParser::is_prompt(const std::string &line) { return false; }
bool TerminalParser::is_command_output(const std::string &line) {
  return false;
}
bool TerminalParser::is_error_output(const std::string &line) { return false; }
void TerminalParser::erase_in_line(int mode) {}
void TerminalParser::erase_in_display(int mode) {}
void TerminalParser::move_cursor(int row, int col) {}
