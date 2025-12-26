#include "terminal.h"
#include "utils.h"

#include <algorithm>

Terminal::Terminal(int width, int height)
    : screen_rows(height),
      screen_cols(width)
{
    screen_buffer.resize(screen_rows);
    for (auto& row : screen_buffer)
        row.resize(screen_cols, Cell{" ", {}});
}

Terminal::~Terminal() = default;

void Terminal::set_window_size(int rows, int cols) {
    screen_rows = rows;
    screen_cols = cols;
    term.set_window_size(rows, cols);

    screen_buffer.resize(screen_rows);
    for (auto& row : screen_buffer)
        row.resize(screen_cols, Cell{" ", {}});
}

void Terminal::send_input(const std::string& input) {
    for (char c : input)
        term.write_to_pty(c);
}

void Terminal::key_pressed(char c, int /*type*/) {
    std::string s(1, c);
    send_input(s);
}

void Terminal::set_preedit(const std::string& text, int cursor) {
    preedit_text = text;
    preedit_cursor = cursor;
}

void Terminal::clear_preedit() {
    preedit_text.clear();
    preedit_cursor = 0;
}

std::string Terminal::poll_output() {
    std::string result = term.handle_pty_output();
    if (result.empty())
        return result;

    auto actions = parser.parse_input(result);
    process_actions(actions);
    return result;
}

// ------------------------------------------------------------
// High-level processing
// ------------------------------------------------------------

void Terminal::process_actions(const std::vector<TerminalAction>& actions) {
    for (const auto& a : actions) {
        if (a.type == ActionType::SET_ALTERNATE_BUFFER) {
            alternate_screen_active = a.flag;
            if (alternate_screen_active) {
                if (screen_buffer.size() != static_cast<size_t>(screen_rows)) {
                    screen_buffer.resize(screen_rows);
                    for (auto& row : screen_buffer)
                        row.resize(screen_cols, Cell{" ", {}});
                }
                screen_cursor_row = 0;
                screen_cursor_col = 0;
            }
            continue;
        }

        if (a.type == ActionType::SET_INSERT_MODE) {
            insert_mode = a.flag;
            continue;
        }

        if (alternate_screen_active)
            process_screen_mode(a);
        else
            process_history_mode(a);
    }
}

// ------------------------------------------------------------
// Screen mode
// ------------------------------------------------------------

void Terminal::process_screen_mode(const TerminalAction& a) {
    switch (a.type) {
        case ActionType::PRINT_TEXT:
            handle_print_text(a.text, a.attributes);
            break;

        case ActionType::NEWLINE:
            wrap_next = false;
            newline();
            break;

        case ActionType::CARRIAGE_RETURN:
            wrap_next = false;
            carriage_return();
            break;

        case ActionType::BACKSPACE:
            wrap_next = false;
            backspace();
            break;

        case ActionType::MOVE_CURSOR:
            wrap_next = false;
            move_cursor(a.row, a.col, a.flag);
            break;

        case ActionType::CLEAR_SCREEN:
            clear_screen(a.row, a.attributes);
            break;

        case ActionType::CLEAR_LINE:
            clear_line(a.row, a.attributes);
            break;

        case ActionType::INSERT_LINE:
            insert_lines(a.row, a.attributes);
            break;

        case ActionType::DELETE_LINE:
            delete_lines(a.row, a.attributes);
            break;

        case ActionType::INSERT_CHAR:
            insert_chars(a.row, a.attributes);
            break;

        case ActionType::DELETE_CHAR:
            delete_chars(a.row, a.attributes);
            break;

        case ActionType::ERASE_CHAR:
            erase_chars(a.row, a.attributes);
            break;

        case ActionType::SET_SCROLL_REGION:
            scroll_region_top = a.row - 1;
            scroll_region_bottom = a.col - 1;
            if (scroll_region_top < 0) scroll_region_top = 0;
            if (scroll_region_bottom < 0) scroll_region_bottom = -1;
            screen_cursor_row = 0;
            screen_cursor_col = 0;
            break;

        case ActionType::SCROLL_TEXT_UP:
            for (int i = 0; i < a.row; ++i) perform_scroll_up();
            break;

        case ActionType::SCROLL_TEXT_DOWN:
            for (int i = 0; i < a.row; ++i) perform_scroll_down();
            break;

        case ActionType::SET_CURSOR_VISIBLE:
            cursor_visible = a.flag;
            break;

        case ActionType::SAVE_CURSOR:
            saved_cursor_row = screen_cursor_row;
            saved_cursor_col = screen_cursor_col;
            break;

        case ActionType::RESTORE_CURSOR:
            wrap_next = false;
            screen_cursor_row = std::clamp(saved_cursor_row, 0, screen_rows - 1);
            screen_cursor_col = std::clamp(saved_cursor_col, 0, screen_cols - 1);
            break;

        case ActionType::REVERSE_INDEX:
            if (screen_cursor_row == scroll_region_top)
                perform_scroll_down();
            else if (screen_cursor_row > 0)
                --screen_cursor_row;
            break;

        case ActionType::NEXT_LINE:
            wrap_next = false;
            screen_cursor_col = 0;
            newline();
            break;

        case ActionType::TAB: {
            int tab_width = 8;
            screen_cursor_col = (screen_cursor_col / tab_width + 1) * tab_width;
            if (screen_cursor_col >= screen_cols)
                screen_cursor_col = screen_cols - 1;
            break;
        }

        case ActionType::REPORT_CURSOR_POSITION: {
            std::string response = "\033[" +
                                   std::to_string(screen_cursor_row + 1) + ";" +
                                   std::to_string(screen_cursor_col + 1) + "R";
            send_input(response);
            break;
        }

        case ActionType::REPORT_DEVICE_STATUS:
            send_input("\033[0n");
            break;

        default:
            break;
    }
}

// ------------------------------------------------------------
// History mode
// ------------------------------------------------------------

void Terminal::process_history_mode(const TerminalAction& a) {
    switch (a.type) {
        case ActionType::PRINT_TEXT:
            append_history_text(a.text, a.attributes);
            break;

        case ActionType::NEWLINE:
            finalize_history_line();
            scroll_offset = 0;
            break;

        case ActionType::CLEAR_SCREEN:
            parsed_buffer.clear();
            active_line = ParsedLine{};
            scroll_offset = 0;
            break;

        case ActionType::BACKSPACE:
            backspace_history();
            break;

        default:
            break;
    }
}

// ------------------------------------------------------------
// UTF-8 / combining
// ------------------------------------------------------------

bool Terminal::decode_next_utf8(std::string& pending, std::string& out_char) {
    if (pending.empty())
        return false;

    unsigned char b = static_cast<unsigned char>(pending[0]);
    int needed = 1;
    if ((b & 0x80) == 0x00)      needed = 1;
    else if ((b & 0xE0) == 0xC0) needed = 2;
    else if ((b & 0xF0) == 0xE0) needed = 3;
    else if ((b & 0xF8) == 0xF0) needed = 4;

    if (static_cast<int>(pending.size()) < needed)
        return false;

    out_char = pending.substr(0, needed);
    pending.erase(0, needed);
    return true;
}

uint32_t Terminal::decode_codepoint(const std::string& utf8) const {
    size_t pos = 0;
    return utl::get_next_codepoint(utf8, pos);
}

bool Terminal::is_combining(uint32_t cp) const {
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    if (cp == 0x200D || cp == 0x200C) return true;
    if (cp >= 0x0900 && cp <= 0x097F) {
        if ((cp >= 0x0900 && cp <= 0x0903) ||
            (cp >= 0x093A && cp <= 0x094F) ||
            (cp >= 0x0951 && cp <= 0x0957) ||
            (cp >= 0x0962 && cp <= 0x0963))
            return true;
    }
    return false;
}

void Terminal::apply_combining(const std::string& mark) {
    if (screen_cursor_row < 0 || screen_cursor_row >= screen_rows)
        return;
    if (screen_cursor_col < 0 || screen_cursor_col >= screen_cols)
        return;

    int target_row = screen_cursor_row;
    int target_col = screen_cursor_col;

    if (target_col > 0)
        --target_col;

    auto& cell = screen_buffer[target_row][target_col];
    if (!cell.content.empty())
        cell.content += mark;
}

// ------------------------------------------------------------
// Screen text handling
// ------------------------------------------------------------

void Terminal::handle_print_text(const std::string& text, const TerminalAttributes& attr) {
    pending_utf8 += text;

    std::string ch;
    while (decode_next_utf8(pending_utf8, ch)) {
        uint32_t cp = decode_codepoint(ch);
        bool combining = is_combining(cp);

        if (auto_wrap_mode && wrap_next && !combining) {
            wrap_next = false;
            screen_cursor_col = 0;
            int bottom = (scroll_region_bottom == -1)
                         ? screen_rows - 1
                         : scroll_region_bottom;
            if (screen_cursor_row == bottom)
                perform_scroll_up();
            else if (screen_cursor_row + 1 < screen_rows)
                ++screen_cursor_row;
        }

        if (combining) {
            apply_combining(ch);
        } else {
            write_char(ch, attr);
        }
    }
}


void Terminal::write_char(const std::string& utf8, const TerminalAttributes& attr) {
    // Bounds check
    if (screen_cursor_row < 0 || screen_cursor_row >= screen_rows)
        return;

    if (screen_cursor_col >= screen_cols)
        screen_cursor_col = screen_cols - 1;

    auto& row = screen_buffer[screen_cursor_row];

    // ------------------------------------------------------------
    // ⭐ CRITICAL FIX:
    // Distinguish between:
    //   ""  → no write (vi leaves cell untouched)
    //   " " → visible blank cell (space)
    // ------------------------------------------------------------
    if (utf8.empty()) {
        // Empty means "no write" — do NOT modify the cell.
        return;
    }

    std::string ch = utf8;

    // Space is a visible blank cell
    if (ch == " ") {
        ch = " ";
    }

    // ------------------------------------------------------------
    // Insert mode vs overwrite mode
    // ------------------------------------------------------------
    if (insert_mode) {
        if (screen_cursor_col <= screen_cols) {
            row.insert(row.begin() + screen_cursor_col, Cell{ch, attr});
            if (row.size() > static_cast<size_t>(screen_cols))
                row.pop_back();
        }
    } else {
        if (screen_cursor_col < screen_cols)
            row[screen_cursor_col] = Cell{ch, attr};
    }

    // ------------------------------------------------------------
    // Cursor advance + wrap logic
    // ------------------------------------------------------------
    if (screen_cursor_col + 1 >= screen_cols) {
        if (auto_wrap_mode)
            wrap_next = true;
    } else {
        ++screen_cursor_col;
        wrap_next = false;
    }
}


void Terminal::newline() {
    int bottom = (scroll_region_bottom == -1) ? screen_rows - 1 : scroll_region_bottom;
    if (screen_cursor_row == bottom) {
        perform_scroll_up();
    } else if (screen_cursor_row + 1 < screen_rows) {
        ++screen_cursor_row;
    }
}

void Terminal::carriage_return() {
    screen_cursor_col = 0;
}

void Terminal::backspace() {
    if (screen_cursor_col > 0)
        --screen_cursor_col;
}

void Terminal::move_cursor(int row, int col, bool absolute) {
    if (absolute) {
        screen_cursor_row = row - 1;
        screen_cursor_col = col - 1;
    } else {
        screen_cursor_row += row;
        screen_cursor_col += col;
    }

    screen_cursor_row = std::clamp(screen_cursor_row, 0, screen_rows - 1);
    screen_cursor_col = std::clamp(screen_cursor_col, 0, screen_cols - 1);
}

void Terminal::clear_screen(int mode, const TerminalAttributes& attr) {
    if (mode == 2 || mode == 3) {
        for (auto& row : screen_buffer)
            std::fill(row.begin(), row.end(), Cell{" ", attr});
    } else if (mode == 0) {
        if (screen_cursor_row < screen_rows) {
            auto& row = screen_buffer[screen_cursor_row];
            for (int i = screen_cursor_col; i < screen_cols; ++i)
                row[i] = Cell{" ", attr};
        }
        for (int r = screen_cursor_row + 1; r < screen_rows; ++r)
            std::fill(screen_buffer[r].begin(), screen_buffer[r].end(), Cell{" ", attr});
    } else if (mode == 1) {
        for (int r = 0; r < screen_cursor_row; ++r)
            std::fill(screen_buffer[r].begin(), screen_buffer[r].end(), Cell{" ", attr});
        if (screen_cursor_row < screen_rows) {
            auto& row = screen_buffer[screen_cursor_row];
            for (int i = 0; i <= screen_cursor_col && i < screen_cols; ++i)
                row[i] = Cell{" ", attr};
        }
    }
}

void Terminal::clear_line(int mode, const TerminalAttributes& attr) {
    if (screen_cursor_row >= screen_rows)
        return;

    auto& row = screen_buffer[screen_cursor_row];
    if (mode == 0) {
        for (int i = screen_cursor_col; i < screen_cols; ++i)
            row[i] = Cell{" ", attr};
    } else if (mode == 1) {
        for (int i = 0; i <= screen_cursor_col && i < screen_cols; ++i)
            row[i] = Cell{" ", attr};
    } else if (mode == 2) {
        std::fill(row.begin(), row.end(), Cell{" ", attr});
    }
}

void Terminal::insert_lines(int count, const TerminalAttributes& attr) {
    int bottom = (scroll_region_bottom == -1) ? screen_rows - 1 : scroll_region_bottom;
    int top = scroll_region_top;

    bottom = std::min(bottom, screen_rows - 1);
    top = std::max(top, 0);

    if (screen_cursor_row < top || screen_cursor_row > bottom)
        return;

    for (int i = 0; i < count; ++i) {
        if (bottom < screen_rows) {
            screen_buffer.erase(screen_buffer.begin() + bottom);
            screen_buffer.insert(screen_buffer.begin() + screen_cursor_row,
                                 std::vector<Cell>(screen_cols, Cell{" ", attr}));
        }
    }
}

void Terminal::delete_lines(int count, const TerminalAttributes& attr) {
    int bottom = (scroll_region_bottom == -1) ? screen_rows - 1 : scroll_region_bottom;
    int top = scroll_region_top;

    bottom = std::min(bottom, screen_rows - 1);
    top = std::max(top, 0);

    if (screen_cursor_row < top || screen_cursor_row > bottom)
        return;

    for (int i = 0; i < count; ++i) {
        if (screen_cursor_row < screen_rows) {
            screen_buffer.erase(screen_buffer.begin() + screen_cursor_row);
            screen_buffer.insert(screen_buffer.begin() + bottom,
                                 std::vector<Cell>(screen_cols, Cell{" ", attr}));
        }
    }
}

void Terminal::insert_chars(int count, const TerminalAttributes& attr) {
    if (screen_cursor_row >= screen_rows)
        return;

    auto& row = screen_buffer[screen_cursor_row];
    for (int i = 0; i < count; ++i) {
        if (screen_cursor_col <= screen_cols) {
            row.insert(row.begin() + screen_cursor_col, Cell{" ", attr});
            if (row.size() > static_cast<size_t>(screen_cols))
                row.pop_back();
        }
    }
}

void Terminal::delete_chars(int count, const TerminalAttributes& attr) {
    if (screen_cursor_row >= screen_rows)
        return;

    auto& row = screen_buffer[screen_cursor_row];
    for (int i = 0; i < count; ++i) {
        if (screen_cursor_col < screen_cols) {
            row.erase(row.begin() + screen_cursor_col);
            row.push_back(Cell{" ", attr});
        }
    }
}

void Terminal::erase_chars(int count, const TerminalAttributes& attr) {
    if (screen_cursor_row >= screen_rows)
        return;

    auto& row = screen_buffer[screen_cursor_row];
    for (int i = 0; i < count; ++i) {
        if (screen_cursor_col + i < screen_cols)
            row[screen_cursor_col + i] = Cell{" ", attr};
    }
}

void Terminal::perform_scroll_up() {
    int bottom = (scroll_region_bottom == -1) ? screen_rows - 1 : scroll_region_bottom;
    int top = scroll_region_top;

    bottom = std::min(bottom, screen_rows - 1);
    top = std::max(top, 0);
    if (top > bottom) top = bottom;

    screen_buffer.erase(screen_buffer.begin() + top);
    screen_buffer.insert(screen_buffer.begin() + bottom,
                         std::vector<Cell>(screen_cols, Cell{" ", {}}));
}

void Terminal::perform_scroll_down() {
    int bottom = (scroll_region_bottom == -1) ? screen_rows - 1 : scroll_region_bottom;
    int top = scroll_region_top;

    bottom = std::min(bottom, screen_rows - 1);
    top = std::max(top, 0);
    if (top > bottom) top = bottom;

    screen_buffer.insert(screen_buffer.begin() + top,
                         std::vector<Cell>(screen_cols, Cell{" ", {}}));
    if (bottom + 1 < screen_rows)
        screen_buffer.erase(screen_buffer.begin() + bottom + 1);
    else
        screen_buffer.pop_back();
}

// ------------------------------------------------------------
// History mode helpers
// ------------------------------------------------------------

void Terminal::append_history_text(const std::string& text, const TerminalAttributes& attr) {
    if (active_line.segments.empty()) {
        active_line.segments.push_back({text, attr});
        return;
    }

    auto& last = active_line.segments.back();

    auto same_color = [](const TerminalColor& a, const TerminalColor& b) {
        return a.type == b.type &&
               a.ansi_color == b.ansi_color &&
               a.indexed_color == b.indexed_color &&
               a.r == b.r && a.g == b.g && a.b == b.b;
    };

    bool same_fg = same_color(last.attributes.foreground, attr.foreground);
    bool same_bg = same_color(last.attributes.background, attr.background);

    if (same_fg && same_bg && last.attributes.bold == attr.bold) {
        last.content += text;
    } else {
        active_line.segments.push_back({text, attr});
    }
}

void Terminal::finalize_history_line() {
    parsed_buffer.push_back(active_line);
    active_line = ParsedLine{};
}

void Terminal::backspace_history() {
    if (active_line.segments.empty())
        return;

    auto& last = active_line.segments.back();
    if (last.content.empty()) {
        active_line.segments.pop_back();
        return;
    }

    while (!last.content.empty()) {
        unsigned char c = static_cast<unsigned char>(last.content.back());
        last.content.pop_back();
        if ((c & 0xC0) != 0x80)
            break;
    }

    if (last.content.empty())
        active_line.segments.pop_back();
}

// ------------------------------------------------------------
// Scrolling API
// ------------------------------------------------------------

void Terminal::scroll_up() {
    if (scroll_offset < static_cast<int>(parsed_buffer.size()))
        ++scroll_offset;
}

void Terminal::scroll_down() {
    if (scroll_offset > 0)
        --scroll_offset;
}

void Terminal::scroll_page_up() {
    scroll_offset += screen_rows;
    if (scroll_offset > static_cast<int>(parsed_buffer.size()))
        scroll_offset = static_cast<int>(parsed_buffer.size());
}

void Terminal::scroll_page_down() {
    scroll_offset -= screen_rows;
    if (scroll_offset < 0)
        scroll_offset = 0;
}

void Terminal::scroll_to_bottom() {
    scroll_offset = 0;
}

