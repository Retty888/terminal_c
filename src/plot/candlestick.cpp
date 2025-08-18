#include "plot/candlestick.h"
#include "implot_internal.h"
#include <vector>

namespace Plot {

template <typename T>
static int BinarySearch(const T* arr, int l, int r, T x) {
    if (r >= l) {
        int mid = l + (r - l) / 2;
        if (arr[mid] == x)
            return mid;
        if (arr[mid] > x)
            return BinarySearch(arr, l, mid - 1, x);
        return BinarySearch(arr, mid + 1, r, x);
    }
    return -1;
}

void PlotCandlestick(const char* label_id,
                     const double* xs,
                     const double* opens,
                     const double* closes,
                     const double* lows,
                     const double* highs,
                     int count,
                     bool tooltip,
                     float width_percent,
                     ImVec4 bullCol,
                     ImVec4 bearCol) {
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    double half_width = count > 1 ? (xs[1] - xs[0]) * width_percent : width_percent;

    if (ImPlot::IsPlotHovered() && tooltip) {
        ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        mouse.x = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Day).ToDouble();
        float tool_l = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
        float tool_r = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
        float tool_t = ImPlot::GetPlotPos().y;
        float tool_b = tool_t + ImPlot::GetPlotSize().y;
        ImPlot::PushPlotClipRect();
        draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128,128,128,64));
        ImPlot::PopPlotClipRect();
        int idx = BinarySearch(xs, 0, count - 1, mouse.x);
        if (idx != -1) {
            ImGui::BeginTooltip();
            char buff[32];
            ImPlot::FormatDate(ImPlotTime::FromDouble(xs[idx]), buff, 32, ImPlotDateFmt_DayMoYr, ImPlot::GetStyle().UseISO8601);
            ImGui::Text("Day:   %s", buff);
            ImGui::Text("Open:  $%.2f", opens[idx]);
            ImGui::Text("Close: $%.2f", closes[idx]);
            ImGui::Text("Low:   $%.2f", lows[idx]);
            ImGui::Text("High:  $%.2f", highs[idx]);
            ImGui::EndTooltip();
        }
    }

    if (ImPlot::BeginItem(label_id)) {
        ImPlot::GetCurrentItem()->Color = IM_COL32(64,64,64,255);
        if (ImPlot::FitThisFrame()) {
            for (int i = 0; i < count; ++i) {
                ImPlot::FitPoint(ImPlotPoint(xs[i], lows[i]));
                ImPlot::FitPoint(ImPlotPoint(xs[i], highs[i]));
            }
        }

        std::vector<ImVec2> open_pos(count);
        std::vector<ImVec2> close_pos(count);
        std::vector<ImVec2> low_pos(count);
        std::vector<ImVec2> high_pos(count);

        for (int i = 0; i < count; ++i) {
            open_pos[i]  = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
            close_pos[i] = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);
            low_pos[i]   = ImPlot::PlotToPixels(xs[i], lows[i]);
            high_pos[i]  = ImPlot::PlotToPixels(xs[i], highs[i]);
        }

        draw_list->PrimReserve(count * 12, count * 8);
        for (int i = 0; i < count; ++i) {
            ImU32 color = ImGui::GetColorU32(opens[i] > closes[i] ? bearCol : bullCol);
            draw_list->PrimRect(ImVec2(low_pos[i].x - 0.5f, high_pos[i].y),
                                ImVec2(low_pos[i].x + 0.5f, low_pos[i].y),
                                color);
            float top = open_pos[i].y < close_pos[i].y ? open_pos[i].y : close_pos[i].y;
            float bottom = open_pos[i].y < close_pos[i].y ? close_pos[i].y : open_pos[i].y;
            draw_list->PrimRect(ImVec2(open_pos[i].x, top),
                                ImVec2(close_pos[i].x, bottom),
                                color);
        }
        ImPlot::EndItem();
    }
}

} // namespace Plot

