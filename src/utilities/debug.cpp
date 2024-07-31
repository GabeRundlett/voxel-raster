#include "debug.hpp"
#include <fmt/format.h>

#include <vector>
#include <mutex>
#include <iostream>
#include <imgui.h>

struct Console {
    char input_buffer[256]{};
    std::vector<std::string> items;
    ImGuiTextFilter filter;
    bool auto_scroll{true};
    bool scroll_to_bottom{false};
    std::shared_ptr<std::mutex> items_mtx = std::make_shared<std::mutex>();
};

auto debug_utils::create_console() -> Console * {
    auto *self = new Console{};
    clear_log(self);
    memset(self->input_buffer, 0, sizeof(self->input_buffer));
    return self;
}

void debug_utils::destroy(Console *self) {
    clear_log(self);
    delete self;
}

void debug_utils::clear_log(Console *self) {
    auto lock = std::lock_guard{*self->items_mtx};
    self->items.clear();
}

void debug_utils::add_log(Console *self, char const *str) {
    {
        auto lock = std::lock_guard{*self->items_mtx};
        self->items.push_back(str);
    }
    std::cout << str << std::endl;
}

void debug_utils::draw_imgui(Console *self, const char *title, bool *p_open) {
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, p_open)) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Close Console")) {
            *p_open = false;
        }
        ImGui::EndPopup();
    }
    if (ImGui::SmallButton("Clear")) {
        clear_log(self);
    }
    ImGui::SameLine();
    bool const copy_to_clipboard = ImGui::SmallButton("Copy");
    ImGui::Separator();
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Auto-scroll", &self->auto_scroll);
        ImGui::EndPopup();
    }
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("Options");
    }
    ImGui::SameLine();
    self->filter.Draw(R"(Filter ("incl,-excl") ("error"))", 180);
    ImGui::Separator();
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("Clear")) {
            clear_log(self);
        }
        ImGui::EndPopup();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    if (copy_to_clipboard) {
        ImGui::LogToClipboard();
    }
    {
        auto lock = std::lock_guard{*self->items_mtx};
        for (auto const &item : self->items) {
            if (!self->filter.PassFilter(item.c_str())) {
                continue;
            }
            ImVec4 color;
            bool has_color = false;
            if (strstr(item.c_str(), "[error]") != nullptr) {
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                has_color = true;
            } else if (strncmp(item.c_str(), "# ", 2) == 0) {
                color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
                has_color = true;
            }
            if (has_color) {
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            }
            ImGui::TextUnformatted(item.c_str());
            if (has_color) {
                ImGui::PopStyleColor();
            }
        }
    }
    if (copy_to_clipboard) {
        ImGui::LogFinish();
    }
    if (self->scroll_to_bottom || (self->auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
    }
    self->scroll_to_bottom = false;
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();
    bool reclaim_focus = false;
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::End();
}
