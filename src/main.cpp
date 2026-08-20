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
#include <utils/camera.h>
#include <utils/dlodObject.h>
#include <utils/data.h>
#include <utils/parsers.h>

using namespace std;
using namespace glm;

// Disable Imgui demo windows to save compile time
#define IMGUI_DISABLE_DEMO_WINDOWS

////////////////// PARAMETERS //////////////////
// Window parameters
const GLuint screen_dimensions[2] = {1820, 980};
const GLuint window_position[2] = {0, 40};
const vec3 clear_color = {0.6, 0.6, 0.6};

// Input handling
bool keys[1024];										// An array of booleans for each key on the keyboard

// Camera parameters
Camera camera(vec3(0.0f, 0.0f, 7.0f), false);
const float camera_speed = 10.0f;
const float mouse_sensitivity = 0.15f;

float first_mouse = true;								// True when the mouse was disabled last frame (or on the first frame)
double last_mouse_x = 0.0;								// The last registered mouse position on the x axis
double last_mouse_y = 0.0;								// The last registered mouse position on the y axis

// Lighting parameters
const vec3 light_direction = vec3(0.0f, 1.0f, 0.5f);	// Vector TO the light (i.e. opposite direction of incidence)
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

// GUI parameters
const int frame_window = 20;							// The number of frames over which render times are averaged
const float time_window = 15.0f;						// The time window (in seconds) over which the GUI shows the aggregated data
	
vector<FrameData> avg_frame_data;						// Average frame data used for GUI
float time_accumulator[frame_window] {-1};				// Circular array to store the last {frame_window} render times
int time_accumulator_idx = 0;							// Indices for the circular array

////////////////// FLAGS //////////////////
bool wireframe = true;
bool display_mouse = false;
LODTech lod_tech = LODTech::STATIC;

////////////////// SIGNATURES //////////////////
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void apply_camera_movements(float delta_time);

float average_time(float vec[]);

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

    // Link i/o callbacks
    glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	// Disable the mouse cursor if needed
	if (!display_mouse) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

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

	////////////////// DATA INITIALIZATION //////////////////
	open_data_file("data/tessellation.csv");

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
    // Run all available parsers on the models
	run_all_parsers();

	// Load all objects in the scene by reading .scene file
	vector<DLODObject*> objects = load_scene("./demo.scene");

    ////////////////// SHADERS //////////////////
    // Load the shader programs
    Shader static_shader("shaders/static.vert", "shaders/illumination.frag");
	Shader dynamic_shader("shaders/dynamic.vert", "shaders/illumination.frag", "shaders/dynamic.tcs", "shaders/dynamic.tes");
	Shader bezier_shader("shaders/bezier.vert", "shaders/illumination.frag", "shaders/bezier.tcs", "shaders/bezier.tes");
	Shader shaders[3] = {static_shader, dynamic_shader, bezier_shader};

	////////////////// CAMERA //////////////////
	// Init camera parameters
	camera.MovementSpeed = camera_speed;
	camera.MouseSensitivity = mouse_sensitivity;

    ////////////////// TRANSFORMS //////////////////
	// Projection and view matrices
	mat4 view = camera.GetViewMatrix();
    mat4 projection = perspective(45.0f, (float) screen_width / screen_height, 0.1f, 10000.0f);

    // Model and normal matrices
	mat4 model_matrix = mat4(1.0f);
	mat3 normal_matrix = mat3(1.0f);

    ////////////////// RENDERING LOOP //////////////////
	// Time variables
    float delta_time, current_frame = 0, last_frame = 0;

    while (!glfwWindowShouldClose(window)) {
        // Compute delta time
        current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        // Check for I/O events
        glfwPollEvents();

		// Move camera according to i/o and update view matrix accordingly
		apply_camera_movements(delta_time);
		view = camera.GetViewMatrix();

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Generate queries for render time computations
		vector<GLuint> time_queries_ids(objects.size());
		glGenQueries(objects.size(), &time_queries_ids[0]);

		// Init frame data variables
		GLfloat total_render_time = 0;
		GLuint total_triangle_count = 0;

		// Render each object
		for (int i = 0; i < objects.size(); i++) {
			
			// Compute distance from camera
			float distance_to_camera = distance(objects[i]->Position, camera.Position);

			// Select the appropriate shader
			shaders[lod_tech].Use();

			// Start the computation for the render time
			glBeginQuery(GL_TIME_ELAPSED, time_queries_ids[i]);

			// Send matrices and uniforms
        	glUniformMatrix4fv(glGetUniformLocation(shaders[lod_tech].Program, "projection_matrix"), 1, GL_FALSE, value_ptr(projection));
        	glUniformMatrix4fv(glGetUniformLocation(shaders[lod_tech].Program, "view_matrix"), 1, GL_FALSE, value_ptr(view));
			glUniform3fv(glGetUniformLocation(shaders[lod_tech].Program, "diffuse_color"), 1, diffuse_color[lod_tech]);
        	glUniform3fv(glGetUniformLocation(shaders[lod_tech].Program, "ambient_color"), 1, ambient_color);
        	glUniform3fv(glGetUniformLocation(shaders[lod_tech].Program, "specular_color"), 1, specular_color);
			glUniform3fv(glGetUniformLocation(shaders[lod_tech].Program, "light_direction"), 1, value_ptr(light_direction));
			glUniform1f(glGetUniformLocation(shaders[lod_tech].Program, "k_d"), Kd);
			glUniform1f(glGetUniformLocation(shaders[lod_tech].Program, "k_s"), Ks);
        	glUniform1f(glGetUniformLocation(shaders[lod_tech].Program, "k_a"), Ka);
        	glUniform1f(glGetUniformLocation(shaders[lod_tech].Program, "shininess"), shininess);

			// Compute and send the model and normal matrices
        	model_matrix = mat4(1.0f);
			normal_matrix = mat3(1.0f);
        	model_matrix = translate(model_matrix, objects[i]->Position);
			model_matrix = rotate(model_matrix, radians(objects[i]->Rotation.x), vec3(1.0f, 0.0f, 0.0f));
			model_matrix = rotate(model_matrix, radians(objects[i]->Rotation.y), vec3(0.0f, 1.0f, 0.0f));
			model_matrix = rotate(model_matrix, radians(objects[i]->Rotation.z), vec3(0.0f, 0.0f, 1.0f));
			model_matrix = scale(model_matrix, objects[i]->Scale);
			normal_matrix = inverseTranspose(mat3(view * model_matrix));

        	glUniformMatrix4fv(glGetUniformLocation(shaders[lod_tech].Program, "model_matrix"), 1, GL_FALSE, value_ptr(model_matrix));
			glUniformMatrix3fv(glGetUniformLocation(shaders[lod_tech].Program, "normal_matrix"), 1, GL_FALSE, value_ptr(normal_matrix));

			// Draw based on the LOD technique
			objects[i]->Draw(static_cast<LODTech>(lod_tech), distance_to_camera, shaders[lod_tech]);

			// Stop time computation
			glEndQuery(GL_TIME_ELAPSED);

			// Save the number of triangles used/generated
			total_triangle_count += objects[i]->TriangleCount(static_cast<LODTech>(lod_tech), distance_to_camera);
		}

		// Gather and sum the render times of each object (here to allow the GPU to asynchronously generate that data)
		for (int i = 0; i < objects.size(); i++) {
			GLuint64 result;
			glGetQueryObjectui64v(time_queries_ids[i], GL_QUERY_RESULT, &result);
			glDeleteQueries(1, &time_queries_ids[i]);
			
			GLfloat render_time = (float) result / 1000000;
			total_render_time += render_time;
		}

		// Save frame data on file
		struct FrameData frameData = {
			current_frame,
			lod_tech,
			total_render_time,
			total_triangle_count
		};
		write_frame_data(frameData);

		// Update time accumulators and use average time for gui
		time_accumulator[time_accumulator_idx] = total_render_time;
		time_accumulator_idx = (time_accumulator_idx + 1) % frame_window;
		frameData.render_time_ms = average_time(time_accumulator);

		// Delete the data more than {time_window} seconds away
		avg_frame_data.push_back(frameData);
		while (avg_frame_data.size() > 0 && current_frame - avg_frame_data[0].timestamp > time_window) {
			avg_frame_data.erase(avg_frame_data.begin());	
		}

		// Prepare and render GUI frame
        prepare_gui_frame(avg_frame_data);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Render the current frame
        glfwSwapBuffers(window);
    }

    ////////////////// CLEANUP //////////////////
	for (int tech = LODTech::STATIC; tech <= LODTech::BEZIER; tech++) {
		shaders[tech].Delete();
	}

	close_data_file();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
	ImPlot::DestroyContext();

    glfwTerminate();

    return 0;
}

////////////////// I/O HANDLING //////////////////

// Callback for keyboard events
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ESC: exit window
        glfwSetWindowShouldClose(window, true);
    } else if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        // P: wireframe on/off
        wireframe = !wireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    } else if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
		// TAB: mouse on/off
		display_mouse = !display_mouse;
		if (display_mouse) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			first_mouse = true;
		} else {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	} else if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		// 1: STATIC LOD
		if (lod_tech != LODTech::STATIC) {
			lod_tech = LODTech::STATIC;

			// Reset time accumulator
			time_accumulator_idx = 0;
			for (int i = 0; i < frame_window; i++) {
				time_accumulator[i] = -1;
			}
		}
	} else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		// 2: DYNAMIC LOD
		if (lod_tech != LODTech::DYNAMIC) {
			lod_tech = LODTech::DYNAMIC;

			// Reset time accumulator
			time_accumulator_idx = 0;
			for (int i = 0; i < frame_window; i++) {
				time_accumulator[i] = -1;
			}
		}
	} else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		// 3: BEZIER LOD
		if (lod_tech != LODTech::BEZIER) {
			lod_tech = LODTech::BEZIER;

			// Reset time accumulator
			time_accumulator_idx = 0;
			for (int i = 0; i < frame_window; i++) {
				time_accumulator[i] = -1;
			}
		}
	}

	// Save each key's pressed status
	if (action == GLFW_PRESS) {
		keys[key] = true;
	} else if (action == GLFW_RELEASE) {
		keys[key] = false;
	}
}

// Callback for mouse events
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	// If the mouse is enabled, don't move the camera
	if (display_mouse) {
		return;
	}
	
	// On the first frame we don't have a "last" position, so we initialize the corresponding variables with the current position
    if (first_mouse) {
        last_mouse_x = xpos;
        last_mouse_y = ypos;
        first_mouse = false;
    }

    // Compute the offset of mouse position
    GLfloat xoffset = xpos - last_mouse_x;
    GLfloat yoffset = last_mouse_y - ypos;

    // Update the last position
    last_mouse_x = xpos;
    last_mouse_y = ypos;

    // Update the camera position based on the computed offsets
    camera.ProcessMouseMovement(xoffset, yoffset);

}

// Move the camera according to WASD input
void apply_camera_movements(float delta_time) {
    // Compute the movement bitmap
	int movement_bitmap = 0;
    
    if (keys[GLFW_KEY_W]) {
		movement_bitmap |= Camera_Movement::FORWARD;
	}
	if (keys[GLFW_KEY_S]) {
		movement_bitmap |= Camera_Movement::BACKWARD;
	}
	if (keys[GLFW_KEY_D]) {
		movement_bitmap |= Camera_Movement::RIGHT;
	}
	if (keys[GLFW_KEY_A]) {
		movement_bitmap |= Camera_Movement::LEFT;
	}
	if (keys[GLFW_KEY_E]) {
		movement_bitmap |= Camera_Movement::UP;
	}
	if (keys[GLFW_KEY_Q]) {
		movement_bitmap |= Camera_Movement::DOWN;
	}

	// Apply the final movement
	camera.ProcessKeyboard(movement_bitmap, delta_time);
}

////////////////// HELPER FUNCTIONS //////////////////

// Computes the average of the passed render times over {frame_window} frames
float average_time(float vec[]) {
	float res = 0.0f;
	for (int i = 0; i < frame_window && vec[i] >= 0.0f; i++) {
		res += vec[i];
	}
	return res / frame_window;
}
