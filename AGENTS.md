# Agent Guidelines for sita Repository

This document outlines the conventions and commands for agents operating within this repository.

## Build Commands
*   **Configure Build:** `meson setup build`
*   **Compile Project:** `meson compile -C build`
*   **Run Tests:** `meson test -C build` (No specific tests are currently defined.)
*   **Custom Compile Script:** `./compile` (Compiles the project)
    *   **Run after compile:** `./compile -r`
    *   **Force compile:** `./compile -f`

## Code Style Guidelines

### Includes
*   System/library includes precede project-specific includes.
*   A blank line separates system/library includes from project includes.

### Formatting
*   **Indentation:** 2 spaces.
*   **Braces:** K&R style for functions and classes; new line for control structures.
*   **Spacing:** Spaces around operators, after commas, and after keywords.

### Naming Conventions
*   **Classes:** PascalCase (e.g., `GLFWApp`).
*   **Functions/Methods:** snake_case (e.g., `create`, `on_key_press`).
*   **Variables:** snake_case (e.g., `window`, `width`).
*   **Macros:** ALL_CAPS (e.g., `GUI_H`).

### Error Handling
*   Error messages are printed to `std::cerr` using `std::println`.
*   Functions return `-1` on failure.
*   `glfwTerminate()` is called for GLFW-related errors.

## Linting and Testing
No formal linting tools or specific test commands are currently configured in this project.

## AI-Specific Rules
No `.cursor/rules/` or `.github/copilot-instructions.md` files were found.
