// ui/diff_viewer_panel.cpp — TextDiff host (tasks 8.3, 8.4, 8.7 cue markers).
#include "ui/diff_viewer_panel.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "dtl.h"

#include "imgui.h"
#include "TextDiff.h"
#include "TextEditor.h"

#include "model/file_meta.h"

namespace diffcue::ui {

namespace {

// Map a file extension to a TextEditor language for syntax highlighting.
const TextEditor::Language* language_for_ext(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    // lowercase
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".c")
        return TextEditor::Language::Cpp();
    if (ext == ".py") return TextEditor::Language::Python();
    if (ext == ".lua") return TextEditor::Language::Lua();
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return TextEditor::Language::Glsl();
    if (ext == ".hlsl") return TextEditor::Language::Hlsl();
    if (ext == ".json") return TextEditor::Language::Json();
    if (ext == ".sql") return TextEditor::Language::Sql();
    return nullptr;
}

// A minimal Mariana-style palette derived from the dark palette with a
// blue-tinted background (task 7.5).
TextEditor::Palette mariana_palette() {
    TextEditor::Palette p = TextEditor::GetDarkPalette();
    // Background: Mariana's deep blue-grey.
    p[static_cast<size_t>(TextEditor::Color::background)] = IM_COL32(0x1B, 0x2B, 0x34, 0xFF);
    // Default text: soft white.
    p[static_cast<size_t>(TextEditor::Color::text)] = IM_COL32(0xD8, 0xDE, 0xE9, 0xFF);
    // Comments: muted teal.
    p[static_cast<size_t>(TextEditor::Color::comment)] = IM_COL32(0x60, 0x76, 0x70, 0xFF);
    // Keywords: soft yellow.
    p[static_cast<size_t>(TextEditor::Color::keyword)] = IM_COL32(0xFF, 0xEE, 0xBB, 0xFF);
    // Strings: green.
    p[static_cast<size_t>(TextEditor::Color::string)] = IM_COL32(0xA6, 0xDC, 0x91, 0xFF);
    // Numbers: orange.
    p[static_cast<size_t>(TextEditor::Color::number)] = IM_COL32(0xFF, 0xB8, 0x6B, 0xFF);
    return p;
}

}  // namespace

struct DiffViewerPanel::Impl {
    TextDiff diff;
    model::FileMeta old_meta{};
    model::FileMeta new_meta{};
    std::filesystem::path current_path;
    bool truncated = false;
    bool binary = false;
    std::string palette_name = "Dark";
    bool ignore_eol = true;
    std::string raw_old_text;
    std::string raw_new_text;
    // Changed line sections (1-based, inclusive) for the heatmap overlay.
    std::vector<std::pair<int,int>> changed_sections;
    int total_lines = 0;
    // Inline cue-add input state.
    bool cue_input_open = false;
    char cue_text_buf[2048] = {};
    char cue_line_buf[16] = {};
    int cue_input_line = 0;
    model::Side cue_input_side = model::Side::New;
    int cue_editing_index = -1;
};

DiffViewerPanel::DiffViewerPanel() : impl_(std::make_unique<Impl>()) {
    impl_->diff.SetSideBySideMode(true);
    // The TextEditor's built-in scrollbar minimap (renderSideBySideMiniMap)
    // draws diff-colored bands inside the scrollbar rect — always aligned
    // with the content, no overlay math needed. We use it instead of a
    // custom heatmap overlay.
}

DiffViewerPanel::~DiffViewerPanel() = default;

void DiffViewerPanel::set_diff(const model::FileDiff& fd, model::DiffMode mode) {
    impl_->current_path = fd.path;
    impl_->old_meta = fd.old_meta;
    impl_->new_meta = fd.new_meta;
    impl_->truncated = fd.truncated;
    impl_->binary = fd.binary;
    impl_->raw_old_text = fd.old_text;
    impl_->raw_new_text = fd.new_text;

    if (fd.binary) {
        impl_->diff.SetText("", "");
        return;
    }

    // Normalize EOL to LF when "Ignore EOL" is checked, so files that only
    // differ in line endings show as identical.
    std::string old_text = fd.old_text;
    std::string new_text = fd.new_text;
    if (impl_->ignore_eol) {
        auto normalize = [](std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\r') {
                    out += '\n';
                    if (i + 1 < s.size() && s[i + 1] == '\n') ++i;  // skip \n of \r\n
                } else {
                    out += s[i];
                }
            }
            s = out;
        };
        normalize(old_text);
        normalize(new_text);
    }

    impl_->diff.SetText(old_text, new_text);
    impl_->diff.SetSideBySideMode(mode == model::DiffMode::SideBySide);
    impl_->diff.SetLanguage(language_for_ext(fd.path));

    // Compute changed sections for the heatmap (same dtl approach as app.cpp).
    {
        auto split = [](const std::string& t) {
            std::vector<std::string> lines; std::string cur;
            for (char c : t) { if (c == '\n') { lines.push_back(cur); cur.clear(); } else cur += c; }
            if (!cur.empty()) lines.push_back(cur);
            return lines;
        };
        auto old_l = split(old_text);
        auto new_l = split(new_text);
        dtl::Diff<std::string, std::vector<std::string>> d(old_l, new_l);
        d.compose();
        auto ses = d.getSes().getSequence();
        impl_->changed_sections.clear();
        int nl = 1; int start = -1;
        for (const auto& e : ses) {
            int t = e.second.type;
            if (t == 1) { if (start < 0) start = nl; nl++; }
            else if (t == 0) { if (start >= 0) { impl_->changed_sections.push_back({start, nl-1}); start = -1; } nl++; }
            else { if (start >= 0) { impl_->changed_sections.push_back({start, nl-1}); start = -1; } }
        }
        if (start >= 0) impl_->changed_sections.push_back({start, nl-1});
        impl_->total_lines = static_cast<int>(new_l.size());
    }
}

void DiffViewerPanel::set_palette_by_name(const std::string& name) {
    impl_->palette_name = name;
    if (name == "Light") {
        impl_->diff.SetPalette(TextEditor::GetLightPalette());
    } else if (name == "Mariana") {
        impl_->diff.SetPalette(mariana_palette());
    } else {
        impl_->diff.SetPalette(TextEditor::GetDarkPalette());
    }
}

void DiffViewerPanel::set_diff_mode(model::DiffMode mode) {
    impl_->diff.SetSideBySideMode(mode == model::DiffMode::SideBySide);
}

void DiffViewerPanel::scroll_to_line(int line) {
    // ScrollToLine marks a request that Render() executes next frame.
    impl_->diff.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle);
}

bool DiffViewerPanel::ignore_eol() const { return impl_->ignore_eol; }

void DiffViewerPanel::clear() {
    impl_->current_path.clear();
    impl_->raw_old_text.clear();
    impl_->raw_new_text.clear();
    impl_->changed_sections.clear();
    impl_->total_lines = 0;
    impl_->diff.SetText("", "");
}

void DiffViewerPanel::set_ignore_eol(bool v) {
    if (impl_->ignore_eol == v) return;
    impl_->ignore_eol = v;
    // Re-diff with the new EOL setting.
    if (!impl_->current_path.empty()) {
        model::FileDiff fd;
        fd.path = impl_->current_path;
        fd.old_text = impl_->raw_old_text;
        fd.new_text = impl_->raw_new_text;
        fd.old_meta = impl_->old_meta;
        fd.new_meta = impl_->new_meta;
        fd.binary = impl_->binary;
        fd.truncated = impl_->truncated;
        set_diff(fd, impl_->diff.GetSideBySideMode()
                     ? model::DiffMode::SideBySide
                     : model::DiffMode::Inline);
    }
}

DiffViewerActions DiffViewerPanel::render(const model::CueStore& cues) {
    DiffViewerActions actions;

    ImGui::BeginChild("diff_viewer", ImVec2(0, 0), ImGuiChildFlags_Borders);

    // Header: side-by-side shows old (left) + new (right) EOL+Enc; inline mode
    // has a single pane so only the new file's metadata is shown. No file path
    // (the tree view already shows which file is selected).
    if (!impl_->current_path.empty()) {
        std::string left_str = std::string("EOL: ") +
            model::eol_label(impl_->old_meta.eol) + "  Enc: " +
            model::encoding_label(impl_->old_meta.encoding);
        std::string right_str = std::string("EOL: ") +
            model::eol_label(impl_->new_meta.eol) + "  Enc: " +
            model::encoding_label(impl_->new_meta.encoding);
        if (impl_->diff.GetSideBySideMode()) {
            float avail = ImGui::GetContentRegionAvail().x;
            float right_w = ImGui::CalcTextSize(right_str.c_str()).x;
            ImGui::TextDisabled("%s", left_str.c_str());
            ImGui::SameLine(avail - right_w);
            ImGui::TextDisabled("%s", right_str.c_str());
        } else {
            // Inline mode: single pane — show only the new file's metadata.
            ImGui::TextDisabled("%s", right_str.c_str());
        }
    }

    if (impl_->binary) {
        ImGui::Separator();
        ImGui::TextDisabled("Binary file — no text diff available.");
    } else if (impl_->truncated) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
        ImGui::TextWrapped("Diff truncated — file exceeds %d changed lines. Open in an external tool for the full view.",
                           model::kMaxChangedLines);
        ImGui::PopStyleColor();
    } else if (!impl_->current_path.empty()) {
        ImGui::Separator();

        // The sidebar lives OUTSIDE the TextDiff's BeginChild (in a left
        // gutter of this `diff_viewer` child), so it uses the outer child
        // geometry. We capture content_left / full_w here, before Render.
        float content_left = ImGui::GetCursorScreenPos().x;
        float full_w = ImGui::GetContentRegionAvail().x;

        // Render the TextDiff at full width (don't steal space for the
        // sidebar — it's drawn as an overlay so the TextDiff keeps its
        // built-in minimap / diff-heat bar).
        impl_->diff.Render("##textdiff", ImVec2(0, 0), false);

        // Geometry: origin.y/line_h come from the editor's last render
        // (post-WindowPadding, scroll-corrected). Drawing at origin.y is
        // pixel-aligned with the text the TextDiff just rendered — no
        // fractional-scroll drift, no padding correction needed.
        const ImVec2 origin = impl_->diff.GetLastRenderOrigin();
        const float  line_h = impl_->diff.GetLineHeight();
        float diff_h = ImGui::GetItemRectSize().y;
        if (diff_h <= 0) diff_h = ImGui::GetContentRegionAvail().y;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // --- Hover line highlight (drawn as a semi-transparent overlay) ---
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            float my = ImGui::GetMousePos().y;
            int hov = static_cast<int>((my - origin.y) / line_h);
            if (hov >= 0) {
                float hy = origin.y + hov * line_h;
                dl->AddRectFilled(
                    ImVec2(content_left, hy),
                    ImVec2(content_left + full_w, hy + line_h),
                    IM_COL32(255, 255, 255, 20));
            }
        }

        // --- Cue indicator sidebar (overlay on the LEFT edge) ---
        {
            float sidebar_w = 6.0f;
            ImVec2 sidebar_pos(content_left, origin.y);
            dl->AddRectFilled(sidebar_pos,
                              ImVec2(sidebar_pos.x + sidebar_w, sidebar_pos.y + diff_h),
                              IM_COL32(30, 30, 35, 160));
            for (const auto& c : cues.cues()) {
                if (c.file != impl_->current_path) continue;
                float dot_y = origin.y + (c.line - 1) * line_h + line_h * 0.5f;
                ImU32 dot_col = c.stale ? IM_COL32(120, 120, 120, 255)
                                        : IM_COL32(255, 210, 0, 255);
                dl->AddCircleFilled(ImVec2(sidebar_pos.x + sidebar_w * 0.5f, dot_y),
                                    3.0f, dot_col);
            }
        }

        // --- Right-click context menu to add a cue ---
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            float mouse_y = ImGui::GetMousePos().y;
            // Same conversion the editor uses internally in GetWordAtScreenPos
            // (TextEditor.cpp:1309-1314): origin is scroll-adjusted, so we
            // don't need firstVisibleLine. +1 to convert 0-based to 1-based.
            int clicked_line = 1 + static_cast<int>((mouse_y - origin.y) / line_h);
            if (clicked_line < 1) clicked_line = 1;
            impl_->cue_input_line = clicked_line;

            // Check if there's an existing cue at this line — pre-fill for editing.
            impl_->cue_input_side = model::Side::New;
            impl_->cue_text_buf[0] = '\0';
            impl_->cue_editing_index = -1;
            for (int i = 0; i < cues.count(); ++i) {
                const auto& c = cues.cues()[i];
                if (c.file == impl_->current_path && c.line == clicked_line) {
                    impl_->cue_editing_index = i;
                    impl_->cue_input_side = c.side;
                    std::strncpy(impl_->cue_text_buf, c.text.c_str(),
                                 sizeof(impl_->cue_text_buf) - 1);
                    impl_->cue_text_buf[sizeof(impl_->cue_text_buf) - 1] = '\0';
                    break;
                }
            }
            std::snprintf(impl_->cue_line_buf, sizeof(impl_->cue_line_buf), "%d", clicked_line);
            ImGui::OpenPopup("##cue_popup");
        }
        if (ImGui::BeginPopup("##cue_popup")) {
            const char* title = (impl_->cue_editing_index >= 0) ? "Edit cue" : "Add cue";
            ImGui::Text("%s", title);
            ImGui::SameLine();
            ImGui::TextDisabled("(file: %s)", impl_->current_path.generic_string().c_str());

            ImGui::PushItemWidth(80);
            ImGui::InputText("Line##cue_line", impl_->cue_line_buf, sizeof(impl_->cue_line_buf),
                             ImGuiInputTextFlags_CharsDecimal);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            // Clarify that this is the diff side (old/new version), not a
            // "create new cue" action — that's what the Add button below does.
            const char* side_label = (impl_->cue_input_side == model::Side::Old)
                ? "Side: old" : "Side: new";
            if (ImGui::Button(side_label)) {
                impl_->cue_input_side = (impl_->cue_input_side == model::Side::Old)
                    ? model::Side::New : model::Side::Old;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Which side of the diff this cue attaches to.\n"
                                  "Click to toggle old/new version.");
            }

            bool submitted = ImGui::InputTextMultiline("##cue_popup_text",
                                              impl_->cue_text_buf,
                                              sizeof(impl_->cue_text_buf),
                                              ImVec2(380, 100),
                                              ImGuiInputTextFlags_EnterReturnsTrue |
                                              ImGuiInputTextFlags_CtrlEnterForNewLine);
            ImGui::TextDisabled("Ctrl+Enter for new line, Enter to submit");

            if (ImGui::Button("Add") || submitted) {
                if (impl_->cue_text_buf[0] != '\0') {
                    actions.add_cue_requested = true;
                    actions.add_cue_line = std::atoi(impl_->cue_line_buf);
                    if (actions.add_cue_line < 1) actions.add_cue_line = 1;
                    actions.add_cue_side = impl_->cue_input_side;
                    actions.add_cue_text = impl_->cue_text_buf;
                    actions.edit_cue_index = impl_->cue_editing_index;
                }
                impl_->cue_text_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            // Delete button when editing an existing cue.
            if (impl_->cue_editing_index >= 0) {
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    actions.delete_cue_index = impl_->cue_editing_index;
                    impl_->cue_text_buf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                impl_->cue_text_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::EndChild();
    return actions;
}

}  // namespace diffcue::ui
