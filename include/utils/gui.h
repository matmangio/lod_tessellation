#include <vector>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/implot.h>
#include <imgui/implot_internal.h>

#define STATIC 0
#define DYNAMIC 1
#define BEZIER 2

using namespace std;

int k_formatter(double value, char* buff, int size, void* data) {
    if (fabs(value) >= 1000) {  
        return snprintf(buff, size, "%.0fk", value / 1000.0);  
    }  
    return snprintf(buff, size, "%.0f", value); 
}

void prepare_gui_frame(const vector<float> avg_times[], const vector<int> trigs[], bool display_lods) {
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

	// Find LOD changes
	vector<int> lod_changes;
	for (int i = 1; i < int(trigs[STATIC].size()) - 1; i++) {
		if (trigs[STATIC][i] != trigs[STATIC][i+1]) {
			lod_changes.push_back(i+1);
		}
	}

    // Init window
    ImGui::Begin("Performance Analysis", NULL, window_flags);

	if (ImPlot::BeginPlot("Frame Times", ImVec2(-1, 0), ImPlotFlags_NoMouseText)) {
		// ImPlot::SetupAxes("##", "##", 0, ImPlotAxisFlags_AutoFit);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, trigs[STATIC].size() - 1, ImPlotCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 1.0f);
		ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, INFINITY);
		ImPlot::SetupAxisFormat(ImAxis_Y1, "%.2f");
		ImPlot::SetupAxisFormat(ImAxis_X1, "");

		// Setup legend
		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImVec4 static_color;
		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotLineFlags_Shaded;

		if (int(avg_times[STATIC].size()) > 0) {
			ImPlot::PlotLine("Static", &avg_times[STATIC][0], int(avg_times[STATIC].size()), 1.0, 0.0, specs);
			static_color = ImPlot::GetLastItemColor();
			ImPlot::PlotLine("Dynamic", &avg_times[DYNAMIC][0], int(avg_times[DYNAMIC].size()), 1.0, 0.0, specs);
			ImPlot::PlotLine("Bezier", &avg_times[BEZIER][0], int(avg_times[BEZIER].size()), 1.0, 0.0, specs);
		}

		// Display LOD lines
		if (int(trigs[STATIC].size()) > 0 && display_lods) {
			specs.LineColor = static_color;
			ImPlot::PlotInfLines("##LOD Changes", &lod_changes[0], int(lod_changes.size()), specs);
		}

		ImPlot::EndPlot();
	}

	if (ImPlot::BeginPlot("Triangle count", ImVec2(-1, 0), ImPlotFlags_NoMouseText)) {
		ImPlot::SetupAxesLimits(0,trigs[STATIC].size()-1, 0, 175000, ImPlotCond_Always);
		ImPlot::SetupAxisFormat(ImAxis_X1, "");
		ImPlot::SetupAxisFormat(ImAxis_Y1, k_formatter);

		ImPlotLegendFlags legend_flags = ImPlotLegendFlags_Horizontal;
		ImPlot::SetupLegend(ImPlotLocation_NorthWest, legend_flags);

		ImPlotSpec specs;
		specs.FillAlpha = 0.25f;
		specs.Flags = ImPlotStairsFlags_Shaded;

		ImPlot::PlotStairs("Static", &trigs[STATIC][0], int(trigs[STATIC].size()), 1.0, 0.0, specs);

		specs.Flags = ImPlotLineFlags_Shaded;
		ImPlot::PlotLine("Dynamic", &trigs[DYNAMIC][0], int(trigs[DYNAMIC].size()), 1.0, 0.0, specs);
		ImPlot::PlotLine("Bezier", &trigs[BEZIER][0], int(trigs[BEZIER].size()), 1.0, 0.0, specs);

		ImPlot::EndPlot();
	}

    ImGui::End();

    return;
}
