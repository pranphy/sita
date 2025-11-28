#include "tty.h"
#include <iostream>
#include <print>

tty::tty(std::string shell_path) {
  // set_terminal_raw_mode();
  // std::atexit(restore_terminal_mode); // Ensure terminal mode is restored on
  // exit
  init_terminal_buffer();
  setup_pty(shell_path);
  line_no = 0;
}

// Function to initialize the terminal screen buffer
void tty::init_terminal_buffer() {
  cursor.x = 0;
  cursor.y = 0;
}

// Function to handle the output from the shell.
std::string tty::handle_pty_output() {
  pollfd fds{.fd = pty_master_fd, .events = POLLIN, .revents = 0};
  std::string op;

  int poll_res = poll(&fds, 1, 10);
  if (poll_res > 0) { // Added a 10ms timeout
    // std::println(std::cerr, "Poll res: {}, revents: {}", poll_res,
    // fds.revents);
    if (fds.revents & POLLIN) {
      char buffer[4096];
      ssize_t bytes_read = read(pty_master_fd, buffer, sizeof(buffer) - 1);
      // std::println(std::cerr, "Bytes read: {}", bytes_read);
      if (bytes_read > 0) {
        op.assign(buffer, bytes_read);
      } else {
        if (bytes_read == -1 && errno == EIO) {
          // EIO means the slave side (shell) has closed
          return "\x04";
        }
        // Other errors or EOF
        // return "\x04"; // End of Transmission (EOT)
      }
    }
    // Check for hangup or error
    if (fds.revents & (POLLHUP | POLLERR)) {
      return "\x04"; // End of Transmission (EOT)
    }
  }
  return op;
}

// Function to set up the pseudo-terminal
void tty::setup_pty(std::string shell_path) {
  int pty_slave_fd;
  if (openpty(&pty_master_fd, &pty_slave_fd, NULL, NULL, NULL) == -1) {
    std::println(std::cerr, "Error: openpty() failed.");
    ;
    exit(1);
  }

  shell_pid = fork();
  if (shell_pid == -1) {
    std::println(std::cerr, "Error: fork() failed.");
    ;
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

    std::vector<const char *> argv;
    argv.push_back(shell_path.c_str());
    argv.push_back(nullptr);

    execvp(argv[0], (char *const *)argv.data());

    std::println(std::cerr, "Error: execvp() failed.");
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
  std::println("The screen buffer size is {} and line no is {}",
               screen_buffer.size(), line_no);
  for (; line_no < screen_buffer.size(); ++line_no) {
    for (auto &col : screen_buffer[line_no]) {
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

void tty::add_to_screen_buffer(std::string s) {
  for (char c : s) {
    current_line.push_back(c);
  }
  screen_buffer.push_back(current_line);
  current_line.clear();
}

void tty::close_master() { close(pty_master_fd); }

void tty::main_loop() {}
