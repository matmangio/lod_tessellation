#pragma once

#include <stdio.h>
#include <glad/glad.h>
#include <utils/dlodObject.h>

using namespace std;

// Struct for holding the data about a frame
struct FrameData {
	GLfloat timestamp;
	LODTech lod_technique;
	GLfloat render_time_ms;
	GLuint triangles;
};

// Reference to the opened data file
ofstream output;

// Open the data file
void open_data_file(char* path) {
	// Open file
	output.open(path);
	
	// Write header
	output << "Timestamp,LOD Technique,Render Time (ms), Triangle Count" << endl;
}

void write_frame_data(FrameData data) {
	output << data.timestamp << "," << static_cast<int>(data.lod_technique) << "," << data.render_time_ms << "," << data.triangles << endl;
}

void close_data_file() {
	output.close();
}
