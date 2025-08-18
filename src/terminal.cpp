#include <print>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include "tty.h"
#include "oglutil.h"
#include "terminal.h"
#include "utils.h"


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
    auto result = execute_command("ip -c a");
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
        float color[] = {0.2f, 1.0f, 1.0f, 1.0f}; // White color
        for(auto& text : text_buffer){
            for(auto& word : utl::split_by_devanagari(text)){
                cursor_pos = text_renderer->render_text_harfbuzz(word, cursor_pos, 1.0f, color, win_width, win_height);
            }
            cursor_pos.y -= 50.0f;
            cursor_pos.x = 25.0f;
        }
        text_renderer->render_text_harfbuzz(input_buffer, cursor_pos, 1.0f, color, win_width, win_height);
    }
    command_test();
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
        if (!enter_pressed) {
            // Process command here
            std::println("Command: {} " , input_buffer );
            text_buffer.push_back(input_buffer);
            input_buffer.clear();
            input_active = false;
            cursor_pos.y -= 20.0f;
        }
        enter_pressed = true;
    } else {
        enter_pressed = false;
    }
    
    // Handle character input
    if (type == 0) {
            input_buffer += c;
            //std::println("Input buffer so far {}", input_buffer);
    } else if (type == 32) {
        input_buffer += ' ';
    }

    show_buffer();
}

