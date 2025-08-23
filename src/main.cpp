#include <iostream>

#include <glad/glad.h>
#include <glfw/glfw3.h>

using namespace std;

////////////////// CONSTANTS //////////////////
const int screen_dimensions[2] = {1200, 900};

////////////////// SIGNATURES DECLARATION //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

int main() {

    ////////////////// WINDOW INITIALIZATION //////////////////
    // Init GLFW context to OpenGL Core 4.1
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(screen_dimensions[0], screen_dimensions[1], "Tessellation", nullptr, nullptr);
    if (window == NULL) {
        cout << "Failed to create GLFW window. Terminating..." << endl;
        glfwTerminate();
        return -1; 
    }
    glfwMakeContextCurrent(window);

    // Link callbacks
    glfwSetKeyCallback(window, key_callback);

    // Disable mouse cursor
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load the GLFW context in GLAD
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD. Terminating..." << endl;
        return -1;
    }

    // Set viewport size (poll the framebuffer dimensions for display compatibility)
    int screen_width, screen_height;
    glfwGetFramebufferSize(window, &screen_width, &screen_height);
    glViewport(0, 0, screen_width, screen_height);

    ////////////////// RENDER LOOP //////////////////
    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ////////////////// CLEANUP //////////////////
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    // TODO: populate
    return;
}