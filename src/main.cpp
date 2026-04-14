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
#include <utils/bezier.h>

// Disable Imgui demo windows to save compile time
#define IMGUI_DISABLE_DEMO_WINDOWS

using namespace std;
using namespace glm;

////////////////// CONSTANTS //////////////////
const int screen_dimensions[2] = {1366, 768};
const int window_position[2] = {100, 100};

const float movement_speed = 4.0f;
const float rotation_speed = 30.0f;

// diffusive, specular and ambient components
const GLfloat diffuseColor_static[] = {0.298f, 0.447f, 0.69f};
const GLfloat diffuseColor_dynamic[] = {0.866f, 0.517f, 0.321f};
const GLfloat specularColor[] = {1.0f, 1.0f, 1.0f};
const GLfloat ambientColor[] = {0.1f, 0.1f, 0.1f};

// weights for the diffusive, specular and ambient components
const GLfloat Kd = 0.5f;
const GLfloat Ks = 0.3f;
const GLfloat Ka = 0.2f;

// shininess coefficient
const GLfloat shininess = 25.0f;

// tessellation coefficients (min - max)
const GLfloat tess_levels_outer[2] = {10.0f, 64.0f};
const GLfloat tess_levels_inner[2] = {5.0f, 32.0f};

// lod distances
const GLfloat min_max_distance[2] = {-5.0f, -41.0f};
const GLfloat lod_switch_distances[2] = {-14.0f, -32.0f};

////////////////// FLAGS //////////////////
bool wireframe = false;
bool movement = true;
bool rotation = true;

////////////////// SIGNATURES //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

void prepare_gui_frame(const vector<float> &static_avg_times, const vector<float> &dynamic_avg_times, const vector<int> &lod_levels, const vector<int> &bezier_trigs, int max_len);
int k_formatter(double value, char* buff, int size, void* data);

bool file_exists(const string& path);
int round_to_odd(float t);
void convert_norm_to_obj(const string& norm_path);

////////////////// MAIN FUNCTION //////////////////
int main() {

    ////////////////// WINDOW INITIALIZATION //////////////////
    // Init GLFW context to OpenGL Core 4.1
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Create GLFW window
    GLFWwindow* window = glfwCreateWindow(screen_dimensions[0], screen_dimensions[1], "Static vs Dynamic LOD", nullptr, nullptr);
    if (window == NULL) {
        cout << "Failed to create GLFW window. Terminating..." << endl;
        glfwTerminate();
        return -1; 
    }
    glfwMakeContextCurrent(window);

	// Move window to specified coordinates
	glfwSetWindowPos(window, window_position[0], window_position[1]);

    // Link callbacks
    glfwSetKeyCallback(window, key_callback);

    // Load the GLFW context in GLAD
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD. Terminating..." << endl;
        return -1;
    }

    // Set viewport size (poll the framebuffer dimensions for display compatibility)
    int screen_width, screen_height;
    glfwGetFramebufferSize(window, &screen_width, &screen_height);
    glViewport(0, 0, screen_width, screen_height);

	////////////////// OPENGL INITIALIZATION //////////////////

	// Enable the Z check
	glEnable(GL_DEPTH_TEST);

    // Set clear color
    glClearColor(0.4, 0.4, 0.4, 1.0);

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
    
	// Load the Bezier representation of the teapot
	Bezier teapot_bezier("./models/teapot_bezier.bpt");

    ////////////////// SHADERS //////////////////
    // Load the shader programs
    Shader static_shader("shaders/static.vert", "shaders/illumination.frag");
	Shader dynamic_shader("shaders/dynamic.vert", "shaders/illumination.frag", "shaders/dynamic.tcs", "shaders/dynamic.tes");

    ////////////////// TRANSFORMS //////////////////
    // Projection and view matrices
	vec3 eye_world_position = vec3(0.0f, 5.0f, 7.0f);
    mat4 projection = perspective(45.0f, (float) screen_width / (float) screen_height, 0.1f, 10000.0f);
    mat4 view = lookAt(eye_world_position, vec3(0.0f, 0.0f, -7.0f), vec3(0.0f, 1.0f, 0.0f));

    // Model matrices
    mat4 static_model_matrix = mat4(1.0f);
	mat4 dynamic_model_matrix = mat4(1.0f);
	mat3 static_normal_matrix = mat3(1.0f);

    ////////////////// RENDERING LOOP //////////////////
    float delta_time, current_frame, last_frame = 0;

    int frame_count = 0;
	int frame_limit = 30;
    float static_time_accumulator = 0.0f;
	float dynamic_time_accumulator = 0.0f;

	float static_avg_render_time;
	float dynamic_avg_render_time;
	int avg_times_max = 2500;
	vector<float> static_avg_times;
	vector<float> dynamic_avg_times;
	vector<int> lod_levels;
	vector<int> bezier_trigs;

    vec3 position = vec3(-3.5f, -1.0f, min_max_distance[0]);
    vec3 direction = vec3(0.0f, 0.0f, -1.0f);
	float rotation_angle = 0.0f;

	vec3 light_position = vec3(0.0f, 4.0f, min_max_distance[0]);
	float light_offset_z = 4.0f;

	GLfloat min_max_distance_from_eye[2] = {
		distance(position, eye_world_position), 
		distance(position + vec3(0.0f, 0.0f, min_max_distance[1] - min_max_distance[0]), eye_world_position)
	};

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

			if (position.z >= min_max_distance[0]) {
				direction = {0.0f, 0.0f, -1.0f};
			} else if (position.z <= min_max_distance[1]) {
				direction = {0.0f, 0.0f, 1.0f};
			}

			light_position.z = position.z + light_offset_z;
		}
		if (rotation) {
			rotation_angle += (rotation_speed * delta_time);
			while (rotation_angle >= 360.0f) {
				rotation_angle -= 360.0f;
			}
		}

		// Select LOD level
		int lod_level = 0;
		if (position.z > lod_switch_distances[0]) {
            lod_level = 2;
        } else if (position.z > lod_switch_distances[1]) {
            lod_level = 1;
		}

		// Select the Static Shader
		static_shader.Use();

		// Start static time computation
		glFinish();
		float static_start_time = glfwGetTime();

		// Send matrices and uniforms
        glUniformMatrix4fv(glGetUniformLocation(static_shader.Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(static_shader.Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));
		glUniform3fv(glGetUniformLocation(static_shader.Program, "diffuse_color"), 1, diffuseColor_static);
        glUniform3fv(glGetUniformLocation(static_shader.Program, "ambient_color"), 1, ambientColor);
        glUniform3fv(glGetUniformLocation(static_shader.Program, "specular_color"), 1, specularColor);
		glUniform3fv(glGetUniformLocation(static_shader.Program, "light_position"), 1, value_ptr(light_position));
		glUniform1f(glGetUniformLocation(static_shader.Program, "k_d"), Kd);
		glUniform1f(glGetUniformLocation(static_shader.Program, "k_s"), Ks);
        glUniform1f(glGetUniformLocation(static_shader.Program, "k_a"), Ka);
        glUniform1f(glGetUniformLocation(static_shader.Program, "shininess"), shininess);

        static_model_matrix = mat4(1.0f);
		static_normal_matrix = mat3(1.0f);
        static_model_matrix = translate(static_model_matrix, position);
		static_model_matrix = rotate(static_model_matrix, radians(rotation_angle), vec3(0, 1, 0));
		static_normal_matrix = inverseTranspose(mat3(view * static_model_matrix));
        glUniformMatrix4fv(glGetUniformLocation(static_shader.Program, "model_matrix"), 1, GL_FALSE, value_ptr(static_model_matrix));
		glUniformMatrix3fv(glGetUniformLocation(static_shader.Program, "normal_matrix"), 1, GL_FALSE, value_ptr(static_normal_matrix));

        if (lod_level == 2) {
            teapot_lod2.Draw();
        } else if (lod_level == 1) {
            teapot_lod1.Draw();
        } else {
            teapot_lod0.Draw();
        }

		// Stop static time computation
		glFinish();
		float static_render_time = (glfwGetTime() - static_start_time) * 1000;
		
		// Start dynamic time computation
		float dynamic_start_time = glfwGetTime();

		// Render Bezier
		dynamic_shader.Use();
		vec3 bezier_position = position + vec3(7.0f, 0.0f, 0.0f);

		// Compute tessellation levels
		float distance_from_eye = distance(position, eye_world_position);
		float t = (distance_from_eye - min_max_distance_from_eye[0]) / (min_max_distance_from_eye[1] - min_max_distance_from_eye[0]);
		float current_tess_level_outer = tess_levels_outer[0] * t + tess_levels_outer[1] * (1 - t);
		float current_tess_level_inner = tess_levels_inner[0] * t + tess_levels_inner[1] * (1 - t);

		glUniformMatrix4fv(glGetUniformLocation(dynamic_shader.Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(dynamic_shader.Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));
		glUniform3fv(glGetUniformLocation(dynamic_shader.Program, "diffuse_color"), 1, diffuseColor_dynamic);
        glUniform3fv(glGetUniformLocation(dynamic_shader.Program, "ambient_color"), 1, ambientColor);
        glUniform3fv(glGetUniformLocation(dynamic_shader.Program, "specular_color"), 1, specularColor);
		glUniform3fv(glGetUniformLocation(dynamic_shader.Program, "light_position"), 1, value_ptr(light_position));
		glUniform1f(glGetUniformLocation(dynamic_shader.Program, "k_d"), Kd);
		glUniform1f(glGetUniformLocation(dynamic_shader.Program, "k_s"), Ks);
        glUniform1f(glGetUniformLocation(dynamic_shader.Program, "k_a"), Ka);
        glUniform1f(glGetUniformLocation(dynamic_shader.Program, "shininess"), shininess);

		glUniform1f(glGetUniformLocation(dynamic_shader.Program, "tess_level_outer"), current_tess_level_outer);
		glUniform1f(glGetUniformLocation(dynamic_shader.Program, "tess_level_inner"), current_tess_level_inner);

		dynamic_model_matrix = mat4(1.0f);
        dynamic_model_matrix = translate(dynamic_model_matrix, bezier_position);
		dynamic_model_matrix = rotate(dynamic_model_matrix, radians(180 + rotation_angle), vec3(0, -1, 0));
        glUniformMatrix4fv(glGetUniformLocation(dynamic_shader.Program, "model_matrix"), 1, GL_FALSE, value_ptr(dynamic_model_matrix));

		teapot_bezier.Draw();

		// Stop dynamic time computation
		glFinish();
		float dynamic_render_time = (glfwGetTime() - dynamic_start_time) * 1000;

		// Update UI
		frame_count++;
		static_time_accumulator += static_render_time;
		dynamic_time_accumulator += dynamic_render_time;
        if (frame_count >= frame_limit) {
            static_avg_render_time = (static_time_accumulator / frame_limit);
			dynamic_avg_render_time = (dynamic_time_accumulator / frame_limit);

            static_time_accumulator = 0;
			dynamic_time_accumulator = 0;
            frame_count = 0;
        }

		int odd_tess_level_outer = round_to_odd(current_tess_level_outer);
		int odd_tess_level_inner = round_to_odd(current_tess_level_inner);
		int trigs_per_patch = 4 * (odd_tess_level_outer + odd_tess_level_inner - 2) + 2 * pow(odd_tess_level_inner - 2, 2);
		int trigs = teapot_bezier.patches * trigs_per_patch;

		static_avg_times.push_back(static_avg_render_time);
		dynamic_avg_times.push_back(dynamic_avg_render_time);
		lod_levels.push_back(lod_level);
		bezier_trigs.push_back(trigs);
		if (int(static_avg_times.size()) > avg_times_max) {
			static_avg_times.erase(static_avg_times.begin());
			dynamic_avg_times.erase(dynamic_avg_times.begin());
			lod_levels.erase(lod_levels.begin());
			bezier_trigs.erase(bezier_trigs.begin());
		}

		// Setup GUI frame
        prepare_gui_frame(static_avg_times, dynamic_avg_times, lod_levels, bezier_trigs, avg_times_max);

        // Render GUI on top
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render the current frame
        glfwSwapBuffers(window);
    }

    ////////////////// CLEANUP //////////////////
    static_shader.Delete();
    
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

void prepare_gui_frame(const vector<float> &static_avg_times, const vector<float> &dynamic_avg_times, const vector<int> &lod_levels, const vector<int> &bezier_trigs, int max_len) {
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
	// lod_changes.push_back(0);

	for (int i = 1; i < int(lod_levels.size()) - 1; i++) {
		if (lod_levels[i] != lod_levels[i+1]) {
			lod_changes.push_back(i+1);
		}
	}

	// Compute triangle counts
	vector<int> static_triangle_counts;
	for (int i = 0; i < int(lod_levels.size()); i++) {
		int trigs = (lod_levels[i] == 0)? 5144 : ((lod_levels[i] == 1))? 22885 : 158865;
		static_triangle_counts.push_back(trigs);
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

		// Setup legend
		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImVec4 static_color;
		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotLineFlags_Shaded;

		if (int(static_avg_times.size()) > 0) {
			ImPlot::PlotLine("Static", &static_avg_times[0], int(static_avg_times.size()), 1.0, 0.0, specs);
			static_color = ImPlot::GetLastItemColor();
			ImPlot::PlotLine("Dynamic", &dynamic_avg_times[0], int(dynamic_avg_times.size()), 1.0, 0.0, specs);
		}

		// Display LOD lines
		if (int(lod_levels.size()) > 0) {
			specs.LineColor = static_color;
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

		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotStairsFlags_Shaded;

		ImPlot::PlotStairs("Static", &static_triangle_counts[0], int(static_triangle_counts.size()), 1.0, 0.0, specs);

		specs.Flags = ImPlotLineFlags_Shaded;
		ImPlot::PlotLine("Dynamic", &bezier_trigs[0], int(bezier_trigs.size()), 1.0, 0.0, specs);

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

int round_to_odd(float t) {
	int res = int(ceil(t));
	return (res % 2 == 0)? res + 1 : res;
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