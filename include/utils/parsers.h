#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <glm/glm.hpp>
#include <utils/dlodObject.h>

using namespace std;
using namespace glm;

// Load objects in a ".scene" file
vector<DLODObject*> load_scene(const string& path) {
	ifstream file(path);

	// Output
	vector<DLODObject*> objects;

	// Temp data
	string directive;

	bool new_object = false;
	vector<string> static_lod_paths;
	string bezier_path;
	vec3 position = vec3(0.0f);
	vec3 rotation = vec3(0.0f);
	vec3 scale = vec3(1.0f);
	LODParameters lod_params;

	// Read and apply one directive at a time
	while (!file.eof()) {
		file >> directive;

		if (directive == "NewObject") {
			// Reset temp data
			static_lod_paths.clear();
			bezier_path = "";
			position = vec3(0.0f);
			rotation = vec3(0.0f);
			scale = vec3(1.0f);
			lod_params = {};
			new_object = true;
		} else if (directive == "EndObject" && new_object) {
			// Create object
			DLODObject* obj = new DLODObject(static_lod_paths, bezier_path, lod_params);
			obj->Position = position;
			obj->Rotation = rotation;
			obj->Scale = scale;

			// Add it to objects array
			objects.push_back(obj);

			new_object = false;
		} else if (directive == "StaticLODs") {
			int num_of_lods;
			string lod_path;

			file >> num_of_lods;
			for (int i = 0; i < num_of_lods; i++) {
				file >> lod_path;
				static_lod_paths.push_back(lod_path);
			}
		} else if (directive == "Bezier") {
			file >> bezier_path;
		} else if (directive == "DynamicBaseLOD") {
			file >> lod_params.dynamic_base_lod;
		} else if (directive == "Position") {
			file >> position.x;
			file >> position.y;
			file >> position.z;
		} else if (directive == "Rotation") {
			file >> rotation.x;
			file >> rotation.y;
			file >> rotation.z;
		} else if (directive == "Scale") {
			file >> scale.x;
			file >> scale.y;
			file >> scale.z;
		} else if (directive == "MinMaxDistance") {
			file >> lod_params.min_max_distance[0];
			file >> lod_params.min_max_distance[1];
		} else if (directive == "TessExtremesOuter") {
			file >> lod_params.tess_extremes_outer[0][0];
			file >> lod_params.tess_extremes_outer[0][1];
			file >> lod_params.tess_extremes_outer[1][0];
			file >> lod_params.tess_extremes_outer[1][1];
		} else if (directive == "TessExtremesInner") {
			file >> lod_params.tess_extremes_inner[0][0];
			file >> lod_params.tess_extremes_inner[0][1];
			file >> lod_params.tess_extremes_inner[1][0];
			file >> lod_params.tess_extremes_inner[1][1];
		} else if (directive == "StaticBuffer") {
			file >> lod_params.static_buffer_dist;
		} else if (directive == "EarlyBackfaceCulling") {
			int value;
			file >> value;
			lod_params.early_backface_culling = (value > 0)? true : false;
		} else if (directive == "//") {
			// Ignore whole line
			getline(file, directive);
		}
	}

	return objects;
}

// Returns true if a certain file exists, false otherwise
bool file_exists(const string& path) {
    ifstream f(path);
    return f.good();
}

// Converts the .norm file format to the desired .obj file given in the {obj_path} parameter
void convert_norm_to_obj(const string& norm_path) {
    // Open files
    string obj_path = norm_path.substr(0, norm_path.length() - 5) + ".obj";
    
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

void convert_rib_to_bpt(const string& rib_path) {
	string bpt_path = rib_path.substr(0, rib_path.length() - 4) + ".bpt";

	ifstream rib_model(rib_path);
    ofstream bpt_model(bpt_path);

	mat4 transform = mat4(1.0f);
	int patch_count = 0;
	string patches = "";

	string directive;
	vec3 temp_vec;
	float temp_float;

	rib_model >> directive;
	while (!rib_model.eof()) {
		if (directive == "TransformBegin" || directive == "TransformEnd") {
			transform = mat4(1.0f);
		} else if (directive == "Translate") {
			rib_model >> temp_vec.x;
			rib_model >> temp_vec.y;
			rib_model >> temp_vec.z;

			transform = translate(transform, temp_vec);
		} else if (directive == "Rotate") {
			rib_model >> temp_float;
			rib_model >> temp_vec.x;
			rib_model >> temp_vec.y;
			rib_model >> temp_vec.z;

			transform = rotate(transform, radians(temp_float), temp_vec);
		} else if (directive == "Scale") {
			rib_model >> temp_vec.x;
			rib_model >> temp_vec.y;
			rib_model >> temp_vec.z;

			transform = scale(transform, temp_vec);
		} else if (directive == "Patch") {
			// Consume prefixes
			rib_model >> directive;
			rib_model >> directive;
			rib_model >> directive[0];

			for (int i = 0; i < 16; i++) {
				rib_model >> temp_vec.x;
				rib_model >> temp_vec.y;
				rib_model >> temp_vec.z;

				vec4 transformed_cpt = transform * vec4(temp_vec, 1.0f);
				patches.append(to_string(transformed_cpt.x) + " ");
				patches.append(to_string(transformed_cpt.y) + " ");
				patches.append(to_string(transformed_cpt.z) + "\n");
			}
			patches.append("\n");
			patch_count++;

			// Consume suffix
			rib_model >> directive;
		}

		// Read next directive
		rib_model >> directive;
	}

	// Write bpt file
	bpt_model << patch_count << endl;
	bpt_model << "3 3" << endl;
	bpt_model << patches;

	// Close files
	bpt_model.close();
	rib_model.close();
}

// Use all parsers to convert all .norm -> .obj and all .rib -> .bpt
void run_all_parsers() {
	// Check if .obj files are present for the different teapot LODs, create them from the .norm files if not
    for (int i = 0; i < 3; i++) {
        string path = "./models/teapot_surface" + to_string(i) + ".obj";
        if (!file_exists(path)) {
            convert_norm_to_obj("./models/teapot_surface" + to_string(i) + ".norm");
        }
    }
	// Check if .bpt files are present for Gumbo, create them from the .rib files if not
	if (!file_exists("./models/gumbo.bpt")) {
		convert_rib_to_bpt("./models/gumbo.rib");
	}
}
