#pragma once

#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

using namespace std;

class Bezier {
public:

	vector<glm::vec3> control_points;
	int patches;
	int degrees[2];

	GLuint VAO;

	Bezier(string path) {
		this->load_file(path.c_str());
		this->setup_mesh();
	}

	void Draw() {
		glBindVertexArray(this->VAO);

		glPatchParameteri(GL_PATCH_VERTICES, control_pts_per_patch());
        glDrawArrays(GL_PATCHES, 0, control_points.size());

        glBindVertexArray(0);
	}

private:

	GLuint VBO;

	// Open the file in BPT format and save its vertex coordinates in the control_points vector
	void load_file(const char* path) {
		// Open the file
		ifstream file;
		file.open(path);

		// Read the number of patches to read
		file >> patches;

		// Read the degrees of the patches
		file >> degrees[0];
		file >> degrees[1];

		// For each patch, read its 16 points and add them to the control_points list
		glm::vec3 tmp;
		for (int i = 0; i < patches * control_pts_per_patch(); i++) {
			file >> tmp.x;
			file >> tmp.y;
			file >> tmp.z;
			control_points.push_back(tmp);
		}

		// Close the file
		file.close();
	}

	void setup_mesh() {
		// Create the buffers and bind them
        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &this->VBO);

        glBindVertexArray(this->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, this->VBO);

		// Copy data in the VBO
        glBufferData(GL_ARRAY_BUFFER, this->control_points.size() * sizeof(glm::vec3), &this->control_points[0], GL_STATIC_DRAW);

        // Set the pointer to the control point's position in the VAO
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        // Unbind the VBO and VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
	}

	int control_pts_per_patch() {
		return (degrees[0] + 1) * (degrees[1] + 1);
	}

};