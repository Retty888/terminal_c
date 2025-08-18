#include "imgui.h"
#include "implot.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"
#include "golden/chart.h"

struct SoftwareRenderer {
    int width;
    int height;
    std::vector<unsigned char> pixels;
    SoftwareRenderer(int w, int h) : width(w), height(h), pixels(w*h*4, 255) {}
    void render(ImDrawData* data) {
        for (int n = 0; n < data->CmdListsCount; ++n) {
            const ImDrawList* list = data->CmdLists[n];
            const ImDrawIdx* idx = list->IdxBuffer.Data;
            const ImDrawVert* vtx = list->VtxBuffer.Data;
            for (int cmd_i = 0; cmd_i < list->CmdBuffer.Size; ++cmd_i) {
                const ImDrawCmd& pcmd = list->CmdBuffer[cmd_i];
                for (unsigned int i = 0; i < pcmd.ElemCount; i += 3) {
                    ImDrawIdx i0 = idx[i+0];
                    ImDrawIdx i1 = idx[i+1];
                    ImDrawIdx i2 = idx[i+2];
                    draw_triangle(pcmd.ClipRect, vtx[i0], vtx[i1], vtx[i2]);
                }
                idx += pcmd.ElemCount;
            }
        }
    }
    void draw_triangle(const ImVec4& clip, const ImDrawVert& v0, const ImDrawVert& v1, const ImDrawVert& v2) {
        float min_x = std::max(clip.x, std::floor(std::min({v0.pos.x, v1.pos.x, v2.pos.x})));
        float max_x = std::min(clip.z, std::ceil(std::max({v0.pos.x, v1.pos.x, v2.pos.x})));
        float min_y = std::max(clip.y, std::floor(std::min({v0.pos.y, v1.pos.y, v2.pos.y})));
        float max_y = std::min(clip.w, std::ceil(std::max({v0.pos.y, v1.pos.y, v2.pos.y})));
        float area = (v1.pos.x - v0.pos.x)*(v2.pos.y - v0.pos.y) - (v1.pos.y - v0.pos.y)*(v2.pos.x - v0.pos.x);
        if (area == 0) return;
        for (int y = (int)min_y; y < (int)max_y; ++y) {
            for (int x = (int)min_x; x < (int)max_x; ++x) {
                float px = x + 0.5f;
                float py = y + 0.5f;
                float w0 = (v1.pos.x - v0.pos.x)*(py - v0.pos.y) - (v1.pos.y - v0.pos.y)*(px - v0.pos.x);
                float w1 = (v2.pos.x - v1.pos.x)*(py - v1.pos.y) - (v2.pos.y - v1.pos.y)*(px - v1.pos.x);
                float w2 = (v0.pos.x - v2.pos.x)*(py - v2.pos.y) - (v0.pos.y - v2.pos.y)*(px - v2.pos.x);
                if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                    w0 /= area; w1 /= area; w2 /= area;
                    float r = ((v0.col >> IM_COL32_R_SHIFT) & 0xFF) * w0 +
                              ((v1.col >> IM_COL32_R_SHIFT) & 0xFF) * w1 +
                              ((v2.col >> IM_COL32_R_SHIFT) & 0xFF) * w2;
                    float g = ((v0.col >> IM_COL32_G_SHIFT) & 0xFF) * w0 +
                              ((v1.col >> IM_COL32_G_SHIFT) & 0xFF) * w1 +
                              ((v2.col >> IM_COL32_G_SHIFT) & 0xFF) * w2;
                    float b = ((v0.col >> IM_COL32_B_SHIFT) & 0xFF) * w0 +
                              ((v1.col >> IM_COL32_B_SHIFT) & 0xFF) * w1 +
                              ((v2.col >> IM_COL32_B_SHIFT) & 0xFF) * w2;
                    float a = ((v0.col >> IM_COL32_A_SHIFT) & 0xFF) * w0 +
                              ((v1.col >> IM_COL32_A_SHIFT) & 0xFF) * w1 +
                              ((v2.col >> IM_COL32_A_SHIFT) & 0xFF) * w2;
                    unsigned char* p = &pixels[4*(y*width + x)];
                    float alpha = a / 255.0f;
                    p[0] = (unsigned char)(r*alpha + p[0]*(1-alpha));
                    p[1] = (unsigned char)(g*alpha + p[1]*(1-alpha));
                    p[2] = (unsigned char)(b*alpha + p[2]*(1-alpha));
                    p[3] = 255;
                }
            }
        }
    }
};

TEST(ChartRender, MatchesGoldenImage) {
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(256,256);
    io.DeltaTime = 1.0f/60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* fp; int fw, fh;
    io.Fonts->GetTexDataAsRGBA32(&fp, &fw, &fh);
    ImGui::NewFrame();
    if (ImGui::Begin("Chart")) {
        if (ImPlot::BeginPlot("Line")) {
            double xs[2] = {0.0, 1.0};
            double ys[2] = {0.0, 1.0};
            ImPlot::PlotLine("line", xs, ys, 2);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
    ImGui::Render();
    SoftwareRenderer renderer((int)io.DisplaySize.x, (int)io.DisplaySize.y);
    renderer.render(ImGui::GetDrawData());
    auto tmp_path = std::filesystem::temp_directory_path() / "chart_output.png";
    stbi_write_png(tmp_path.string().c_str(), renderer.width, renderer.height, 4, renderer.pixels.data(), renderer.width*4);
    int gw, gh, gc;
    unsigned char* golden = stbi_load_from_memory(chart_png, chart_png_len, &gw, &gh, &gc, 4);
    ASSERT_TRUE(golden != nullptr);
    EXPECT_EQ(gw, renderer.width);
    EXPECT_EQ(gh, renderer.height);
    double diff = 0.0;
    for (int i = 0; i < renderer.width*renderer.height*4; ++i) {
        diff += std::abs((int)renderer.pixels[i] - (int)golden[i]);
    }
    diff /= (renderer.width*renderer.height*4);
    stbi_image_free(golden);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    EXPECT_LT(diff, 1.0);
}

