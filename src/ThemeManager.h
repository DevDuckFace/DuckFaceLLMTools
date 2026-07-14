#pragma once
#include "imgui.h"

// Tema visual do app. Alem das cores, ajusta espacamentos e arredondamentos
// para um visual mais moderno e "respirado" que o padrao do ImGui.
class ThemeManager {
public:
    static void ApplyCommonStyle() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 8.0f;
        s.ChildRounding     = 8.0f;
        s.FrameRounding     = 6.0f;
        s.PopupRounding     = 6.0f;
        s.GrabRounding      = 6.0f;
        s.TabRounding       = 6.0f;
        s.ScrollbarRounding = 8.0f;
        s.ScrollbarSize     = 12.0f;
        s.FramePadding      = ImVec2(8, 5);
        s.ItemSpacing       = ImVec2(8, 7);
        s.ItemInnerSpacing  = ImVec2(6, 5);
        s.WindowPadding     = ImVec2(12, 10);
        s.CellPadding       = ImVec2(6, 4);
        s.GrabMinSize       = 12.0f;
        s.FrameBorderSize   = 0.0f;
        s.WindowBorderSize  = 0.0f;
        s.TabBarBorderSize  = 1.0f;
    }

    static void ApplyDark() {
        ImGui::StyleColorsDark();
        ApplyCommonStyle();
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        c[ImGuiCol_ChildBg]            = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
        c[ImGuiCol_PopupBg]            = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);
        c[ImGuiCol_FrameBg]            = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        c[ImGuiCol_FrameBgHovered]     = ImVec4(0.24f, 0.24f, 0.27f, 1.00f);
        c[ImGuiCol_FrameBgActive]      = ImVec4(0.27f, 0.27f, 0.31f, 1.00f);
        c[ImGuiCol_Button]             = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
        c[ImGuiCol_ButtonHovered]      = ImVec4(0.26f, 0.55f, 0.90f, 1.00f);
        c[ImGuiCol_ButtonActive]       = ImVec4(0.16f, 0.38f, 0.70f, 1.00f);
        c[ImGuiCol_Header]             = ImVec4(0.20f, 0.45f, 0.80f, 0.55f);
        c[ImGuiCol_HeaderHovered]      = ImVec4(0.26f, 0.55f, 0.90f, 0.80f);
        c[ImGuiCol_HeaderActive]       = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
        c[ImGuiCol_Tab]                = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
        c[ImGuiCol_TabHovered]         = ImVec4(0.26f, 0.55f, 0.90f, 0.80f);
        c[ImGuiCol_TabActive]          = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
        c[ImGuiCol_TabUnfocused]       = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.35f, 0.58f, 1.00f);
        c[ImGuiCol_Text]               = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
        c[ImGuiCol_TextDisabled]       = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
        c[ImGuiCol_Border]             = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        c[ImGuiCol_ScrollbarBg]        = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
        c[ImGuiCol_ScrollbarGrab]      = ImVec4(0.32f, 0.32f, 0.37f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.42f, 0.42f, 0.48f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.50f, 0.50f, 0.58f, 1.00f);
        c[ImGuiCol_CheckMark]          = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);
        c[ImGuiCol_SliderGrab]         = ImVec4(0.30f, 0.58f, 0.95f, 1.00f);
        c[ImGuiCol_SliderGrabActive]   = ImVec4(0.40f, 0.68f, 1.00f, 1.00f);
        c[ImGuiCol_Separator]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        c[ImGuiCol_SeparatorHovered]   = ImVec4(0.30f, 0.55f, 0.90f, 0.78f);
        c[ImGuiCol_SeparatorActive]    = ImVec4(0.30f, 0.55f, 0.90f, 1.00f);
        c[ImGuiCol_TitleBgActive]      = ImVec4(0.14f, 0.24f, 0.38f, 1.00f);
    }

    static void ApplyLight() {
        ImGui::StyleColorsLight();
        ApplyCommonStyle();
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;
        // O tema light padrao do ImGui e cinza-chapado e com pouco
        // contraste nos frames; estes ajustes deixam mais proximo de um
        // app Windows moderno (fundo quase branco, azul de destaque).
        c[ImGuiCol_WindowBg]        = ImVec4(0.96f, 0.96f, 0.97f, 1.00f);
        c[ImGuiCol_ChildBg]         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        c[ImGuiCol_PopupBg]         = ImVec4(1.00f, 1.00f, 1.00f, 0.99f);
        c[ImGuiCol_FrameBg]         = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
        c[ImGuiCol_FrameBgHovered]  = ImVec4(0.84f, 0.86f, 0.90f, 1.00f);
        c[ImGuiCol_FrameBgActive]   = ImVec4(0.78f, 0.82f, 0.88f, 1.00f);
        c[ImGuiCol_Button]          = ImVec4(0.26f, 0.53f, 0.90f, 1.00f);
        c[ImGuiCol_ButtonHovered]   = ImVec4(0.32f, 0.60f, 0.96f, 1.00f);
        c[ImGuiCol_ButtonActive]    = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
        c[ImGuiCol_Text]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        c[ImGuiCol_Border]          = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
        // Botoes azuis com texto claro precisam disso para checkbox/labels
        // continuarem escuros: ImGui usa Text para ambos, entao apenas
        // garantimos contraste suficiente no azul escolhido acima.
    }
};
