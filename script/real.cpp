#include <iostream>
#include <print>
#include <string>
#include <vector>
#include <unistd.h>     // For fork(), execvp(), read(), write()
#include <fcntl.h>      // For fcntl(), O_NONBLOCK
#include <termios.h>    // For terminal control functions
#include <sys/ioctl.h>  // For ioctl(), TIOCGWINSZ
#include <stdlib.h>     // For grantpt(), ptsname(), unlockpt(), getenv()
#include <sys/select.h> // For select()
#include <algorithm>    // For std::max
#include <cctype>       // For isdigit()

/**
 * @brief Sets up a pseudoterminal (PTY) and launches a shell.
 * This function creates a communication channel between the application
 * (PTY master) and a shell (PTY slave).
 * @return The file descriptor of the PTY master. Returns -1 on failure.
 */
int setup_pty_and_shell() {
    int pty_master_fd;
    char *pty_slave_name;

    // Open the PTY master
    pty_master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty_master_fd == -1) {
        std::println(std::cerr,"Error: Failed to open PTY master.");;
        return -1;
    }

    // Grant access to the PTY slave
    if (grantpt(pty_master_fd) == -1 || unlockpt(pty_master_fd) == -1) {
        std::println(std::cerr,"Error: Failed to grant/unlock PTY slave.");;
        close(pty_master_fd);
        return -1;
    }
    
    // Get the PTY slave device name
    pty_slave_name = ptsname(pty_master_fd);
    if (!pty_slave_name) {
        std::println(std::cerr,"Error: Failed to get PTY slave name.");;
        close(pty_master_fd);
        return -1;
    }

    // Fork a new process
    pid_t pid = fork();
    if (pid < 0) {
        std::println(std::cerr,"Error: Failed to fork process.");;
        close(pty_master_fd);
        return -1;
    }

    if (pid == 0) {
        // Child process logic: this will become the shell.
        setsid();
        int pty_slave_fd = open(pty_slave_name, O_RDWR);
        if (pty_slave_fd < 0) {
            std::println(std::cerr,"Error: Child failed to open PTY slave.");;
            exit(1);
        }
        ioctl(pty_slave_fd, TIOCSCTTY, 0);
        dup2(pty_slave_fd, STDIN_FILENO);
        dup2(pty_slave_fd, STDOUT_FILENO);
        dup2(pty_slave_fd, STDERR_FILENO);
        
        if (pty_slave_fd > STDERR_FILENO) {
            close(pty_slave_fd);
        }
        close(pty_master_fd);

        // Execute the shell
        char *shell = getenv("SHELL");
        if (shell == nullptr) {
            shell = (char *)"/bin/bash";
        }
        execlp(shell, shell, nullptr);

        std::println(std::cerr,"Error: Failed to exec shell.");;
        _exit(1);
    } else {
        // Parent process logic
        fcntl(pty_master_fd, F_SETFL, O_NONBLOCK);
        return pty_master_fd;
    }
}

// A simple terminal state to hold the content
struct terminal_state {
    int cursor_x = 0;
    int cursor_y = 0;
};

// ANSI escape sequence parser states
enum class AnsiParserState {
    Normal,
    Escape,
    CSI, // Control Sequence Introducer
};

/**
 * @brief Parses ANSI escape sequences and prints to the console.
 * This is a minimal implementation that only handles basic formatting codes.
 */
class AnsiParser {
public:
    AnsiParser(terminal_state& state) : m_state(state) {}

    void parse(const std::string& data) {
        for (char c : data) {
            switch (m_parser_state) {
                case AnsiParserState::Normal:
                    handle_normal(c);
                    break;
                case AnsiParserState::Escape:
                    handle_escape(c);
                    break;
                case AnsiParserState::CSI:
                    handle_csi(c);
                    break;
            }
        }
    }

private:
    terminal_state& m_state;
    AnsiParserState m_parser_state = AnsiParserState::Normal;
    std::string m_parameter_buffer;
    
    // Prints a single character to the console and updates the cursor position.
    void write_char(char c) {
        std::print("{}",c);
        if (c == '\n') {
            m_state.cursor_x = 0;
            m_state.cursor_y++;
        } else {
            m_state.cursor_x++;
        }
    }

    // Handles characters in the normal state.
    void handle_normal(char c) {
        if (c == '\x1B') {
            m_parser_state = AnsiParserState::Escape;
        } else if (c == '\r') {
            m_state.cursor_x = 0;
        } else if (c == '\n') {
            write_char('\n');
        } else {
            write_char(c);
        }
    }

    // Handles characters after a '\x1B' escape character.
    void handle_escape(char c) {
        if (c == '[') {
            m_parser_state = AnsiParserState::CSI;
            m_parameter_buffer.clear();
        } else {
            // Unhandled escape sequence, return to normal.
            m_parser_state = AnsiParserState::Normal;
        }
    }

    // Handles characters for CSI sequences (\x1B[... )
    void handle_csi(char c) {
        if (isdigit(c) || c == ';') {
            m_parameter_buffer += c;
        } else if (c == 'm') {
            // SGR (Select Graphic Rendition) command.
            // Split parameters and apply them.
            if (m_parameter_buffer.empty()) {
                // No parameters, means reset all attributes.
                std::cout << "\x1B[0m";
            } else {
                std::cout << "\x1B[" << m_parameter_buffer << "m";
            }
            m_parser_state = AnsiParserState::Normal;
        } else if (c == 'H' || c == 'f') {
            // CUP (Cursor Position) command.
            // We don't track the screen grid, so we'll just ignore this,
            // but in a full terminal, you'd move the cursor here.
            m_parser_state = AnsiParserState::Normal;
        } else {
            // Unhandled CSI sequence, return to normal.
            m_parser_state = AnsiParserState::Normal;
        }
    }
};

int main() {
    // Save original terminal settings to restore later
    struct termios original_termios;
    tcgetattr(STDIN_FILENO, &original_termios);
    
    // Set terminal to raw mode to handle all key presses
    struct termios raw_termios = original_termios;
    raw_termios.c_lflag &= ~(ECHO | ICANON | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);

    // Setup the PTY and shell
    int pty_fd = setup_pty_and_shell();
    if (pty_fd == -1) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        return 1;
    }

    std::println("Starting terminal emulator... press Ctrl+C to exit.");
    
    terminal_state terminal_state_instance;
    AnsiParser parser(terminal_state_instance);
    
    char buffer[4096];
    
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(pty_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int max_fd = std::max(pty_fd, STDIN_FILENO);
        
        if (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
            if (errno == EINTR) continue; // Handle interrupted system calls
            perror("select");
            break;
        }

        // Check for data from the PTY
        if (FD_ISSET(pty_fd, &read_fds)) {
            ssize_t bytes_read = read(pty_fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                parser.parse(std::string(buffer, bytes_read));
                std::cout.flush();
            } else if (bytes_read == 0) {
                // Shell has terminated
                std::println(std::cerr,"\r\nShell terminated.");;
                break;
            }
        }

        // Check for data from the keyboard (user input)
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                if (buffer[0] == '\x03') { // Ctrl+C
                    break;
                }
                write(pty_fd, buffer, bytes_read);
            }
        }
    }

    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    close(pty_fd);
    return 0;
}

