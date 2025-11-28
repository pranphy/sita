# SITA Source Implementation (`src`) - Gemini Context

This document details the implementation of the core components of the SITA project. The source files in this directory provide the logic for the classes and functions declared in the `include` directory.

## Core Component Implementations

- **`gui.cpp`**:
  - Implements the `GLFWApp` class.
  - **Responsibilities**:
    - Initializes GLFW and GLEW.
    - Creates and manages the main application window.
    - Sets up OpenGL context hints.
    - Implements callbacks for keyboard input (`on_key_press`) and window resizing (`on_resize`). The key press callback translates GLFW key codes into characters and sends them to the `Terminal` instance.
    - Contains the `mainloop`, which continuously polls for events, polls for shell output via the `Terminal`, and swaps the display buffers.

- **`terminal.cpp`**:
  - Implements the `Terminal` class.
  - **Responsibilities**:
    - Manages the terminal's state, including the `input_buffer` for the current command and the `text_buffer` for the history.
    - Receives character input from `gui.cpp` and forwards it to the `tty` to be processed by the shell.
    - Handles special keys like Backspace and Enter. On Enter, the command is sent to the shell.
    - Regularly `poll_output` from the `tty` to get new output from the shell.
    - Uses the `TerminalParser` to process the raw shell output into structured `ParsedLine` objects.
    - Manages the cursor's position and blinking state.
    - The `show_buffer` method iterates through the parsed lines and the current input buffer, calling the `TextRenderer` to draw each part with the correct styling and position.

- **`text_renderer.cpp`**:
  - Implements the `TextRenderer` class.
  - **Responsibilities**:
    - Initializes the FreeType library to load font files (`.ttf`).
    - Initializes the HarfBuzz library for text shaping. It creates a HarfBuzz font object from a FreeType face.
    - The `shape_text` method takes a string, sends it to HarfBuzz, and gets back a series of `ShapedGlyph` objects with correct positioning for complex scripts.
    - Lazily loads glyphs on demand. When a new glyph is needed, it's rendered using FreeType and uploaded to an OpenGL texture. These are cached for reuse.
    - The `render_text_harfbuzz` method iterates through shaped glyphs and renders each one as a textured quad using OpenGL. It sets up the necessary shaders and projection matrix.

- **`tty.cpp`**:
  - Implements the `tty` struct.
  - **Responsibilities**:
    - Uses `openpty()` to create a new pseudo-terminal.
    - Uses `fork()` to create a child process.
    - The child process uses `execvp()` to replace itself with a shell (e.g., `/bin/bash`), with its standard input, output, and error streams redirected to the slave end of the pty.
    - The parent process (the main application) communicates with the shell by reading from and writing to the master end of the pty file descriptor.
    - Implements a state machine (`intrepret_bytes`) to parse incoming byte streams from the shell, handling plain characters and ANSI escape sequences.

- **`terminal_parser.cpp`**:
  - Implements the `TerminalParser` class.
  - **Responsibilities**:
    - Uses a set of regular expressions (`prompt_patterns`) to identify shell prompts.
    - Provides functions to determine if a line is a prompt, command output, or an error message based on heuristics.
    - Implements logic to parse ANSI escape sequences to extract color codes and other text attributes, updating the current terminal state accordingly.
    - The main `parse_output` function splits raw text into lines and converts each line into a `ParsedLine` struct with associated content and attributes.

- **`shader.cpp`**, **`oglutil.cpp`**, **`utils.cpp`**:
  - These files implement the utility classes and functions for shader management, OpenGL helpers, and string manipulation, respectively, as declared in their corresponding header files.
