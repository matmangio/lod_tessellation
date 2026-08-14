#pragma once

#include <vector>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/implot.h>
#include <imgui/implot_internal.h>
#include <utils/dlodObject.h>
#include <utils/data.h>

int k_formatter(double value, char* buff, int size, void* data) {
    if (fabs(value) >= 1000) {  
        return snprintf(buff, size, "%.0fk", value / 1000.0);  
    }  
    return snprintf(buff, size, "%.0f", value); 
}

void prepare_gui_frame(const vector<FrameData>& avg_frame_data) {
    // Return if no data
	if (avg_frame_data.size() == 0) {
		return;
	}

	// Find LOD changes
	vector<int> technique_changes;
	for (int i = 0; i < avg_frame_data.size(); i++) {
		if (i == 0 || avg_frame_data[i].lod_technique != avg_frame_data[i-1].lod_technique) {
			technique_changes.push_back(i);
		}
	}

	// Compute plots
	vector<float> x_time[3];
	vector<float> y_time[3];
	vector<float> x_trigs[3];
	vector<float> y_trigs[3];

	for (int i = 0; i < technique_changes.size(); i++) {
		LODTech tech = avg_frame_data[technique_changes[i]].lod_technique;
		x_time[tech].push_back(technique_changes[i] - 1);
		x_trigs[tech].push_back(technique_changes[i] - 1);
		y_time[tech].push_back(0);
		y_trigs[tech].push_back(0);
		for (int j = technique_changes[i]; j < avg_frame_data.size() && (i == technique_changes.size() - 1 || j < technique_changes[i+1]); j++) {
			x_time[tech].push_back(j);
			x_trigs[tech].push_back(j);
			y_time[tech].push_back(avg_frame_data[j].render_time_ms);
			y_trigs[tech].push_back(avg_frame_data[j].triangles);
		}
		x_time[tech].push_back(x_time[tech][x_time[tech].size() - 1] + 1);
		x_trigs[tech].push_back(x_trigs[tech][x_trigs[tech].size() - 1] + 1);
		y_time[tech].push_back(0);
		y_trigs[tech].push_back(0);
	}

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

    // Init window
    ImGui::Begin("Performance Analysis", NULL, window_flags);

	if (ImPlot::BeginPlot("Frame Times", ImVec2(-1, 0), ImPlotFlags_NoMouseText)) {
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, avg_frame_data.size(), ImPlotCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 2.0f);
		ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, INFINITY);
		ImPlot::SetupAxisFormat(ImAxis_Y1, "%.2f");
		ImPlot::SetupAxisFormat(ImAxis_X1, "");

		// Setup legend
		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImPlotSpec specs;
		specs.FillAlpha = 0.5f;
		specs.Flags = ImPlotLineFlags_Shaded;

		ImPlot::PlotLine("Static", &x_time[LODTech::STATIC][0], &y_time[LODTech::STATIC][0], x_time[LODTech::STATIC].size(), specs);
		ImPlot::PlotLine("Dynamic", &x_time[LODTech::DYNAMIC][0], &y_time[LODTech::DYNAMIC][0], x_time[LODTech::DYNAMIC].size(), specs);
		ImPlot::PlotLine("Bezier", &x_time[LODTech::BEZIER][0], &y_time[LODTech::BEZIER][0], x_time[LODTech::BEZIER].size(), specs);

		ImPlot::EndPlot();
	}

	if (ImPlot::BeginPlot("Triangle count", ImVec2(-1, 0), ImPlotFlags_NoMouseText)) {
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, avg_frame_data.size(), ImPlotCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 300000);
		ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, INFINITY);
		ImPlot::SetupAxisFormat(ImAxis_X1, "");
		ImPlot::SetupAxisFormat(ImAxis_Y1, k_formatter);

		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImPlotSpec specs;
		specs.FillAlpha = 0.5f;
		specs.Flags = ImPlotLineFlags_Shaded;

		ImPlot::PlotLine("Static", &x_trigs[LODTech::STATIC][0], &y_trigs[LODTech::STATIC][0], x_trigs[LODTech::STATIC].size(), specs);
		ImPlot::PlotLine("Dynamic", &x_trigs[LODTech::DYNAMIC][0], &y_trigs[LODTech::DYNAMIC][0], x_trigs[LODTech::DYNAMIC].size(), specs);
		ImPlot::PlotLine("Bezier", &x_trigs[LODTech::BEZIER][0], &y_trigs[LODTech::BEZIER][0], x_trigs[LODTech::BEZIER].size(), specs);

		ImPlot::EndPlot();
	}

    ImGui::End();

    return;
}
