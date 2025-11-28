#pragma once

#include <unistd.h>     // For fork(), read(), write()
#include <pty.h>        // For openpty()
#include <termios.h>    // For termios
#include <sys/ioctl.h>  // For ioctl()
#include <sys/wait.h>   // For waitpid()
#include <poll.h>       // For poll()
#include <signal.h>     // For signal handling
#include <termios.h>    // For raw mode

#include <vector>
#include <string>


// Represents the grid of characters on the screen
struct vec2{
    int x = 0;
    int y = 0;
};


// --- Global State Definitions ---
struct tty{
    tty(std::string shell_path="/bin/bash");
    //std::vector<std::vector<char>> screen_buffer;
    std::vector<std::vector<char>> screen_buffer;
    std::vector<char> current_line;
    vec2 cursor;
    const int MAX_LINES = 25;
    const int MAX_COLS = 80;
    int pty_master_fd;
    pid_t shell_pid;
    void init();
    unsigned line_no;

    static struct termios old_termios;

    static bool running; // true;

    // --- Function Prototypes ---
    void sig_handler(int signal);
    void init_terminal_buffer();
    std::string handle_pty_output();
    void setup_pty(std::string shell_path);
    void cleanup_child_process();
    void write_to_pty(int c);
    void render_to_console();
    void set_terminal_raw_mode();
    static void restore_terminal_mode();

    void add_to_screen_buffer(std::string);
    void close_master();
    void main_loop();

};

