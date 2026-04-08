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
#include <imgui/implot.h>
#include <imgui/implot_internal.h>

#include <utils/model.h>
#include <utils/shader.h>

// Disable Imgui demo windows to save compile time
#define IMGUI_DISABLE_DEMO_WINDOWS

using namespace std;
using namespace glm;

////////////////// CONSTANTS //////////////////
const int screen_dimensions[2] = {1200, 900};

const float movement_speed = 5.0f;
const float rotation_speed = 30.0f;

// diffusive, specular and ambient components
GLfloat diffuseColor[] = {0.298f, 0.447f, 0.69f};
GLfloat specularColor[] = {1.0f, 1.0f, 1.0f};
GLfloat ambientColor[] = {0.1f, 0.1f, 0.1f};

// weights for the diffusive, specular and ambient components
GLfloat Kd = 0.5f;
GLfloat Ks = 0.3f;
GLfloat Ka = 0.2f;

// shininess coefficient
GLfloat shininess = 25.0f;

////////////////// FLAGS //////////////////
bool wireframe = false;
bool movement = true;
bool rotation = true;

////////////////// SIGNATURES //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

void prepare_gui_frame(const vector<float> &frame_times, const vector<int> &lod_levels, int max_len);
int k_formatter(double value, char* buff, int size, void* data);

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

	// Enable the Z check
	glEnable(GL_DEPTH_TEST);

    // Set clear color
    glClearColor(0.2, 0.2, 0.2, 1.0);

    ////////////////// GUI INITIALIZATION //////////////////
    // Setup ImGui context and options
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImPlot::CreateContext();

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
    Shader static_LOD("shaders/static.vert", "shaders/static.frag");
    static_LOD.Use();

    ////////////////// TRANSFORMS //////////////////
    // Projection and view matrices
    mat4 projection = perspective(45.0f, (float) screen_width / (float) screen_height, 0.1f, 10000.0f);
    mat4 view = lookAt(vec3(0.0f, 5.0f, 7.0f), vec3(0.0f, 0.0f, -7.0f), vec3(0.0f, 1.0f, 0.0f));

    // Model matrices
    mat4 static_model_matrix = mat4(1.0f);
	mat3 static_normal_matrix = mat3(1.0f);

    ////////////////// RENDERING LOOP //////////////////
    float delta_time, current_frame, last_frame = 0;

    int frame_count = 0;
	int frame_limit = 25;
    float time_accumulator = 0.0f;

	int avg_times_max = 200;
	vector<float> avg_times;
	vector<int> lod_levels;

    vec3 position = vec3(0.0f, -1.0f, -9.9f);
    vec3 direction = vec3(0.0f, 0.0f, -1.0f);
	float rotation_angle = 0.0f;

	vec3 light_position = position;
	vec3 light_offset = vec3(0.0f, 5.0f, 2.0f);

    while (!glfwWindowShouldClose(window)) {
        
        // Compute delta time
        current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        // Check for I/O events
        glfwPollEvents();

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update position and rotation if movement is true
		if (movement) {
			position += (movement_speed * delta_time) * direction;

			if (position.z >= -1.0f) {
				direction = {0.0f, 0.0f, -1.0f};
			} else if (position.z <= -37.0f) {
				direction = {0.0f, 0.0f, 1.0f};
			}

			light_position = position + light_offset;
		}
		if (rotation) {
			rotation_angle += (rotation_speed * delta_time);
			while (rotation_angle >= 360.0f) {
				rotation_angle -= 360.0f;
			}
		}

		// Select LOD level
		int lod_level = 0;
		if (position.z > -10.0f) {
            lod_level = 2;
        } else if (position.z > -28.0f) {
            lod_level = 1;
		}

		// Save time for render_time computation
		glFinish();
		float start_time_static = glfwGetTime();

		// Send matrices and uniforms
        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));
		glUniform3fv(glGetUniformLocation(static_LOD.Program, "diffuse_color"), 1, diffuseColor);
        glUniform3fv(glGetUniformLocation(static_LOD.Program, "ambient_color"), 1, ambientColor);
        glUniform3fv(glGetUniformLocation(static_LOD.Program, "specular_color"), 1, specularColor);
		glUniform3fv(glGetUniformLocation(static_LOD.Program, "light_position"), 1, value_ptr(light_position));
		glUniform1f(glGetUniformLocation(static_LOD.Program, "k_d"), Kd);
		glUniform1f(glGetUniformLocation(static_LOD.Program, "k_s"), Ks);
        glUniform1f(glGetUniformLocation(static_LOD.Program, "k_a"), Ka);
        glUniform1f(glGetUniformLocation(static_LOD.Program, "shininess"), shininess);

        static_model_matrix = mat4(1.0f);
		static_normal_matrix = mat3(1.0f);
        static_model_matrix = translate(static_model_matrix, position);
		static_model_matrix = rotate(static_model_matrix, radians(rotation_angle), vec3(0, 1, 0));
		static_normal_matrix = inverseTranspose(mat3(view * static_model_matrix));
        glUniformMatrix4fv(glGetUniformLocation(static_LOD.Program, "model_matrix"), 1, GL_FALSE, value_ptr(static_model_matrix));
		glUniformMatrix3fv(glGetUniformLocation(static_LOD.Program, "normal_matrix"), 1, GL_FALSE, value_ptr(static_normal_matrix));

        if (lod_level == 2) {
            teapot_lod2.Draw();
        } else if (lod_level == 1) {
            teapot_lod1.Draw();
        } else {
            teapot_lod0.Draw();
        }

		// Update avg_frame_time_ms
		glFinish();
		float render_time = (glfwGetTime() - start_time_static) * 1000;
		
		frame_count++;
		time_accumulator += render_time;
        if (frame_count >= frame_limit) {
            float avg_frame_time_ms = (time_accumulator / frame_limit);
            time_accumulator = 0;
            frame_count = 0;

			avg_times.push_back(avg_frame_time_ms);
			lod_levels.push_back(lod_level);
			if (int(avg_times.size()) > avg_times_max) {
				avg_times.erase(avg_times.begin());
				lod_levels.erase(lod_levels.begin());
			}
        }

		// Setup GUI frame
        prepare_gui_frame(avg_times, lod_levels, avg_times_max);

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
	ImPlot::DestroyContext();

    glfwTerminate();

    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ESC: exit window
        glfwSetWindowShouldClose(window, true);
    } else if (key == GLFW_KEY_W && action == GLFW_PRESS) {
        // W: wireframe on/off
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    } else if (key == GLFW_KEY_M && action == GLFW_PRESS) {
		// M: movement on/off
		movement = !movement;
	} else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		// R: rotation on/off
		rotation = !rotation;
	}
}

////////////////// GUI FUNCTIONS //////////////////

void prepare_gui_frame(const vector<float> &avg_times, const vector<int> &lod_levels, int max_len) {
    // Setup new GUI frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Define options for the GUI window
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;

	// Find LOD changes
	vector<int> lod_changes;
	lod_changes.push_back(0);

	for (int i = 1; i < int(lod_levels.size()) - 1; i++) {
		if (lod_levels[i] != lod_levels[i+1]) {
			lod_changes.push_back(i+1);
		}
	}

	// Compute triangle counts
	vector<int> triangle_counts;
	for (int i = 0; i < int(lod_levels.size()); i++) {
		int trigs = (lod_levels[i] == 0)? 5144 : ((lod_levels[i] == 1))? 22885 : 158865;
		triangle_counts.push_back(trigs);
	}

    // Init window
    ImGui::Begin("Performance Analysis", NULL, window_flags);

	if (ImPlot::BeginPlot("Frame Times")) {
		// ImPlot::SetupAxes("##", "##", 0, ImPlotAxisFlags_AutoFit);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, max_len-1, ImPlotCond_Always);
		ImPlot::SetupAxisFormat(ImAxis_X1, "");
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 1.0f);
		ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, INFINITY);
		ImPlot::SetupAxisFormat(ImAxis_Y1, "%.2f");

		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotLineFlags_Shaded;

		if (int(avg_times.size()) > 0) {
			ImPlot::PlotLine("Static LOD", &avg_times[0], int(avg_times.size()), 1.0, 0.0, specs);
		}

		if (int(lod_levels.size()) > 0) {
			specs.LineColor = ImPlot::GetLastItemColor();
			ImPlot::PlotInfLines("##LOD Changes", &lod_changes[0], int(lod_changes.size()), specs);

			for (int i = 0; i < int(lod_changes.size()); i++) {
				string text = "LOD " + std::to_string(lod_levels[lod_changes[i]]);
				ImPlot::PlotText(text.c_str(), lod_changes[i], 0.0f, ImVec2(25, -15));
			}
		}

		ImPlot::EndPlot();
	}

	if (ImPlot::BeginPlot("Triangle count")) {
		ImPlot::SetupAxesLimits(0,max_len-1,0,190000, ImPlotCond_Always);
		ImPlot::SetupAxisFormat(ImAxis_X1, "");
		ImPlot::SetupAxisFormat(ImAxis_Y1, k_formatter);

		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotStairsFlags_Shaded;

		ImPlot::PlotStairs("Static LOD", &triangle_counts[0], int(triangle_counts.size()), 1.0, 0.0, specs);

		ImPlot::EndPlot();
	}

    ImGui::End();

    return;
}

int k_formatter(double value, char* buff, int size, void* data) {
    if (fabs(value) >= 1000) {  
        return snprintf(buff, size, "%.0fk", value / 1000.0);  
    }  
    return snprintf(buff, size, "%.0f", value); 
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

    string options = "";

    // Write vertices, normals and faces
    obj_model << vertices << endl << normals << endl << options << endl << faces;

    norm_model.close();
    obj_model.close();
}