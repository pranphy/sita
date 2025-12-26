#ifndef TERMINAL_H
#define TERMINAL_H

#include "terminal_parser.h"
#include "tty.h"
#include <string>
#include <vector>

class Terminal {
public:
    Terminal(int width, int height);
    ~Terminal();

    // Window / size
    void set_window_size(int rows, int cols);

    // Scrollback
    void scroll_up();
    void scroll_down();
    void scroll_page_up();
    void scroll_page_down();
    void scroll_to_bottom();

    // Input
    void send_input(const std::string& input);
    void key_pressed(char c, int type); // legacy, still supported

    // PTY → terminal state
    std::string poll_output();

    // Input method (IME)
    void set_preedit(const std::string& text, int cursor);
    const std::string& get_preedit() const { return preedit_text; }
    int get_preedit_cursor() const { return preedit_cursor; }
    void clear_preedit();

    // Exposed for renderer
    const std::vector<std::vector<Cell>>& screen() const { return screen_buffer; }
    const std::vector<ParsedLine>& history() const { return parsed_buffer; }
    int scroll_offset_value() const { return scroll_offset; }
    int rows() const { return screen_rows; }
    int cols() const { return screen_cols; }

    // PTY
    tty term;

public:
    // State used by renderer / rest of app

    int scroll_offset = 0;

    bool alternate_screen_active = false;
    bool insert_mode = false;
    bool auto_wrap_mode = true;
    bool wrap_next = false;

    std::vector<std::vector<Cell>> screen_buffer;
    int screen_cursor_row = 0;
    int screen_cursor_col = 0;
    int screen_rows = 24;
    int screen_cols = 80;

    int scroll_region_top = 0;
    int scroll_region_bottom = -1;

    std::string pending_utf8;

    std::vector<ParsedLine> parsed_buffer;
    ParsedLine active_line;

    TerminalParser parser;

    std::string preedit_text;
    int preedit_cursor = 0;

    int saved_cursor_row = 0;
    int saved_cursor_col = 0;
    bool cursor_visible = true;
    bool application_cursor_keys = false;

private:
    // High-level processing
    void process_actions(const std::vector<TerminalAction>& actions);
    void process_screen_mode(const TerminalAction& a);
    void process_history_mode(const TerminalAction& a);

    // UTF‑8 / combining
    bool decode_next_utf8(std::string& pending, std::string& out_char);
    uint32_t decode_codepoint(const std::string& utf8) const;
    bool is_combining(uint32_t cp) const;
    void apply_combining(const std::string& mark);

    // Screen operations
    void handle_print_text(const std::string& text, const TerminalAttributes& attr);
    void write_char(const std::string& utf8, const TerminalAttributes& attr);
    void newline();
    void carriage_return();
    void backspace();
    void move_cursor(int row, int col, bool absolute);
    void clear_screen(int mode, const TerminalAttributes& attr);
    void clear_line(int mode, const TerminalAttributes& attr);
    void insert_lines(int count, const TerminalAttributes& attr);
    void delete_lines(int count, const TerminalAttributes& attr);
    void insert_chars(int count, const TerminalAttributes& attr);
    void delete_chars(int count, const TerminalAttributes& attr);
    void erase_chars(int count, const TerminalAttributes& attr);
    void perform_scroll_up();
    void perform_scroll_down();

    // History mode
    void append_history_text(const std::string& text, const TerminalAttributes& attr);
    void finalize_history_line();
    void backspace_history();
};

#endif // TERMINAL_H

