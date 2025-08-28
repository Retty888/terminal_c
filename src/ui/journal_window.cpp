#include "ui/journal_window.h"

#include "imgui.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

void DrawJournalWindow(JournalService &service, bool save_csv) {
  auto &journal = service.journal();
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
    e.side = j_side == 0 ? Journal::Side::Buy : Journal::Side::Sell;
    e.price = j_price;
    e.quantity = j_qty;
    e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    journal.add_entry(e);
    j_symbol[0] = '\0';
    j_price = 0.0;
    j_qty = 0.0;
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    service.save("journal.json");
    if (save_csv) {
      journal.save_csv((service.base_dir() / "journal.csv").string());
    }
  }

  static int edit_index = -1;
  static char edit_symbol[32];
  static int edit_side = 0;
  static double edit_price = 0.0;
  static double edit_qty = 0.0;
  static char edit_time[20];

  auto format_timestamp = [](std::int64_t ms) {
    std::time_t t = ms / 1000;
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
  };

  auto parse_timestamp = [](const char *str, std::int64_t &out_ms) {
    std::tm tm{};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    if (iss.fail())
      return false;
    std::time_t t = std::mktime(&tm);
    out_ms = static_cast<std::int64_t>(t) * 1000;
    return true;
  };

  if (ImGui::BeginTable("JournalTable", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Symbol");
    ImGui::TableSetupColumn("Side");
    ImGui::TableSetupColumn("Price");
    ImGui::TableSetupColumn("Qty");
    ImGui::TableSetupColumn("Time");
    ImGui::TableSetupColumn("Actions");
    ImGui::TableHeadersRow();
    auto &entries = journal.entries();
    for (size_t i = 0; i < entries.size(); ++i) {
      auto &e = entries[i];
      ImGui::TableNextRow();
      if (edit_index == static_cast<int>(i)) {
        ImGui::TableSetColumnIndex(0);
        ImGui::InputText("##sym", edit_symbol, sizeof(edit_symbol));
        ImGui::TableSetColumnIndex(1);
        ImGui::Combo("##side", &edit_side, "BUY\0SELL\0");
        ImGui::TableSetColumnIndex(2);
        ImGui::InputDouble("##price", &edit_price, 0, 0, "%.2f");
        ImGui::TableSetColumnIndex(3);
        ImGui::InputDouble("##qty", &edit_qty, 0, 0, "%.4f");
        ImGui::TableSetColumnIndex(4);
        ImGui::InputText("##time", edit_time, sizeof(edit_time));
        ImGui::TableSetColumnIndex(5);
        if (ImGui::Button(("Save##" + std::to_string(i)).c_str())) {
          e.symbol = edit_symbol;
          e.side = edit_side == 0 ? Journal::Side::Buy : Journal::Side::Sell;
          e.price = edit_price;
          e.quantity = edit_qty;
          std::int64_t ms;
          if (parse_timestamp(edit_time, ms)) {
            e.timestamp = ms;
          }
          edit_index = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button(("Cancel##" + std::to_string(i)).c_str())) {
          edit_index = -1;
        }
      } else {
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", e.symbol.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", Journal::side_to_string(e.side));
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.2f", e.price);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.4f", e.quantity);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%s", format_timestamp(e.timestamp).c_str());
        ImGui::TableSetColumnIndex(5);
        if (ImGui::Button(("Edit##" + std::to_string(i)).c_str())) {
          edit_index = static_cast<int>(i);
          std::strncpy(edit_symbol, e.symbol.c_str(), sizeof(edit_symbol));
          edit_symbol[sizeof(edit_symbol) - 1] = '\0';
          edit_side = e.side == Journal::Side::Buy ? 0 : 1;
          edit_price = e.price;
          edit_qty = e.quantity;
          std::string ts = format_timestamp(e.timestamp);
          std::strncpy(edit_time, ts.c_str(), sizeof(edit_time));
          edit_time[sizeof(edit_time) - 1] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button(("Delete##" + std::to_string(i)).c_str())) {
          entries.erase(entries.begin() + i);
          --i;
        }
      }
    }
    ImGui::EndTable();
  }
  ImGui::End();
}
