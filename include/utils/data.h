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
	bool wireframe;
	bool early_culling;
};

// Reference to the current data file
ofstream output;

// Open the data file
void open_data_file(const char* path) {
	// Open file
	output.open(path);
	
	// Write header
	output << "timestamp,lod_tech,render_time_ms,triangles,wireframe,early_culling" << endl;
}

void write_frame_data(FrameData data) {
	output 	<< data.timestamp << "," 
			<< static_cast<int>(data.lod_technique) << "," 
			<< data.render_time_ms << "," 
			<< data.triangles << "," 
			<< data.wireframe << ","
			<< data.early_culling << endl;
}

void close_data_file() {
	output.close();
}
