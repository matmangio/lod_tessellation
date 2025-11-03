#include <iostream>
#include <fstream>
#include <string>

#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <utils/model.h>
#include <utils/shader.h>

using namespace std;
using namespace glm;

////////////////// CONSTANTS //////////////////
const int screen_dimensions[2] = {1200, 900};

const float teapot_speed = 5.0f;

////////////////// FLAGS //////////////////
bool wireframe = false;

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

    // Set clear color
    glClearColor(0.05, 0.05, 0.2, 1.0);

    ////////////////// GUI INITIALIZATION //////////////////
    // Setup ImGui context and options
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Connect to GLFW window
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    ////////////////// MODELS //////////////////
    // Check if .obj files are present for the different LODs, create them from the .norm files if not
    for (int i = 0; i < 3; i++) {
        string path = "./models/teapot_surface" + to_string(i) + ".obj";
        if (!file_exists(path)) {
            convert_norm_to_obj(path);
        }
    }

    // Load the .obj models
    Model teapot_lod0("./models/teapot_surface0.obj", true);
    Model teapot_lod1("./models/teapot_surface1.obj", true);
    Model teapot_lod2("./models/teapot_surface2.obj", true);
    
    ////////////////// SHADERS //////////////////
    // Load the shader programs
    Shader static_LOD("src/static.vert", "src/static.frag");
    static_LOD.Use();

    ////////////////// TRANSFORMS //////////////////
    // Projection and view matrices
    mat4 projection = perspective(45.0f, (float) screen_width / (float) screen_height, 0.1f, 10000.0f);
    mat4 view = lookAt(vec3(0.0f, 0.0f, 7.0f), vec3(0.0f, 0.0f, -7.0f), vec3(0.0f, 1.0f, 0.0f));

    // Model matrices
    mat4 static_model_matrix = mat4(1.0f);

    ////////////////// RENDERING LOOP //////////////////
    float delta_time, current_frame, last_frame = 0;
    vec3 position = vec3(0.0f, -1.0f, -1.0f);
    vec3 direction = vec3(0.0f, 0.0f, -1.0f);

    while (!glfwWindowShouldClose(window)) {
        
        // Compute delta time
        current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;
        
        // Check for I/O events
        glfwPollEvents();

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start GUI frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update position
        position += (teapot_speed * delta_time) * direction;
        if (position.z <= -40.0f || position.z >= -1.0f) {
            direction = -direction;
        }

        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));

        static_model_matrix = mat4(1.0f);
        static_model_matrix = translate(static_model_matrix, position);
        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "model_matrix"), 1, GL_FALSE, value_ptr(static_model_matrix));

        if (position.z > -5.0f) {
            teapot_lod2.Draw();
        } else if (position.z > -25.0f) {
            teapot_lod1.Draw();
        } else {
            teapot_lod0.Draw();
        }

        // Render GUI on top
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render the current frame
        glfwSwapBuffers(window);
    }

    ////////////////// CLEANUP //////////////////
    static_LOD.Delete();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ESC: exit window
        glfwSetWindowShouldClose(window, true);
    } else if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        // L: wireframe on/off
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
}

////////////////// HELPER FUNCTIONS //////////////////

bool file_exists(const string& path) {
    ifstream f(path);
    return f.good();
}

void convert_norm_to_obj(const string& obj_path) {
    
    // Open files
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
        // Read 6 lines as 3 vertex coordinates and 3 normal coordinates intertwined
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

    string options = "s 1";

    // Write vertices, normals and faces
    obj_model << vertices << endl << normals << endl << options << endl << faces;

    norm_model.close();
    obj_model.close();
}