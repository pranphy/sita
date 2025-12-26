#ifndef TERMINAL_PARSER_H
#define TERMINAL_PARSER_H

#include <map>
#include <regex>
#include <string>
#include <vector>

enum class ActionType {
  PRINT_TEXT,
  NEWLINE,
  CARRIAGE_RETURN,
  BACKSPACE,
  MOVE_CURSOR,
  CLEAR_SCREEN,
  CLEAR_LINE,
  SET_ALTERNATE_BUFFER,
  SET_ATTRIBUTE,
  SCROLL_UP,
  SCROLL_DOWN,
  INSERT_LINE,
  DELETE_LINE,
  INSERT_CHAR,
  DELETE_CHAR,
  SET_SCROLL_REGION,
  REPORT_CURSOR_POSITION,
  REPORT_DEVICE_STATUS,
  TAB,
  ERASE_CHAR,
  SET_INSERT_MODE,
  SAVE_CURSOR,
  RESTORE_CURSOR,
  SET_CURSOR_VISIBLE,
  REVERSE_INDEX,
  SET_AUTO_WRAP_MODE,
  SCROLL_TEXT_UP,
  SCROLL_TEXT_DOWN,
  SET_APPLICATION_CURSOR_KEYS,
  NEXT_LINE
};

enum class AnsiColor {
  BLACK,
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGENTA,
  CYAN,
  WHITE,
  BRIGHT_BLACK,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_YELLOW,
  BRIGHT_BLUE,
  BRIGHT_MAGENTA,
  BRIGHT_CYAN,
  BRIGHT_WHITE,
  RESET
};

struct TerminalColorE {
  enum class Type { DEFAULT, ANSI, INDEXED, RGB } type = Type::DEFAULT;
  AnsiColor ansi_color = AnsiColor::WHITE;
  uint8_t indexed_color = 0;
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

struct TerminalColor {
    enum class Type { DEFAULT, ANSI, INDEXED, RGB } type = Type::DEFAULT;

    AnsiColor ansi_color = AnsiColor::WHITE;
    int indexed_color = 0;
    uint8_t r = 255, g = 255, b = 255;

    bool operator==(const TerminalColor& other) const {
        return type == other.type &&
               ansi_color == other.ansi_color &&
               indexed_color == other.indexed_color &&
               r == other.r &&
               g == other.g &&
               b == other.b;
    }

    bool operator!=(const TerminalColor& other) const {
        return !(*this == other);
    }
};


struct TerminalAttributes {
  TerminalColor foreground; // Default initialized to Type::DEFAULT
  TerminalColor background; // Default initialized to Type::DEFAULT
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool blink = false;
  bool reverse = false;
  bool strikethrough = false;
};

struct TerminalAction {
  ActionType type;
  std::string text = "";
  TerminalAttributes attributes = {};
  int row = 0;
  int col = 0;
  bool flag = false; // For boolean toggles like alternate buffer
};

enum class LineType {
  PROMPT,
  COMMAND_OUTPUT,
  ERROR_OUTPUT,
  USER_INPUT,
  UNKNOWN
};

struct Cell {
  std::string content;
  TerminalAttributes attributes;
};

struct Segment {
  std::string content;
  TerminalAttributes attributes;
};

struct ParsedLine {
  std::vector<Segment> segments;
  LineType type;
  bool clear_screen = false;
};

class TerminalParser {
public:
  TerminalParser();
  std::vector<TerminalAction> parse_input(const std::string &input);

  // Deprecated, kept for compatibility during refactor
  std::vector<ParsedLine> parse_output(const std::string &output);

private:
  enum class State {
    NORMAL,
    ESCAPE,
    CSI,     // [
    STR,     // ] or P or _ or ^
    STR_END, // ST (String Terminator)
    ALT_CHARSET
  };

  State state = State::NORMAL;
  std::string escape_buf;
  std::vector<int> csi_args;
  std::string str_buf;
  char csi_mode = 0;
  bool csi_priv = false;

  TerminalAttributes current_attributes;
  struct CursorPosition {
    int row;
    int col;
  };
  CursorPosition current_cursor;

  std::vector<std::regex> prompt_patterns;

  // Helper methods for state machine
  void process_char(char c, std::vector<TerminalAction> &actions);
  void handle_control_code(char c, std::vector<TerminalAction> &actions);
  void handle_escape(char c, std::vector<TerminalAction> &actions);
  void handle_csi(char c, std::vector<TerminalAction> &actions);
  void parse_csi_params();
  void handle_str(char c);

  // Action helpers
  void update_attributes(const std::vector<int> &params);
  AnsiColor parse_color(int code);

  // Old helpers
  std::regex escape_sequence_regex;
  std::regex color_escape_regex;
  std::regex cursor_escape_regex;

  // Helper functions
  TerminalAttributes parse_escape_sequence(const std::string &escape_seq);
  CursorPosition parse_cursor_escape(const std::string &escape_seq);
  void clear_screen();
  void clear_line();
  void erase_in_line(int mode);
  void erase_in_display(int mode);
  void move_cursor(int row, int col);

  AnsiColor parse_color_code(int code);
  void update_attributes_from_code(int code);

  std::string strip_escape_sequences(const std::string &text);
  bool is_prompt(const std::string &line);
  bool is_command_output(const std::string &line);
  bool is_error_output(const std::string &line);
};

#endif // TERMINAL_PARSER_H
