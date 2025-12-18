# SITA

SImple Terminal Application

## Description

This is a terminal emulator written in C++. This is a GPU-accelerated terminal application that hopes to render complex scripts like Devanagari nicely.

Demo
![](screenshot.png)

## Features

- Supports Indic fonts like Devanagari and has nice support for Indic languages.

## Installation

To build this project from source, follow these steps:

1. Install build dependencies:

   ```bash
   sudo apt update &&
   sudo apt install \
   libglfw3-dev \
   libharfbuzz-dev \
   libfreetype-dev \
   libglew-dev \
   libglm-dev \
   libgl1-mesa-dev \
   libglu1-mesa-dev \
   wayland-protocols\
   pkg-config
   ```

2. Install Meson and Ninja:

   ```bash
   sudo apt install meson ninja-build
   ```

3. Build the project:

   ```bash
   meson setup build
   cd build
   ninja
   ```

> [! WARNING ]
> This is very volatile and rapidly develooping application. Use with caution.
