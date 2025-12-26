#include "terminal_view.h"
#include "oglutil.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <iostream>

TerminalView::TerminalView(Terminal& term)
    : terminal(term)
{
    last_cursor_time = glfwGetTime();
}

TerminalView::~TerminalView() = default;

void TerminalView::set_renderer(TextRenderer* renderer) {
    text_renderer = renderer;
    update_dimensions();
}

void TerminalView::set_window_size(float width, float height) {
    win_width  = width;
    win_height = height;

    int rows = static_cast<int>(height / LINE_HEIGHT);
    int cols = static_cast<int>(width  / CELL_WIDTH);
    terminal.set_window_size(rows, cols);
}

void TerminalView::update_dimensions() {
    if (!text_renderer)
        return;

    CELL_WIDTH  = text_renderer->get_char_width();
    LINE_HEIGHT = text_renderer->get_line_height();
}

void TerminalView::update_cursor_blink() {
    double now = glfwGetTime();
    if (now - last_cursor_time >= 0.5) {
        cursor_visible   = !cursor_visible;
        last_cursor_time = now;
    }
}

void TerminalView::render() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!text_renderer)
        return;

    if (terminal.alternate_screen_active) {
        render_alternate_screen();
    } else {
        render_history_mode();
    }

    if (!terminal.get_preedit().empty()) {
        render_preedit(cursor_pos.x, cursor_pos.y);
    } else if (cursor_visible && terminal.cursor_visible) {
        render_cursor(cursor_pos.x, cursor_pos.y);
    }
}

// ------------------------------------------------------------
// Alternate screen (modernized, non-gridified)
// ------------------------------------------------------------
void TerminalView::render_alternate_screen() {
    float y = win_height - LINE_HEIGHT;

    ParsedLine line;
    line.type = LineType::UNKNOWN;
    line.clear_screen = false;

    for (int row = 0; row < terminal.screen_rows; ++row) {
        line.segments.clear();

        const auto& cells = terminal.screen_buffer[row];
        if (cells.empty()) {
            y -= LINE_HEIGHT;
            continue;
        }

        TerminalAttributes current_attrs = cells[0].attributes;
        std::string current_text;

        for (int col = 0; col < terminal.screen_cols; ++col) {
            const Cell& cell = cells[col];

            bool attrs_match =
                cell.attributes.foreground == current_attrs.foreground &&
                cell.attributes.background == current_attrs.background &&
                cell.attributes.bold       == current_attrs.bold &&
                cell.attributes.reverse    == current_attrs.reverse;

            bool last_cell = (col == terminal.screen_cols - 1);

            if (!attrs_match || last_cell) {
                if (attrs_match)
                    current_text += cell.content;

                if (!current_text.empty()) {
                    line.segments.push_back(Segment{
                        .content    = current_text,
                        .attributes = current_attrs
                    });
                }

                current_text.clear();
                current_attrs = cell.attributes;

                if (!attrs_match)
                    current_text += cell.content;
            } else {
                current_text += cell.content;
            }
        }

        if (!line.segments.empty()) {
            float row_y = y;
            render_line(line, row_y);
        }

        y -= LINE_HEIGHT;
    }

    cursor_pos.x = 25.0f + terminal.screen_cursor_col * CELL_WIDTH;
    cursor_pos.y = win_height - LINE_HEIGHT - terminal.screen_cursor_row * LINE_HEIGHT;
}

// ------------------------------------------------------------
// History mode (unchanged except cleanup)
// ------------------------------------------------------------
void TerminalView::render_history_mode() {
    float start_y = win_height - LINE_HEIGHT;
    cursor_pos     = {25.0f, start_y};

    size_t total_lines = terminal.parsed_buffer.size() + 1;
    size_t max_lines   = (win_height / LINE_HEIGHT) - 1;

    int max_offset = 0;
    if (total_lines > max_lines) {
        max_offset = static_cast<int>(total_lines - max_lines);
    }

    terminal.scroll_offset =
        std::clamp(terminal.scroll_offset, 0, max_offset);

    size_t start_index = 0;
    if (total_lines > max_lines) {
        start_index = total_lines - max_lines - terminal.scroll_offset;
    }

    float y = start_y;

    for (size_t i = 0; i < terminal.parsed_buffer.size(); ++i) {
        if (i >= start_index && i < start_index + max_lines)
            render_line(terminal.parsed_buffer[i], y);
    }

    if (terminal.parsed_buffer.size() >= start_index &&
        terminal.parsed_buffer.size() < start_index + max_lines) {

        float active_y = y;
        render_line(terminal.active_line, y);

        float cx = 25.0f;
        float cy = active_y;
        float limit = 25.0f + terminal.screen_cols * CELL_WIDTH;

        for (const auto& seg : terminal.active_line.segments) {
            for (const auto& chunk : utl::split_by_devanagari(seg.content)) {
                size_t pos = 0;
                unsigned int cp = utl::get_next_codepoint(chunk, pos);

                if (utl::is_devanagari(cp)) {
                    float w = text_renderer->measure_text_width(chunk, 1.0f);
                    if (cx + w > limit) {
                        cy -= LINE_HEIGHT;
                        cx = 25.0f;
                    }
                    cx += w;
                } else {
                    size_t p = 0;
                    while (p < chunk.size()) {
                        utl::get_next_codepoint(chunk, p);
                        if (cx + CELL_WIDTH > limit) {
                            cy -= LINE_HEIGHT;
                            cx = 25.0f;
                        }
                        cx += CELL_WIDTH;
                    }
                }
            }
        }

        cursor_pos.x = cx;
        cursor_pos.y = cy;
    }
}


// ------------------------------------------------------------
// Line rendering (unchanged except cleanup)
// ------------------------------------------------------------

void TerminalView::render_line(const ParsedLine& line, float& y_pos) {
    float x = 25.0f;
    float limit = 25.0f + terminal.screen_cols * CELL_WIDTH;

    for (const auto& seg : line.segments) {
        float fg[4], bg[4];

        if (seg.attributes.reverse) {
            get_color(seg.attributes.background, fg, true);
            get_color(seg.attributes.foreground, bg, false);
        } else {
            get_color_for_attributes(seg.attributes, fg);
            get_color(seg.attributes.background, bg, true);
        }

        for (const auto& chunk : utl::split_by_devanagari(seg.content)) {
            size_t pos = 0;
            unsigned int cp = utl::get_next_codepoint(chunk, pos);
            float baseline = LINE_HEIGHT * 0.25f;

            if (utl::is_devanagari(cp)) {
                float w = text_renderer->measure_text_width(chunk, 1.0f);
                if (x + w > limit) {
                    y_pos -= LINE_HEIGHT;
                    x = 25.0f;
                }

                text_renderer->draw_solid_rectangle(
                    x, y_pos, w, LINE_HEIGHT,
                    bg, win_width, win_height);

                text_renderer->render_text_harfbuzz(
                    chunk, {x, y_pos + baseline}, 1.0f,
                    fg, win_width, win_height);

                x += w;
            } else {
                size_t p = 0;
                while (p < chunk.size()) {
                    size_t prev = p;
                    utl::get_next_codepoint(chunk, p);
                    std::string ch = chunk.substr(prev, p - prev);

                    if (x + CELL_WIDTH > limit) {
                        y_pos -= LINE_HEIGHT;
                        x = 25.0f;
                    }

                    text_renderer->draw_solid_rectangle(
                        x, y_pos, CELL_WIDTH, LINE_HEIGHT,
                        bg, win_width, win_height);

                    text_renderer->render_text_harfbuzz(
                        ch, {x, y_pos + baseline}, 1.0f,
                        fg, win_width, win_height);

                    x += CELL_WIDTH;
                }
            }
        }
    }

    y_pos -= LINE_HEIGHT;
}

// ------------------------------------------------------------
// Cursor & preedit
// ------------------------------------------------------------

void TerminalView::render_cursor(float x, float y) {
    float color[4] = {1.f, 1.f, 1.f, 1.f};
    text_renderer->draw_solid_rectangle(
        x, y, CELL_WIDTH, LINE_HEIGHT,
        color, win_width, win_height);
}

void TerminalView::render_preedit(float x, float y) {
    const std::string& pre = terminal.get_preedit();
    if (pre.empty())
        return;

    float cx = x;
    float baseline = LINE_HEIGHT * 0.25f;

    float fg[4] = {1.f, 1.f, 1.f, 1.f};
    float bg[4] = {0.2f, 0.2f, 0.2f, 1.f};

    for (const auto& chunk : utl::split_by_devanagari(pre)) {
        size_t pos = 0;
        unsigned int cp = utl::get_next_codepoint(chunk, pos);

        if (utl::is_devanagari(cp)) {
            float w = text_renderer->measure_text_width(chunk, 1.0f);

            text_renderer->draw_solid_rectangle(
                cx, y, w, LINE_HEIGHT,
                bg, win_width, win_height);

            text_renderer->render_text_harfbuzz(
                chunk, {cx, y + baseline}, 1.0f,
                fg, win_width, win_height);

            float underline_y = y + baseline - 2.0f;
            text_renderer->draw_solid_rectangle(
                cx, underline_y, w, 2.0f,
                fg, win_width, win_height);

            cx += w;
        } else {
            size_t p = 0;
            while (p < chunk.size()) {
                size_t prev = p;
                utl::get_next_codepoint(chunk, p);
                std::string ch = chunk.substr(prev, p - prev);

                text_renderer->draw_solid_rectangle(
                    cx, y, CELL_WIDTH, LINE_HEIGHT,
                    bg, win_width, win_height);

                text_renderer->render_text_harfbuzz(
                    ch, {cx, y + baseline}, 1.0f,
                    fg, win_width, win_height);

                float underline_y = y + baseline - 2.0f;
                text_renderer->draw_solid_rectangle(
                    cx, underline_y, CELL_WIDTH, 2.0f,
                    fg, win_width, win_height);

                cx += CELL_WIDTH;
            }
        }
    }
}


void TerminalView::get_color(const TerminalColor &color,
                             float *out_color,
                             bool is_bg)
{
    if (color.type == TerminalColor::Type::RGB) {
        out_color[0] = color.r / 255.0f;
        out_color[1] = color.g / 255.0f;
        out_color[2] = color.b / 255.0f;
        out_color[3] = 1.0f;
        return;
    }

    if (color.type == TerminalColor::Type::INDEXED) {
        // Simple fallback
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
        return;
    }

    if (color.type == TerminalColor::Type::ANSI) {
        switch (color.ansi_color) {
        case AnsiColor::BLACK:         out_color[0]=0.0f; out_color[1]=0.0f; out_color[2]=0.0f; break;
        case AnsiColor::RED:           out_color[0]=0.8f; out_color[1]=0.0f; out_color[2]=0.0f; break;
        case AnsiColor::GREEN:         out_color[0]=0.0f; out_color[1]=0.8f; out_color[2]=0.0f; break;
        case AnsiColor::YELLOW:        out_color[0]=0.8f; out_color[1]=0.8f; out_color[2]=0.0f; break;
        case AnsiColor::BLUE:          out_color[0]=0.0f; out_color[1]=0.0f; out_color[2]=0.8f; break;
        case AnsiColor::MAGENTA:       out_color[0]=0.8f; out_color[1]=0.0f; out_color[2]=0.8f; break;
        case AnsiColor::CYAN:          out_color[0]=0.0f; out_color[1]=0.8f; out_color[2]=0.8f; break;
        case AnsiColor::WHITE:         out_color[0]=0.9f; out_color[1]=0.9f; out_color[2]=0.9f; break;
        case AnsiColor::BRIGHT_BLACK:  out_color[0]=0.5f; out_color[1]=0.5f; out_color[2]=0.5f; break;
        case AnsiColor::BRIGHT_RED:    out_color[0]=1.0f; out_color[1]=0.0f; out_color[2]=0.0f; break;
        case AnsiColor::BRIGHT_GREEN:  out_color[0]=0.0f; out_color[1]=1.0f; out_color[2]=0.0f; break;
        case AnsiColor::BRIGHT_YELLOW: out_color[0]=1.0f; out_color[1]=1.0f; out_color[2]=0.0f; break;
        case AnsiColor::BRIGHT_BLUE:   out_color[0]=0.0f; out_color[1]=0.0f; out_color[2]=1.0f; break;
        case AnsiColor::BRIGHT_MAGENTA:out_color[0]=1.0f; out_color[1]=0.0f; out_color[2]=1.0f; break;
        case AnsiColor::BRIGHT_CYAN:   out_color[0]=0.0f; out_color[1]=1.0f; out_color[2]=1.0f; break;
        case AnsiColor::BRIGHT_WHITE:  out_color[0]=1.0f; out_color[1]=1.0f; out_color[2]=1.0f; break;
        default:                       out_color[0]=1.0f; out_color[1]=1.0f; out_color[2]=1.0f; break;
        }
        out_color[3] = 1.0f;
        return;
    }

    // DEFAULT
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
}

void TerminalView::get_color_for_attributes(const TerminalAttributes& attrs,
                                            float* color)
{
    get_color(attrs.foreground, color, false);

    if (attrs.bold &&
        attrs.foreground.type == TerminalColor::Type::ANSI &&
        attrs.foreground.ansi_color < AnsiColor::BRIGHT_BLACK)
    {
        for (int i = 0; i < 3; ++i)
            color[i] = std::min(1.0f, color[i] + 0.5f);
    }
}

