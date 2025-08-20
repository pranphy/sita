#include <iostream>
#include<print>
#include "tty.h"

tty::tty(std::string shell_path){
    //set_terminal_raw_mode();
    //std::atexit(restore_terminal_mode); // Ensure terminal mode is restored on exit
    init_terminal_buffer();
    setup_pty(shell_path);
    line_no = 0;
}

// Function to initialize the terminal screen buffer
void tty::init_terminal_buffer() {
    cursor.x = 0;
    cursor.y = 0;
}

std::vector<char> tty::intrepret_bytes(char* buffer, int bytes_read){
    std::vector<char> read;
    if (bytes_read > 0) {
        // Process the buffer character by character
        for (ssize_t i = 0; i < bytes_read; ++i) {
            char c = buffer[i];
            //current_line.push_back(c);
            switch (current_state) {
                case STATE_NORMAL:
                    if (c == '\033') { // Escape sequence
                        current_state = STATE_ESCAPE;
                        escape_sequence_buffer.clear();
                    } else if (c == '\n') {
                        cursor.x = 0;
                        cursor.y++;
                        read.push_back(c);
                        current_line.push_back(c);
                        screen_buffer.push_back(current_line);
                        std::println("Screen buffer updated to size {}",screen_buffer.size());
                        current_line.clear();
                    } else {
                        read.push_back(c);
                        current_line.push_back(c); // we filter only normal mode characters and not have newline
                    }
                    break;
                case STATE_ESCAPE:
                    if (c == '[') {
                        current_state = STATE_CSI;
                    } else {
                        // Handle other escape sequences
                        current_state = STATE_NORMAL;
                    }
                    break;
                case STATE_CSI:
                    if ((c >= '0' && c <= '9') || c == ';') {
                        escape_sequence_buffer += c;
                    } else if (c == 'm') { // SGR (Select Graphic Rendition) command
                                           // Placeholder: Process color codes here
                        current_state = STATE_NORMAL;
                        escape_sequence_buffer.clear();
                    } else if (c == 'K') { // Erase to end of line
                        for(int col = cursor.x; col < MAX_COLS; ++col) {
                            //screen_buffer[cursor.y][col] = ' ';
                        }
                        current_state = STATE_NORMAL;
                        escape_sequence_buffer.clear();
                    } else if (c == 'H' || c == 'f') { // Cursor position
                        int row = 0, col = 0;
                        size_t semicolon_pos = escape_sequence_buffer.find(';');
                        if (semicolon_pos != std::string::npos) {
                            row = std::stoi(escape_sequence_buffer.substr(0, semicolon_pos));
                            col = std::stoi(escape_sequence_buffer.substr(semicolon_pos + 1));
                        } else if (!escape_sequence_buffer.empty()) {
                            row = std::stoi(escape_sequence_buffer);
                        }
                        cursor.x = col > 0 ? col - 1 : 0;
                        cursor.y = row > 0 ? row - 1 : 0;
                        current_state = STATE_NORMAL;
                        escape_sequence_buffer.clear();
                    } else {
                        current_state = STATE_NORMAL;
                        escape_sequence_buffer.clear();
                    }
                    break;
            }
        }
        std::println("Rendering to console ");

        render_to_console();
    } else if (bytes_read == 0) {
        running = false;
    }
    return read;
}

// Function to handle the output from the shell.
std::string tty::handle_pty_output() {
    pollfd fds{.fd = pty_master_fd, .events = POLLIN};
    std::string op;

    if (poll(&fds, 1, 0) > 0) {
        if (fds.revents & POLLIN) {
            char buffer[4096];
            ssize_t bytes_read = read(pty_master_fd, buffer, sizeof(buffer) - 1);
            std::println("I read {} bytes ",bytes_read);
            auto read = intrepret_bytes(buffer, bytes_read);
            op += std::string(read.begin(),read.end());
        }
    }
    return op;
}

// Function to set up the pseudo-terminal
void tty::setup_pty(std::string shell_path) {
    int pty_slave_fd;
    if (openpty(&pty_master_fd, &pty_slave_fd, NULL, NULL, NULL) == -1) {
        std::println(std::cerr,"Error: openpty() failed.");;
        exit(1);
    }

    shell_pid = fork();
    if (shell_pid == -1) {
        std::println(std::cerr,"Error: fork() failed.");;
        exit(1);
    }

    if (shell_pid == 0) { // Child process
        close(pty_master_fd);
        dup2(pty_slave_fd, STDIN_FILENO);
        dup2(pty_slave_fd, STDOUT_FILENO);
        dup2(pty_slave_fd, STDERR_FILENO);

        if (pty_slave_fd > 2) {
            close(pty_slave_fd);
        }

        std::vector<const char*> argv;
        argv.push_back(shell_path.c_str());
        argv.push_back(nullptr);

        execvp(argv[0], (char* const*)argv.data());

        std::println(std::cerr,"Error: execvp() failed.");
        exit(1);
    } else { // Parent process
        close(pty_slave_fd);
    }
}

// Cleans up the child process
void tty::cleanup_child_process() {
    if (shell_pid > 0) {
        kill(shell_pid, SIGTERM);
        int status;
        waitpid(shell_pid, &status, 0); // Wait for the child to exit
    }
}

// Function to write a character to the pty
void tty::write_to_pty(int c) {
    char key_char = static_cast<char>(c);
    write(pty_master_fd, &key_char, 1);
}

// Function to render the screen buffer to the console
void tty::render_to_console() {
    std::println("The screen buffer size is {} and line no is {}",screen_buffer.size(),line_no);
    for(; line_no < screen_buffer.size(); ++line_no){
        for(auto& col : screen_buffer[line_no]){
            std::cout << col;
        }
    }
}

// Set terminal to raw mode for character-by-character input
void tty::set_terminal_raw_mode() {
    termios new_termios;
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

// Restore original terminal mode on exit
void tty::restore_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
}

void tty::add_to_screen_buffer(std::string s){
    for (char c : s) {
        current_line.push_back(c);
    }
    screen_buffer.push_back(current_line);
    current_line.clear();
}

void tty::close_master(){
    close(pty_master_fd);
}

void tty::main_loop(){
}
