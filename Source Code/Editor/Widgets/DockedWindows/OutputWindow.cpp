#include "stdafx.h"

#include "Headers/OutputWindow.h"
#include "Core/Headers/StringHelper.h"
#include "Editor/Headers/Editor.h"

#include <imgui_internal.h>
#include <IconFontCppHeaders/IconsForkAwesome.h>

namespace Divide {
    constexpr U16 g_maxLogEntries = 512;

    std::atomic_size_t g_writeIndex = 0;
    std::array<Console::OutputEntry, g_maxLogEntries> g_log;

    OutputWindow::OutputWindow(Editor& parent, const Descriptor& descriptor)
        : DockedWindow(parent, descriptor),
         _inputBuf{}
    {
        memset(_inputBuf, 0, sizeof _inputBuf);

        std::atomic_init(&g_writeIndex, 0);
        _consoleCallbackIndex = Console::bindConsoleOutput([this](const Console::OutputEntry& entry) {
            PrintText(entry);
            _scrollToButtomReset = true;
        });
    }

    OutputWindow::~OutputWindow()
    {
        if (!Console::unbindConsoleOutput(_consoleCallbackIndex)) {
            DIVIDE_UNEXPECTED_CALL();
        }

        clearLog();
    }


    void OutputWindow::clearLog() {
        g_log.fill({ "", Console::EntryType::INFO });
        g_writeIndex.store(0);
        _scrollToBottom = true;
    }

    void OutputWindow::drawInternal() {
        OPTICK_EVENT();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        {
            bool tooltip = false;
            ImGui::Text(ICON_FK_SEARCH); tooltip = tooltip || ImGui::IsItemHovered();
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            _filter.Draw("##Filter", 180); tooltip = tooltip || ImGui::IsItemHovered();
            if (tooltip) {
                ImGui::SetTooltip("Search/Filter (\"incl,-excl\") (\"error\")");
            }
        }
        ImGui::PopStyleVar();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::SameLine(window->SizeFull.x - 100);
        if (ImGui::SmallButton("Clear")) {
            clearLog();
        }
        ImGui::SameLine();
        const bool copy_to_clipboard = ImGui::SmallButton("Copy");
        ImGui::Separator();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear")) {
                clearLog();
            }
            ImGui::EndPopup();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

        if (copy_to_clipboard) {
            ImGui::LogToClipboard();
        }

        static ImVec4 colours[] = {
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
        };

        const size_t start = g_writeIndex.load();

        bool typePushed = false;
        Console::EntryType previousType = Console::EntryType::INFO;
        for (U16 i = 0u; i < g_maxLogEntries;  ++i) {
            const size_t index = (start + 1 + i) % g_maxLogEntries;
            const Console::OutputEntry& message = g_log[index];
            const char* textStart = message._text.c_str();
            const char* textEnd = textStart + message._text.length();

            if (!_filter.PassFilter(textStart, textEnd)) {
                continue;
            }
            const Console::EntryType currentType = message._type;
            if (previousType != currentType) {
                if (typePushed) {
                    ImGui::PopStyleColor();
                }
                ImGui::PushStyleColor(ImGuiCol_Text, colours[to_U8(currentType)]);
                typePushed = true;
                previousType = currentType;
            }
            
            ImGui::TextUnformatted(textStart, textEnd);
        }
        if (typePushed) {
            ImGui::PopStyleColor();
        }
        if (copy_to_clipboard) {
            ImGui::LogFinish();
        }
        if (_scrollToBottom && _scrollToButtomReset) {
            ImGui::SetScrollHereY();
        }
        _scrollToButtomReset = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::Separator();

        if (ImGui::InputText("Input",
                             _inputBuf,
                             IM_ARRAYSIZE(_inputBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                             [](ImGuiInputTextCallbackData* data) noexcept {
                                const OutputWindow* console = static_cast<OutputWindow*>(data->UserData);
                                return console->TextEditCallback(data);
                             },
                             (void*)this))
        {
            char* input_end = _inputBuf + strlen(_inputBuf);
            while (input_end > _inputBuf && input_end[-1] == ' ') {
                input_end--;
            }
            *input_end = 0;

            if (_inputBuf[0]) {
                executeCommand(_inputBuf);
            }
            strcpy(_inputBuf, "");
        }
        {
            bool tooltip = false;
            ImGui::SameLine(window->SizeFull.x - 55);
            ImGui::Text(ICON_FK_ARROW_CIRCLE_DOWN); tooltip = tooltip || ImGui::IsItemHovered();
            ImGui::SameLine();
            ImGui::PushID(ICON_FK_ARROW_CIRCLE_DOWN"_ID");
            ImGui::Checkbox("", &_scrollToBottom); tooltip = tooltip || ImGui::IsItemHovered();
            ImGui::PopID();
            if (tooltip) {
                ImGui::SetTooltip("Auto-scroll to bottom");
            }
        }
        // Demonstrate keeping auto focus on the input box
        if (ImGui::IsItemHovered() || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
        }
        ImGui::PopStyleColor();
    }

    void OutputWindow::PrintText(const Console::OutputEntry& entry) {
        g_log[g_writeIndex.fetch_add(1) % g_maxLogEntries] = entry;
    }

    void OutputWindow::executeCommand(const char* command_line) {
        PrintText(
            {
                Util::StringFormat("# %s\n", command_line),
                Console::EntryType::COMMAND
            }
        );
        _scrollToButtomReset = true;
    }

    I32 OutputWindow::TextEditCallback(const ImGuiInputTextCallbackData* data) noexcept {
        switch (data->EventFlag)
        {
            case ImGuiInputTextFlags_CallbackCompletion:
            case ImGuiInputTextFlags_CallbackHistory:
            default: break;
        }

        return -1;
    }
}