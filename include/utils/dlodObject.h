#pragma once

#include <utils/bezier.h>
#include <utils/model.h>
#include <vector>
#include <string>
#include <glm/gtc/quaternion.hpp>

using namespace std;

// Define handles for lod techniques
enum LODTech {
	STATIC = 0,
	DYNAMIC = 1,
	BEZIER = 2
};

// Struct for holding LOD params
struct LODParameters {
	// The min-max distances for deciding LOD level
	// - Anything below the min is assured to be LOD0
	// - Anything above the max is assured to be the coarsest LOD
	// - The space in-between is equally divided among the other LODs
	// NOTE: at least 3 LODs are required
	GLfloat min_max_distance[2] = {12.0f, 36.0f};

	// The extremes for the outer tessellation levels of DYNAMIC (0) and BEZIER (1)
	GLfloat tess_extremes_outer[2][2] = {
		{0.1f, 16.0f},
		{3.0f, 64.0f}
	};

	// The extremes for the inner tessellation levels of DYNAMIC (0) and BEZIER (1)
	GLfloat tess_extremes_inner[2][2] = {
		{1.0f, 2.0f},
		{3.0f, 56.0f}
	};

	// Subtracted from the static boundary when moving backwards from a higher to a lower LOD to avoid flickering
	GLfloat static_buffer_dist = 3.0f;

	// Whether or not to perform early backface culling when tessellating
	// WARNING: this may create artifacts if the object isn't completely closed
	bool early_backface_culling = true;

	// The LOD to use when applying the DYNAMIC method
	int dynamic_base_lod = -1;

	// The coefficient of displacement for each vertex along its normal when applying the DYNAMIC method
	float dynamic_displacement_factor = 0.05;
};

struct BezierMeshRef {
	string path;			// The path to the bezier "mesh"
	bool inverse_order;		// Whether or not the bezier "mesh" control points are in inverse order
};

class DLODObject {
private:

	// Models of the object's lods, ordered in the canonical way (LOD0 = most detailed)
	vector<Model> lods;

	// Bezier representation of the object
	vector<Bezier> bezier;
	vector<bool> inverts;

	// LOD Parameters
	LODParameters lod_params;
	vector<GLfloat> static_boundaries;
	int last_static_lod = -1;

public:

	// Spatial parameters
	glm::vec3 Position = glm::vec3(0.0f);
	glm::vec3 Rotation = glm::vec3(0.0f);	// Euler angles (degrees)
	glm::vec3 Scale = glm::vec3(1.0f);

	// Disallow copy
	DLODObject(const DLODObject& copy) = delete; 
    DLODObject& operator=(const DLODObject&) = delete;

	// Constructor
	DLODObject(vector<string>& lod_paths, vector<BezierMeshRef>& bezier_params, LODParameters params) {
		
		// Load a model for each LOD
		for (int i = 0; i < lod_paths.size(); i++) {
			this->lods.push_back(Model(lod_paths[i], true));
		}

		// Create all bezier for the model and save their corresponding value for inverse_order
		for (int i = 0; i < bezier_params.size(); i++) {
			Bezier b(bezier_params[i].path);

			bezier.push_back(b);
			inverts.push_back(bezier_params[i].inverse_order);
		}
		
		// Load LOD parameters
		lod_params = params;

		// Compute static LOD boundaries
		int other_lods = lods.size() - 2;
		float accumulator = lod_params.min_max_distance[0];
		static_boundaries.push_back(accumulator);	// The first boundary is min_distance
		for (int i = 0; i < other_lods; i++) {
			accumulator += (lod_params.min_max_distance[1] - lod_params.min_max_distance[0]) / other_lods;
			static_boundaries.push_back(accumulator);
		}
	}

	// Draw the object on screen
	void Draw(LODTech tech, GLfloat distance_to_camera, const Shader& shader, bool early_culling = true) {
		if (tech == LODTech::STATIC) {
			// Compute LOD level
			int lod_level = get_static_lod(distance_to_camera);

			// Draw appropriate LOD
			this->lods[lod_level].Draw();
		} else {
			// Compute tessellation levels and send them to the shader
			float tess_level_outer = get_outer_tess_level(tech, distance_to_camera);
			float tess_level_inner = get_inner_tess_level(tech, distance_to_camera);

			glUniform1i(glGetUniformLocation(shader.Program, "early_backface_culling"), early_culling && this->lod_params.early_backface_culling);
			glUniform1f(glGetUniformLocation(shader.Program, "tess_level_outer"), tess_level_outer);
			glUniform1f(glGetUniformLocation(shader.Program, "tess_level_inner"), tess_level_inner);
			glUniform1f(glGetUniformLocation(shader.Program, "displacement_factor"), this->lod_params.dynamic_displacement_factor);

			// Draw based on technique
			if (tech == LODTech::DYNAMIC) {
				// Select max lod (coarser) if dynamic_base_lod isn't set
				int base_lod = this->lods.size() - 1;
				if (this->lod_params.dynamic_base_lod >= 0) {
					base_lod = glm::min(base_lod, this->lod_params.dynamic_base_lod);
				}
				this->lods[base_lod].Draw(true);
			} else if (tech == LODTech::BEZIER) {
				for (int i = 0; i < bezier.size(); i++) {
					// For each "bezier mesh" send whether they should invert
					glUniform1i(glGetUniformLocation(shader.Program, "invert_order"), this->inverts[i]);
					
					this->bezier[i].Draw();
				}
			}
		}
	}

	// Compute the object's triangle count given a technique index and a tessellation level
	int TriangleCount(LODTech tech, float distance_to_camera) {
		if (tech == LODTech::STATIC) {
			int lod_level = get_static_lod(distance_to_camera);

			int tot_trigs = 0;
			for (int i = 0; i < lods[lod_level].meshes.size(); i++) {
				tot_trigs += lods[lod_level].meshes[i].indices.size() / 3;
			}
			return tot_trigs;
		} else if (tech == LODTech::DYNAMIC) {
			// DYNAMIC uses equal_spacing (clamp in [1, max] then ceil)
			int actual_tess_level_outer = ceil(glm::clamp(get_outer_tess_level(tech, distance_to_camera), 1.0f, (float) GL_MAX_TESS_GEN_LEVEL));
			int actual_tess_level_inner = ceil(glm::clamp(get_inner_tess_level(tech, distance_to_camera), 1.0f, (float) GL_MAX_TESS_GEN_LEVEL));
			
			// Compute how many triangles each patch is tessellated into
			int trigs_per_patch = 1;
			if (actual_tess_level_outer > 1 || actual_tess_level_inner > 1) {
			
				// Minimum inner level when tessellating is 2
				if (actual_tess_level_inner == 1) {
					actual_tess_level_inner = 2;
				}

				// We compute how many triangles sit between each ring
				int last_edges = actual_tess_level_outer;			// The first outer edge uses the actual outer tessellation
				int current_edges = actual_tess_level_inner - 2;	// The first inner edge uses the inner tessellation - 2
				trigs_per_patch = 0;
				while (current_edges >= 0) {

					trigs_per_patch += 3 * (last_edges + current_edges);	// 3 since triangle has 3 sides

					last_edges = current_edges;			// The current inner edge becomes the next outer edge
					current_edges = current_edges - 2;	// Each ring the number of edges reduces by 2
				}
			}

			// Get base tessellated lod and its total number of triangles
			int base_lod = this->lods.size() - 1;
			if (this->lod_params.dynamic_base_lod >= 0) {
				base_lod = glm::min(base_lod, this->lod_params.dynamic_base_lod);
			}

			int tot_trigs = 0;
			for (int i = 0; i < lods[base_lod].meshes.size(); i++) {
				tot_trigs += lods[base_lod].meshes[i].indices.size() / 3;
			}

			// Each triangle is tessellated with the same tessellation levels
			return tot_trigs * trigs_per_patch;
		} else if (tech == LODTech::BEZIER) {
			// BEZIER uses fractional_odd_spacing for smoother transitions (clamp in [1, max - 1] then ceil to odd)
			int actual_tess_level_outer = round_to_odd(glm::clamp(get_outer_tess_level(tech, distance_to_camera), 1.0f, (float) GL_MAX_TESS_GEN_LEVEL - 1));
			int actual_tess_level_inner = round_to_odd(glm::clamp(get_inner_tess_level(tech, distance_to_camera), 1.0f, (float) GL_MAX_TESS_GEN_LEVEL - 1));
			
			// Compute how many triangles each patch is tessellated into
			int trigs_per_patch = 2;
			if (actual_tess_level_outer > 1 || actual_tess_level_inner > 1) {

				// Minimum inner level when tessellating is 3
				if (actual_tess_level_inner == 1) {
					actual_tess_level_inner = 3;
				}

				// We compute how many triangles sit between each ring
				int last_edges = actual_tess_level_outer;			// The first outer edge uses the actual outer tessellation
				int current_edges = actual_tess_level_inner - 2;	// The first inner edge uses the inner tessellation - 2 
				trigs_per_patch = 0;
				while (current_edges >= 0) {

					trigs_per_patch += 4 * (last_edges + current_edges);	// 4 since quad has 4 sides

					last_edges = current_edges;			// The current inner edge becomes the next outer edge
					current_edges = current_edges - 2;	// Each ring the number of edges reduces by 2
				}

			}

			int total_patches = 0;
			for (int i = 0; i < bezier.size(); i++) {
				total_patches += bezier[i].patches;
			}

			return total_patches * trigs_per_patch;
		}

		// If defaulted, return -1 as error
		return -1;
	}

private:

	////////////// LOD SELECTION //////////////

	// Select the current LOD level based on distance from camera
	int get_static_lod(float distance_to_camera) {
		int lod_level = 0;
		for (lod_level = 0; lod_level < lods.size() - 1; lod_level++) {
			float boundary = (lod_level == this->last_static_lod - 1)? 
				this->static_boundaries[lod_level] - this->lod_params.static_buffer_dist : this->static_boundaries[lod_level];
			if (distance_to_camera < boundary) {
				break;
			}
		}
		// Save the level as the last lod
		last_static_lod = lod_level;

		return lod_level;
	}

	// Select the current outer tessellation level based on distance from camera
	float get_outer_tess_level(LODTech tech, float distance_to_camera) {
		float t = glm::clamp((distance_to_camera - this->lod_params.min_max_distance[0]) / (this->lod_params.min_max_distance[1] - this->lod_params.min_max_distance[0]), 0.0f, 1.0f);
		float tess_level_outer = this->lod_params.tess_extremes_outer[tech - 1][0] * t + this->lod_params.tess_extremes_outer[tech - 1][1] * (1 - t);
	
		return tess_level_outer;
	}

	// Select the current inner tessellation level based on distance from camera
	float get_inner_tess_level(LODTech tech, float distance_to_camera) {
		float t = glm::clamp((distance_to_camera - this->lod_params.min_max_distance[0]) / (this->lod_params.min_max_distance[1] - this->lod_params.min_max_distance[0]), 0.0f, 1.0f);
		float tess_level_inner = this->lod_params.tess_extremes_inner[tech - 1][0] * t + this->lod_params.tess_extremes_inner[tech - 1][1] * (1 - t);
	
		return tess_level_inner;
	}

	////////////// UTILS //////////////
	
	// Rounds the number to the next nearest odd
	int round_to_odd(float t) {
		if (t < 0) t = 0.0f;

		int res = int(ceil(t));
		return (res % 2 == 0)? res + 1 : res;
	}

};