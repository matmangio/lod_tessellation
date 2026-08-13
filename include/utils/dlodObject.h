#pragma once

#include <utils/bezier.h>
#include <utils/model.h>
#include <vector>
#include <glm/gtc/quaternion.hpp>

using namespace std;

// Define handles for lod techniques
#define STATIC 0
#define DYNAMIC 1
#define BEZIER 2

class DLODObject {
private:

	// Models of the object's lods, ordered in the canonical way (LOD0 = most detailed)
	vector<Model> lods;

	// Bezier representation of the object
	Bezier bezier;

public:

	// Spatial parameters
	glm::vec3 Position;
	glm::quat Rotation;

	// Constructor
	DLODObject(vector<char*> lod_paths, char* bezier_path) 
		: bezier(bezier_path) {
		
		// Load a model for each LOD
		for (int i = 0; i < lod_paths.size(); i++) {
			this->lods.push_back(Model(lod_paths[i], true));
		}
	}

	// Delete default copy constructor & assignment
	DLODObject(const DLODObject& copy) = delete;
    DLODObject& operator=(const DLODObject&) = delete;

	// Draw the object on screen
	void Draw(int technique, int lod_level = 0) {
		switch (technique) {
		case STATIC:
			this->lods[lod_level].Draw();
			break;
		case DYNAMIC:
			this->lods[this->lods.size() - 1].Draw(true);
			break;
		case BEZIER:
			this->bezier.Draw();
			break;
		default:
			break;
		}
	}

};