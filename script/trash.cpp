
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(400, 400, "Triangle");
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSize(window, 400, 400);

    // Vertex data for a triangle
    GLfloat vertices[] = {
        0.0f, 0.5f,
        -0.5f, -0.5f,
        0.5f, -0.5f
    };

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Clear the color buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Draw the triangle
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.5f);
    glVertex3f(-0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f);
    glEnd();

    glfwTerminate();
    return 0;
}
