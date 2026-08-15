#pragma once

#include <utils/bezier.h>
#include <utils/model.h>
#include <vector>
#include <glm/gtc/quaternion.hpp>

using namespace std;

// Define handles for lod techniques
enum LODTech {
	STATIC = 0,
	DYNAMIC = 1,
	BEZIER = 2
};

class DLODObject {
private:

	// Models of the object's lods, ordered in the canonical way (LOD0 = most detailed)
	vector<Model> lods;

	// Bezier representation of the object
	Bezier bezier;

public:

	// Spatial parameters
	glm::vec3 Position = glm::vec3(0.0f);;
	glm::vec3 RotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
	GLfloat RotationAngle = 0.0f;
	glm::vec3 Scale = glm::vec3(1.0f);

	// Whether or not to perform early backface culling when tessellating
	// WARNING: this may create artifacts if the object isn't completely closed
	bool EarlyBackfaceCulling;

	// Constructor
	DLODObject(vector<char*> lod_paths, char* bezier_path, bool early_backface_culling = true) 
		: bezier(bezier_path) {
		
		// Load a model for each LOD
		for (int i = 0; i < lod_paths.size(); i++) {
			this->lods.push_back(Model(lod_paths[i], true));
		}

		EarlyBackfaceCulling = early_backface_culling;
	}

	// Draw the object on screen
	void Draw(LODTech tech, int lod_level = 0) {
		switch (tech) {
		case LODTech::STATIC:
			this->lods[lod_level].Draw();
			break;
		case LODTech::DYNAMIC:
			this->lods[this->lods.size() - 1].Draw(true);
			break;
		case LODTech::BEZIER:
			this->bezier.Draw();
			break;
		default:
			break;
		}
	}

	// Compute the object's triangle count given a technique index and a tessellation level
	int TriangleCount(LODTech tech, int lod_level = 0, float tess_level_outer = 0.0f, float tess_level_inner = 0.0f) {
		if (tech == LODTech::STATIC) {
			int tot = 0;
			for (int i = 0; i < lods[lod_level].meshes.size(); i++) {
				tot += lods[lod_level].meshes[i].indices.size() / 3;
			}
			return tot;
		} else if (tech == LODTech::DYNAMIC) {
			int actual_tess_level_outer = ceil(tess_level_outer);
			int actual_tess_level_inner = ceil(tess_level_inner);
			int trigs_per_patch = 1;
			if (actual_tess_level_outer != 1) {
				trigs_per_patch = 3 * (actual_tess_level_outer - 1) + 1;
			}

			int tot = 0;
			for (int i = 0; i < lods[lods.size() - 1].meshes.size(); i++) {
				tot += lods[lods.size() - 1].meshes[i].indices.size() / 3;
			}

			return tot * trigs_per_patch;
		} else if (tech == LODTech::BEZIER) {
			int actual_tess_level_outer = round_to_odd(tess_level_outer);
			int actual_tess_level_inner = round_to_odd(tess_level_inner);
			int trigs_per_patch = 4 * (actual_tess_level_outer + actual_tess_level_inner - 2) + 2 * pow(actual_tess_level_inner - 2, 2);
			
			return bezier.patches * trigs_per_patch;
		}

		// If defaulted, return -1 as error
		return -1;
	}

private:

	////////////// UTILS //////////////
	
	// Rounds the number to the next nearest odd
	int round_to_odd(float t) {
		if (t < 0) t = 0.0f;

		int res = int(ceil(t));
		return (res % 2 == 0)? res + 1 : res;
	}

};