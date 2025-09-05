#ifndef TERMINAL_H
#define TERMINAL_H

#include <GLFW/glfw3.h>
#include <string>
#include "text_renderer.h"
#include "tty.h"
#include "terminal_parser.h"

// Terminal state management
class Terminal {
public:
    Terminal(int width,int height);
    ~Terminal();
    void render_cursor(float x, float y,float scale,int width=0, int height=0);
    
    // Terminal operations
    void set_renderer(TextRenderer* renderer);
    std::string get_prompt();
    void render_prompt(float x, float y, float scale);
    //void render_cursor(float x, float y, float scale);
    void handle_input(GLFWwindow* window);
    void render_input(float x, float y, float scale);
    void set_window_size(float width, float height);
    
    // Getters
    const std::string& get_input_buffer() const { return input_buffer; }
    bool is_input_active() const { return input_active; }
    float get_prompt_width() const;
    float get_input_width() const;
    void show_buffer();
    void show_input_buffer();
    void key_pressed(char c, int type);
    
    // Input control
    void activate_input() { input_active = true; }
    void clear_input() { input_buffer.clear(); }
    void command_test();
    std::string poll_output();
    tty term;
    
public:
    std::string input_buffer;
    bool input_active;
    float win_width;
    float win_height;
    
    // Input handling state
    double last_cursor_time;
    bool cursor_visible;
    bool backspace_pressed;
    bool enter_pressed;
    //bool key_pressed[26];
    bool space_pressed;
    
    // Helper functions
    void update_cursor_blink();

    TextRenderer* text_renderer;
    std::vector<std::string> text_buffer;
    std::vector<ParsedLine> parsed_buffer;
    Coord cursor_pos;
    TerminalParser parser;
    
    // Color management
    void get_color_for_line_type(LineType type, float* color);
    void get_color_for_attributes(const TerminalAttributes& attrs, float* color);
};

#endif // TERMINAL_H

