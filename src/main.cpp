#include <iostream>
#include <fstream>
#include <string>

#include <glad/glad.h>
#include <glfw/glfw3.h>

using namespace std;

////////////////// CONSTANTS //////////////////
const int screen_dimensions[2] = {1200, 900};

////////////////// SIGNATURES //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

bool file_exists(const string& path);
void convert_norm_to_obj(const string& norm_path);

////////////////// MAIN FUNCTION //////////////////
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

    ////////////////// LOAD MODELS //////////////////
    // Check if .obj files are present for the different LODs, create them from the .norm files if not
    for (int i = 0; i < 3; i++) {
        string path = "./models/teapot_surface" + to_string(i) + ".obj";
        if (!file_exists(path)) {
            convert_norm_to_obj(path);
        }
    }
        
    ////////////////// RENDER LOOP //////////////////
    while (!glfwWindowShouldClose(window)) {
        // Check for I/O events
        glfwPollEvents();

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render the current frame
        glfwSwapBuffers(window);
    }

    ////////////////// CLEANUP //////////////////
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    
    // ESC: exit window
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

}

////////////////// HELPER FUNCTIONS //////////////////

bool file_exists(const string& path) {
    ifstream f(path);
    return f.good();
}

void convert_norm_to_obj(const string& obj_path) {
    string norm_path = obj_path.substr(0, obj_path.length() - 4) + ".norm";
    
    ifstream norm_model(norm_path);
    ofstream obj_model(obj_path);

    // Write a comment regarding the number of triangles
    string triangles_str;
    getline(norm_model, triangles_str);
    obj_model << "# Utah teapot, " << triangles_str << " triangles" << endl;
    int triangles_count = stoi(triangles_str);

    // Convert the .norm format to OBJ
    string vertices = "# Vertices\n", normals = "# Normals\n", faces = "# Faces\n", temp;
    int vertex_index = 1;
    int faces_count = 0;
    while (!norm_model.eof() && faces_count < triangles_count) {
        // Read 6 lines as vertex coordinates and normal coordinates intertwined
        for (int i = 0; i < 6; i++) {
            getline(norm_model, temp);
            if (i % 2 == 0) {
                vertices.append("v " + temp + "\n");
            } else {
                normals.append("vn " + temp + "\n");
            }
        }

        // Burn blank line
        getline(norm_model, temp);

        // Add the face description
        faces.append("f");
        for (int i = 0; i < 3; i++) {
            string index = to_string(vertex_index + i);
            faces.append(" " + index + "//" + index);
        }
        faces.append("\n");

        faces_count += 1;
        vertex_index += 3;
    }

    // Write vertices, normals and faces
    obj_model << vertices << endl << normals << endl << faces;

    norm_model.close();
    obj_model.close();
}