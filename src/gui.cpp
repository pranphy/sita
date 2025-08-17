#include <print>
#include <iostream>
#include <GL/glew.h>
#include "text_renderer.h"

#include "gui.h"

GLFWApp::GLFWApp(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

int GLFWApp::create(int width, int height, const char* title){

    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        std::println(std::cerr,"ERROR::GLFW: Failed to create GLFW window");;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::println(std::cerr,"ERROR::GLEW: Failed to initialize GLEW");
        return -1;
    }
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    return 0;
}

void GLFWApp::mainloop(){
    // Use IosevkaTerm as primary font and Laila-Regular as fallback for Devanagari
    TextRenderer text_renderer(
        "/home/pranphy/.local/share/fonts/iosevka/IosevkaTermSlabNerdFont-Regular.ttf",
        "/home/pranphy/.local/share/fonts/devanagari/Laila-Regular.ttf"
    );
    
    while (!glfwWindowShouldClose(window)) {
        // Get window dimensions
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT);

        float color[] = {1.0f, 1.0f, 1.0f, 1.0f}; // White color
        
        // Use HarfBuzz for proper text shaping with font fallback
        text_renderer.render_text_harfbuzz("Hello World! --> != <= := >=", 25.0f, height - 50.0f, 1.0f, color, width, height);
        
        // Test mixed text - Latin will use IosevkaTerm, Devanagari will use Laila
        text_renderer.render_text_harfbuzz("माया श्रेष्ठ मृत्यु fi fil --> -> !=", 25.0f, height - 100.0f, 1.0f, color, width, height);
        
        // Test different text with proper spacing
        text_renderer.render_text_harfbuzz("Text with proper kerning", 25.0f, height - 150.0f, 1.0f, color, width, height);
        
        // Test numbers and symbols
        text_renderer.render_text_harfbuzz("Numbers: 1234567890", 25.0f, height - 200.0f, 1.0f, color, width, height);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void GLFWApp::cleanup(){
    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
}
