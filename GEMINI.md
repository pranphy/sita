# SITA (SImple Terminal Application) - Gemini Context

This document provides a comprehensive overview of the SITA project, a GPU-accelerated terminal emulator written in C++. It's intended to be a quick reference for understanding the project's architecture, key components, and build process.

## Project Overview

SITA is a modern terminal emulator with a focus on providing excellent rendering support for complex scripts, particularly Devanagari. It leverages GPU acceleration for smooth and efficient rendering.

- **Language:** C++23
- **Graphics:** OpenGL (via GLEW)
- **Windowing:** GLFW
- **Text Rendering:** FreeType and HarfBuzz
- **Build System:** Meson and Ninja

## Key Components

The project is structured into several key components, each with a specific responsibility:

- **`main.cpp`:** The application's entry point. It initializes the `GLFWApp` and starts the main loop.

- **`gui.h`/`gui.cpp`:** Manages the GLFW window, including its creation, event handling (keyboard input, resizing), and the main application loop. It owns the `Terminal` instance.

- **`terminal.h`/`terminal.cpp`:** The core of the terminal logic. It manages the terminal's state, including the input buffer, cursor position, and the history of commands and their output. It orchestrates the rendering process by calling the `TextRenderer` and communicates with the underlying shell via the `tty` component.

- **`text_renderer.h`/`text_renderer.cpp`:** Responsible for all text rendering. It uses FreeType to load font files and HarfBuzz for text shaping, which is essential for the correct rendering of complex scripts. The shaped text is then rendered to the screen using OpenGL.

- **`tty.h`/`tty.cpp`:** Manages the pseudo-terminal (pty). It forks a child process to run a shell (e.g., `/bin/bash`) and establishes a communication channel with it. This allows the terminal to send user input to the shell and receive its output.

- **`terminal_parser.h`/`terminal_parser.cpp`:** Parses the output received from the shell. It can identify different types of output (prompts, command output, errors) and interpret ANSI escape codes for text styling (colors) and cursor positioning.

- **`shader.h`/`shader.cpp`:** A utility class for loading and managing OpenGL shaders.

- **`oglutil.h`/`oglutil.cpp`:** Contains helper functions for OpenGL-related tasks, such as creating textures for glyphs and rendering them.

- **`utils.h`/`utils.cpp`:** Provides various utility functions, including string manipulation functions for splitting text based on newlines or script type (e.g., Devanagari).

- **`meson.build`:** The build script for the Meson build system. It defines the project's source files, dependencies, and build configuration.

## Build Instructions

To build the project, you need to have Meson, Ninja, and the required dependencies installed.

1.  **Install Dependencies:**
    - `harfbuzz`
    - `freetype2`
    - `glfw3`
    - `glew`
    - `glm`
    - `opengl`
    - `glu`

2.  **Build the project:**
    ```bash
    meson setup build
    cd build
    ninja
    ```

## How it Works

1.  The `main` function creates a `GLFWApp` instance.
2.  The `GLFWApp` creates a window and a `Terminal` instance.
3.  The `Terminal` instance creates a `tty` instance, which spawns a shell in a pseudo-terminal.
4.  The `GLFWApp` enters its main loop, waiting for events.
5.  When the user types, the `GLFWApp` captures the key presses and sends them to the `Terminal`.
6.  The `Terminal` forwards the input to the `tty`, which writes it to the shell's pty.
7.  The shell processes the input and writes its output to the pty.
8.  The `Terminal` reads the shell's output from the `tty`, parses it using the `TerminalParser`, and then uses the `TextRenderer` to display it in the window.
9.  The `TextRenderer` uses FreeType and HarfBuzz to shape the text and OpenGL to render it.
