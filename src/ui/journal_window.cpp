#include "ui/journal_window.h"

#include "imgui.h"

#include <chrono>

void DrawJournalWindow(Journal::Journal& journal) {
    ImGui::Begin("Journal");
    static char j_symbol[32] = "";
    static int j_side = 0;
    static double j_price = 0.0;
    static double j_qty = 0.0;
    ImGui::InputText("Symbol", j_symbol, IM_ARRAYSIZE(j_symbol));
    ImGui::Combo("Side", &j_side, "BUY\0SELL\0");
    ImGui::InputDouble("Price", &j_price, 0, 0, "%.2f");
    ImGui::InputDouble("Quantity", &j_qty, 0, 0, "%.4f");
    if (ImGui::Button("Add Trade")) {
        Journal::Entry e;
        e.symbol = j_symbol;
        e.side = j_side == 0 ? "BUY" : "SELL";
        e.price = j_price;
        e.quantity = j_qty;
        e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        journal.add_entry(e);
        j_symbol[0] = '\0';
        j_price = 0.0;
        j_qty = 0.0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        journal.save_json("journal.json");
        journal.save_csv("journal.csv");
    }
    if (ImGui::BeginTable("JournalTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Time");
        ImGui::TableHeadersRow();
        for (const auto& e : journal.entries()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", e.symbol.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", e.side.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", e.price);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", e.quantity);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%lld", (long long)e.timestamp);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

