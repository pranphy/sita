# SITA Public API & Data Structures (`include`) - Gemini Context

This document provides an overview of the public interfaces, classes, and data structures defined in the header files under the `include/` directory. It serves as a reference for the project's architecture and API.

## Header File Breakdown

- **`gui.h`**:
  - Defines the `GLFWApp` class, which encapsulates the main application window and the terminal instance. It's responsible for initializing GLFW, creating the window, handling user input events, and managing the main application loop.

- **`terminal.h`**:
  - Defines the `Terminal` class, the central component that manages the state of the terminal. It handles the input buffer, cursor, and the text buffer containing the command history and output. It interfaces with the `TextRenderer` for drawing and the `tty` for shell communication.

- **`text_renderer.h`**:
  - Defines the `TextRenderer` class, which is responsible for all aspects of text rendering.
  - **Key Structures:**
    - `Coord`: A simple struct to hold `x` and `y` floating-point coordinates.
    - `Character`: Holds information about a single rendered glyph, including its texture ID, dimensions, bearing, and advance.
    - `ShapedGlyph`: Represents a glyph after being processed by HarfBuzz, including its ID, offsets, advance, and a pointer to its `Character` data.

- **`tty.h`**:
  - Defines the `tty` struct, which manages the pseudo-terminal (pty). It's responsible for forking a shell process and handling the low-level communication (reading and writing) between the terminal application and the shell.

- **`terminal_parser.h`**:
  - Defines the `TerminalParser` class, which is responsible for parsing the output from the shell.
  - **Key Enums and Structs:**
    - `LineType`: An enum to classify lines of text (e.g., `PROMPT`, `COMMAND_OUTPUT`).
    - `AnsiColor`: An enum for standard ANSI color codes.
    - `TerminalAttributes`: A struct to hold text attributes like foreground/background colors, bold, italic, etc.
    - `ParsedLine`: A struct that represents a line of text after parsing, containing its content, type, and attributes.

- **`shader.h`**:
  - Defines the `Shader` class, a simple interface for loading, compiling, and using OpenGL shaders from vertex and fragment shader files.

- **`oglutil.h`**:
  - Declares a namespace `oglutil` containing helper functions for common OpenGL tasks, such as loading glyphs into textures and rendering textured rectangles.

- **`utils.h`**:
  - Declares a namespace `utl` with various utility functions, primarily for string manipulation like splitting strings by newlines or by script type (Devanagari).
