#include <iostream>
#include <fstream>
#include <string>
#include <math.h>

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

#include <utils/model.h>
#include <utils/shader.h>
#include <utils/bezier.h>
#include <utils/gui.h>

using namespace std;
using namespace glm;

// Disable Imgui demo windows to save compile time
#define IMGUI_DISABLE_DEMO_WINDOWS

// Define indices for different LOD types
#define STATIC 0
#define DYNAMIC 1
#define BEZIER 2

////////////////// CONSTANTS //////////////////
// Window parameters
const GLuint screen_dimensions[2] = {1920, 900};
const GLuint window_position[2] = {0, 40};
const vec3 clear_color = {0.6, 0.6, 0.6};

// Movement parameters
const float movement_speed = 4.0f;
const float rotation_speed = 30.0f;
const float min_max_distance[2] = {-8.0f, -50.0f};
const vec3 position_offset = vec3{7.0f, 0.0f, 0.0f};	// The offset between different teapots
const vec3 light_offset = vec3(0.0f, 4.0f, 4.0f);		// The offset of the light source from each teapot

// Lighting parameters
const GLfloat diffuse_color[3][3] = {
	{0.298f, 0.447f, 0.69f},
	{0.866f, 0.517f, 0.321f},
	{0.333f, 0.658f, 0.407f}
};
const GLfloat specular_color[3] = {1.0f, 1.0f, 1.0f};
const GLfloat ambient_color[3] = {0.1f, 0.1f, 0.1f};
const GLfloat Kd = 0.5f;
const GLfloat Ks = 0.3f;
const GLfloat Ka = 0.2f;
const GLfloat shininess = 25.0f;

// LOD and Tessellation parameters (min - max)
const GLfloat tess_extremes_outer[3][2] = {
	{0.0f, 2.0f},	// STATIC
	{0.1f, 4.9f},	// DYNAMIC
	{3.0f, 64.0f}	// BEZIER
};
const GLfloat tess_extremes_inner[3][2] = {
	{0.0f, 2.0f},	// STATIC
	{1.0f, 1.0f},	// DYNAMIC
	{3.0f, 32.0f}	// BEZIER
};
const GLfloat dynamic_displacement = 0.025;

// Data parameters
const int frame_window = 20;							// The number of frames over which render times are averaged
const float time_window = 15.0f;						// The time window (in seconds) over which the GUI shows the aggregated data

////////////////// FLAGS //////////////////
bool wireframe = true;
bool movement = true;
bool rotation = true;
bool display_lods_in_gui = false;

////////////////// SIGNATURES //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

float average_time(float vec[]);
void poll_time_queries(GLuint query_id, float accumulator[], int *index);

int round_to_odd(float t);
int get_triangle_count(int tech, float tess_level_outer, float tess_level_inner);
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

	// Make window non-resizable
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

    // Link keyboard callback
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
    glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.0);

	// Set wireframe mode
	if (wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

    ////////////////// GUI INITIALIZATION //////////////////
    // Setup ImGui and ImPlot contexts
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImPlot::CreateContext();

	// Disable keyboard navigation in UI
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;

    // Connect ImGui to GLFW window
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
	Shader bezier_shader("shaders/bezier.vert", "shaders/illumination.frag", "shaders/bezier.tcs", "shaders/bezier.tes");
	Shader shaders[3] = {static_shader, dynamic_shader, bezier_shader};

    ////////////////// TRANSFORMS //////////////////
    // Projection and view matrices
    mat4 projection = perspective(45.0f, (float) screen_width / screen_height, 0.1f, 10000.0f);
    mat4 view = lookAt(vec3(0.0f, 0.0f, 7.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));

    // Model and normal matrices
	mat4 model_matrix = mat4(1.0f);
	mat3 normal_matrix = mat3(1.0f);

	// Movement variables
    vec3 position = vec3(-7.0f, -1.0f, min_max_distance[0]);
	vec3 light_position = position + light_offset;
    vec3 direction = vec3(0.0f, 0.0f, -1.0f);
	float rotation_angle = 0.0f;

    ////////////////// RENDERING LOOP //////////////////
	// Time variables
    float delta_time, current_frame = 0, last_frame = 0;

	// Data collection variables
	vector<float> timestamps;							// Timestamps of the collected data (equal across all techniques)
	vector<float> avg_times[3];							// Render times for each technique averaged over {frame_window} frames
	vector<int> trigs[3];								// Triangle counts for each technique
	
	float time_accumulator[3][frame_window] {-1};		// Circular arrays to store the last {frame_window} render times
	int time_accumulator_idx[3] {0};					// Indices for the circular arrays

    while (!glfwWindowShouldClose(window)) {
        // Compute delta time
        current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        // Check for I/O events
        glfwPollEvents();

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update position and rotation according to the respective flags
		if (movement) {
			position += (movement_speed * delta_time) * direction;
			if (position.z >= min_max_distance[0]) {
				direction = {0.0f, 0.0f, -1.0f};
			} else if (position.z <= min_max_distance[1]) {
				direction = {0.0f, 0.0f, 1.0f};
			}
		}
		if (rotation) {
			rotation_angle = fmod(rotation_angle + rotation_speed * delta_time, 360.0f);
		}

		// Generate queries for time computations
		GLuint time_queries_ids[3];
		glGenQueries(3, time_queries_ids);

		// Render each model
		for (int tech = STATIC; tech <= BEZIER; tech++) {
			// Update model and light positions
			vec3 model_position = position + position_offset * (float) tech;
			light_position = model_position + light_offset;

			// Select LOD/tessellation level
			float t = (position.z - min_max_distance[0]) / (min_max_distance[1] - min_max_distance[0]);
			float tess_level_outer = tess_extremes_outer[tech][0] * t + tess_extremes_outer[tech][1] * (1 - t);
			float tess_level_inner = tess_extremes_inner[tech][0] * t + tess_extremes_inner[tech][1] * (1 - t);

			// Select the appropriate shader
			shaders[tech].Use();

			// Start the computation for the render time
			glBeginQuery(GL_TIME_ELAPSED, time_queries_ids[tech]);

			// Send matrices and uniforms
        	glUniformMatrix4fv(glGetUniformLocation(shaders[tech].Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        	glUniformMatrix4fv(glGetUniformLocation(shaders[tech].Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));
			glUniform3fv(glGetUniformLocation(shaders[tech].Program, "diffuse_color"), 1, diffuse_color[tech]);
        	glUniform3fv(glGetUniformLocation(shaders[tech].Program, "ambient_color"), 1, ambient_color);
        	glUniform3fv(glGetUniformLocation(shaders[tech].Program, "specular_color"), 1, specular_color);
			glUniform3fv(glGetUniformLocation(shaders[tech].Program, "light_position"), 1, value_ptr(light_position));
			glUniform1f(glGetUniformLocation(shaders[tech].Program, "k_d"), Kd);
			glUniform1f(glGetUniformLocation(shaders[tech].Program, "k_s"), Ks);
        	glUniform1f(glGetUniformLocation(shaders[tech].Program, "k_a"), Ka);
        	glUniform1f(glGetUniformLocation(shaders[tech].Program, "shininess"), shininess);

			glUniform1f(glGetUniformLocation(shaders[tech].Program, "tess_level_outer"), tess_level_outer);
			glUniform1f(glGetUniformLocation(shaders[tech].Program, "tess_level_inner"), tess_level_inner);
			glUniform1f(glGetUniformLocation(shaders[tech].Program, "dynamic_displacement"), dynamic_displacement);

			// Compute and send the model and normal matrices
        	model_matrix = mat4(1.0f);
			normal_matrix = mat3(1.0f);
        	model_matrix = translate(model_matrix, model_position);
			model_matrix = rotate(model_matrix, radians(rotation_angle), vec3(0, 1, 0));
			normal_matrix = inverseTranspose(mat3(view * model_matrix));

        	glUniformMatrix4fv(glGetUniformLocation(shaders[tech].Program, "model_matrix"), 1, GL_FALSE, value_ptr(model_matrix));
			glUniformMatrix3fv(glGetUniformLocation(shaders[tech].Program, "normal_matrix"), 1, GL_FALSE, value_ptr(normal_matrix));

			// Draw based on the LOD technique
			if (tech == STATIC) {
				int lod_level = round(tess_level_outer);
				if (lod_level == 2) {
        		    teapot_lod2.Draw();
        		} else if (lod_level == 1) {
        		    teapot_lod1.Draw();
        		} else {
        		    teapot_lod0.Draw();
        		}
			} else if (tech == DYNAMIC) {
				teapot_lod0.Draw(true);
			} else {
				teapot_bezier.Draw();
			}

			// Stop time computation
			glEndQuery(GL_TIME_ELAPSED);

			// Save the number of triangles used/generated
			int triangle_count = get_triangle_count(tech, tess_level_outer, tess_level_inner);
			trigs[tech].push_back(triangle_count);
		}

		// Gather the render times of each technique (here to allow the GPU to asynchronously generate that data)
		for (int tech = STATIC; tech <= BEZIER; tech++) {
			poll_time_queries(time_queries_ids[tech], time_accumulator[tech], &time_accumulator_idx[tech]);
			avg_times[tech].push_back(average_time(time_accumulator[tech]));
		}

		// Delete the data more than {time_window} seconds away
		timestamps.push_back(current_frame);
		while (timestamps.size() > 0 && current_frame - timestamps[0] > time_window) {
			for (int tech = STATIC; tech <= BEZIER; tech++) {
				avg_times[tech].erase(avg_times[tech].begin());
				trigs[tech].erase(trigs[tech].begin());
			}
			timestamps.erase(timestamps.begin());	
		}

		// Prepare and render GUI frame
        prepare_gui_frame(avg_times, trigs, display_lods_in_gui);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render the current frame
        glfwSwapBuffers(window);
    }

    ////////////////// CLEANUP //////////////////
	for (int tech = STATIC; tech <= BEZIER; tech++) {
		shaders[tech].Delete();
	}
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	ImPlot::DestroyContext();

    glfwTerminate();

    return 0;
}

////////////////// KEYBOARD CALLBACKS //////////////////
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
	} else if (key == GLFW_KEY_L && action == GLFW_PRESS) {
		// L: lods on/off
		display_lods_in_gui = !display_lods_in_gui;
	}
}

////////////////// HELPER FUNCTIONS //////////////////

// Compute the proper triangle count given a technique index and a tessellation level
int get_triangle_count(int tech, float tess_level_outer, float tess_level_inner) {
	if (tech == STATIC) {
		int lod_level = round(tess_level_outer);
		return (lod_level == 0)? 3488 : (lod_level == 1)? 19480 : 145620;
	} else if (tech == DYNAMIC) {
		int actual_tess_level_outer = ceil(tess_level_outer);
		int actual_tess_level_inner = ceil(tess_level_inner);
		int trigs_per_patch = 1;
		if (actual_tess_level_outer != 1) {
			trigs_per_patch = 3 * (actual_tess_level_outer - 1) + 1;
		}
		return 3488 * trigs_per_patch;
	} else {
		int actual_tess_level_outer = round_to_odd(tess_level_outer);
		int actual_tess_level_inner = round_to_odd(tess_level_inner);
		int trigs_per_patch = 4 * (actual_tess_level_outer + actual_tess_level_inner - 2) + 2 * pow(actual_tess_level_inner - 2, 2);
		return 28 * trigs_per_patch;
	}
}

// Returns true if a certain file exists, false otherwise
bool file_exists(const string& path) {
    ifstream f(path);
    return f.good();
}

// Rounds the number to the next nearest odd
int round_to_odd(float t) {
	if (t < 0) t = 0.0f;

	int res = int(ceil(t));
	return (res % 2 == 0)? res + 1 : res;
}

// Computes the average of the passed render times over {frame_window} frames
float average_time(float vec[]) {
	float res = 0.0f;
	for (int i = 0; i < frame_window && vec[i] >= 0.0f; i++) {
		res += vec[i];
	}
	return res / frame_window;
}

// Gets the result of the render time query and stores it in the {accumulator} circular array
void poll_time_queries(GLuint query_id, float accumulator[], int *index) {
	GLuint64 result;
	glGetQueryObjectui64v(query_id, GL_QUERY_RESULT, &result);
	glDeleteQueries(1, &query_id);

	accumulator[*index] = (float) result / 1000000;
	*index = (*index + 1) % frame_window;
}

// Converts the .norm file format to the desired .obj file given in the {obj_path} parameter
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