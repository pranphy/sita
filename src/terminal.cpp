#include <print>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include "oglutil.h"
#include "terminal.h"
#include "utils.h"
#include "terminal_parser.h"

bool tty::running{true};
termios tty::old_termios;

std::vector<std::string> readlines(std::string filename,std::vector<std::string>& all_lines, unsigned int nol=0){
    std::string line; std::ifstream fileobj(filename);
    while(std::getline(fileobj,line)){
        all_lines.push_back(std::move(line));
        if(all_lines.size() >= nol) break;
    }
    return all_lines;
}

Terminal::Terminal(int width, int height) : input_active(true), last_cursor_time(0.0), cursor_visible(true),
                       backspace_pressed(false), enter_pressed(false), space_pressed(false) {
    // Initialize key_pressed array

    win_width = width;
    win_height = height;
    cursor_pos = {25.0f, height - 50.0f};

    text_buffer = {
        "pranphy@localhost>= ",
    };
    readlines("/home/pranphy/Documents/deva.txt", text_buffer, 12);
    text_renderer = nullptr;
}

void Terminal::set_window_size(float width, float height) {
    win_width = width;
    win_height = height;

    cursor_pos = {25.0f, height - 50.0f};
    //std::println("Changed sized of terminal to {}x{}", width, height);
}

Terminal::~Terminal() {
    // Cleanup if needed
    cursor_pos  = {0.0f, 0.0f};
}

std::string Terminal::get_prompt() {
    // Get username
    const char* username = getenv("USER");
    if (!username) username = "user";
    
    // Get hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "hostname");
    }
    
    // Get current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        // Shorten the path to show only the last directory
        char* last_slash = strrchr(cwd, '/');
        if (last_slash && last_slash != cwd) {
            strcpy(cwd, last_slash + 1);
        }
    } else {
        strcpy(cwd, "~");
    }
    
    return std::string(username) + "@" + hostname + ":" + cwd + "$ ";
}

void Terminal::render_prompt(float x, float y, float scale) {
    std::string prompt = get_prompt();
    // Note: This will call the renderer's render_text function
    // We'll need to pass the renderer instance or make this a friend class
}

void Terminal::render_cursor(float x, float y, float scale,int width, int height) {
    if (!cursor_visible) return;
    oglutil::draw_rectangle(x, y, scale*8);
    
    
}

void Terminal::handle_input(GLFWwindow* window) {
    update_cursor_blink();
    
    if (input_active) {
        //process_key_input(window);
    }
}

void Terminal::render_input(float x, float y, float scale) {
    // Note: This will call the renderer's render_text function
    // We'll need to pass the renderer instance or make this a friend class
}

float Terminal::get_prompt_width() const {
    // Note: This will call the renderer's calculate_text_width function
    // We'll need to pass the renderer instance or make this a friend class
    return 0.0f; // Placeholder
}

float Terminal::get_input_width() const {
    // Note: This will call the renderer's calculate_text_width function
    // We'll need to pass the renderer instance or make this a friend class
    return 0.0f; // Placeholder
}
void Terminal::command_test(){
    //auto result = execute_command("ip -c a");
    auto result = "hello world";
    std::println("Result: {}", result);
    float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    for(auto& line : utl::split_by_newline(result)){
        cursor_pos = text_renderer->render_text_harfbuzz(line, cursor_pos, 1.0f, white, win_width, win_height);
        cursor_pos.y -= 50.0f;
        cursor_pos.x = 25.0f;
    }
}

void Terminal::show_buffer(){
    glClear(GL_COLOR_BUFFER_BIT);
    cursor_pos = {25.0f, win_height - 50.0f};
    if(text_renderer){
        // Show parsed buffer with proper colors and formatting
        unsigned start_no = std::max(parsed_buffer.size() - 20.0, 0.0); // show only the last 20 lines
        for(unsigned i = start_no; i < parsed_buffer.size(); i++){
            const auto& parsed_line = parsed_buffer[i];
            float color[4];
            get_color_for_line_type(parsed_line.type, color);
            
            // Split each line by newlines first, then handle each sub-line
            std::vector<std::string> sub_lines = utl::split_by_newline(parsed_line.content);
            for(const auto& sub_line : sub_lines) {
                for(auto& word : utl::split_by_devanagari(sub_line)){
                    cursor_pos = text_renderer->render_text_harfbuzz(word, cursor_pos, 1.0f, color, win_width, win_height);
                }
                cursor_pos.y -= 50.0f;
                cursor_pos.x = 25.0f;
            }
        }
        
        // Handle input buffer with newlines
        std::vector<std::string> input_lines = utl::split_by_newline(input_buffer);
        float input_color[] = {0.2f, 1.0f, 1.0f, 1.0f}; // Cyan for input
        for(const auto& line : input_lines) {
            for(auto& word : utl::split_by_devanagari(line)){
                cursor_pos = text_renderer->render_text_harfbuzz(word, cursor_pos, 1.0f, input_color, win_width, win_height);
            }
            cursor_pos.y -= 50.0f;
            cursor_pos.x = 25.0f;
        }
    }
    //command_test();
}

void Terminal::update_cursor_blink() {
    double current_time = glfwGetTime();
    if (current_time - last_cursor_time > 0.5) { // Blink every 0.5 seconds
        cursor_visible = !cursor_visible;
        last_cursor_time = current_time;
    }
}


void Terminal::set_renderer(TextRenderer* renderer) {
    text_renderer = renderer;
}

void Terminal::show_input_buffer(){
    glClear(GL_COLOR_BUFFER_BIT);
    if(text_renderer){
        float color[] = {0.2f, 1.0f, 1.0f, 1.0f}; // White color
        text_renderer->render_text_harfbuzz(input_buffer, cursor_pos, 1.0f, color, win_width, win_height);
    }
}

void Terminal::key_pressed(char c, int type) {
    // Check for backspace
    if (type == -1) {
        if (!input_buffer.empty()) {
            input_buffer.pop_back();
        }
    }
    
    // Check for enter key
    if (type == 13) {
        // Process command here
        //std::println("Command: {} " , input_buffer );
        text_buffer.push_back(input_buffer);

        input_buffer.clear();
        input_active = false;
        cursor_pos.y -= 20.0f;
        term.write_to_pty('\n');
    }
    
    // Handle character input
    if (type == 0 or type == 32) {
        input_buffer += c;
        term.write_to_pty(c);
        //std::println("Input buffer so far {}", input_buffer);
    } 

    show_buffer();
}

std::string Terminal::poll_output(){
    auto result = term.handle_pty_output();
    if(result.size() > 2){
        std::println("Result: {}", result);
        text_buffer.push_back(result);
        
        // Parse the output and add to parsed buffer
        auto parsed_lines = parser.parse_output(result);
        for(const auto& parsed_line : parsed_lines) {
            parsed_buffer.push_back(parsed_line);
        }
    }
    return result;
}

void Terminal::get_color_for_line_type(LineType type, float* color) {
    switch (type) {
        case LineType::PROMPT:
            color[0] = 0.0f;   // R
            color[1] = 1.0f;   // G
            color[2] = 0.0f;   // B
            color[3] = 1.0f;   // A
            break;
        case LineType::COMMAND_OUTPUT:
            color[0] = 1.0f;   // R
            color[1] = 1.0f;   // G
            color[2] = 1.0f;   // B
            color[3] = 1.0f;   // A
            break;
        case LineType::ERROR_OUTPUT:
            color[0] = 1.0f;   // R
            color[1] = 0.0f;   // G
            color[2] = 0.0f;   // B
            color[3] = 1.0f;   // A
            break;
        case LineType::USER_INPUT:
            color[0] = 0.2f;   // R
            color[1] = 1.0f;   // G
            color[2] = 1.0f;   // B
            color[3] = 1.0f;   // A
            break;
        default:
            color[0] = 0.8f;   // R
            color[1] = 0.8f;   // G
            color[2] = 0.8f;   // B
            color[3] = 1.0f;   // A
            break;
    }
}

void Terminal::get_color_for_attributes(const TerminalAttributes& attrs, float* color) {
    // Convert ANSI colors to RGB
    switch (attrs.foreground) {
        case AnsiColor::BLACK:
            color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f;
            break;
        case AnsiColor::RED:
            color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f;
            break;
        case AnsiColor::GREEN:
            color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f;
            break;
        case AnsiColor::YELLOW:
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f;
            break;
        case AnsiColor::BLUE:
            color[0] = 0.0f; color[1] = 0.0f; color[2] = 1.0f;
            break;
        case AnsiColor::MAGENTA:
            color[0] = 1.0f; color[1] = 0.0f; color[2] = 1.0f;
            break;
        case AnsiColor::CYAN:
            color[0] = 0.0f; color[1] = 1.0f; color[2] = 1.0f;
            break;
        case AnsiColor::WHITE:
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
            break;
        case AnsiColor::BRIGHT_RED:
            color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.5f;
            break;
        case AnsiColor::BRIGHT_GREEN:
            color[0] = 0.5f; color[1] = 1.0f; color[2] = 0.5f;
            break;
        case AnsiColor::BRIGHT_YELLOW:
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.5f;
            break;
        case AnsiColor::BRIGHT_BLUE:
            color[0] = 0.5f; color[1] = 0.5f; color[2] = 1.0f;
            break;
        case AnsiColor::BRIGHT_MAGENTA:
            color[0] = 1.0f; color[1] = 0.5f; color[2] = 1.0f;
            break;
        case AnsiColor::BRIGHT_CYAN:
            color[0] = 0.5f; color[1] = 1.0f; color[2] = 1.0f;
            break;
        case AnsiColor::BRIGHT_WHITE:
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
            break;
        default:
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f;
            break;
    }
    color[3] = 1.0f; // Alpha
}

