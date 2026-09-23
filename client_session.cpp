#include "client_session.h"
#include "misc/cpp/imgui_stdlib.h"
#include <iostream>
#include <fstream>
#include <server.h>
#include <string>

//Main gui rendering method
void ClientSession::DrawGUI(void* ws) {
    //example with login window
    if (logged_in) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize({ viewport->Size.x * 0.8f, viewport->Size.y * 0.8f }, ImGuiCond_Appearing);
        ImGuiWindowFlags flags = ImGuiItemFlags_None;
         /*  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            */ 
        ImGui::Begin("Main Window", 0, flags);//, NULL, 0);// flags);
        ImGui::Text("Welcome! Your session ID: %s \nWS pointer: 0x%p",session_id.c_str(), (void*)ws);
        ImGui::SliderFloat("Your Slider", &sliderValue, 0.0f, 1.0f);

        if (ImGui::Button("Click Me")) {
            clickCounter++;
        }
        ImGui::Text("You clicked button: %d times", clickCounter);

        if (ImGui::Button("Open External Link")) {
            open_client_url("https://github.com/valentineshilov");
        }
        ImGui::SameLine();
        if (ImGui::Button("Download File From Server")) {
            download_file_to_client("CMakeCache.txt");
        }
        ImGui::SameLine();
        if (ImGui::Button("Upload File To Server")) {
            trigger_file_upload_dialog();
        }
        ImGui::TextUnformatted("Remote texture: ");
        ImGui::Image((ImTextureID)(uintptr_t)2, { 64 , 64 });
        ImGui::InputText("Your Private Text", textBuffer, IM_ARRAYSIZE(textBuffer));
        if (ImPlot::BeginPlot("Line and Scatter Plot", ImVec2(-1, 300))) {
            float x_data[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
            float y_data1[10] = { 0, 1, 4, 9, 16, 25, 36, 49, 64, 81 };
            float y_data2[10] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18 }; 
            ImPlot::SetupAxes("X Axis Label", "Y Axis Label");
            ImPlot::PlotLine("y = x^2", x_data, y_data1, 10);
            ImPlot::PlotScatter("y = 2x", x_data, y_data2, 10);
            ImPlot::EndPlot();
        }

        ImGui::Checkbox("Show demo window", &show_demo_window); 
        ImGui::End();
        if (show_demo_window) ImGui::ShowDemoWindow();
    }
    else
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400.0f, 200.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("LoginWindiw");
        ImGui::TextUnformatted("Example, password is 12345");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enter password: ");
        ImGui::SameLine();
        if (ImGui::InputText("##Password", &password)) {
           
        }
        bool enter_pressed = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter); 
        if (ImGui::Button("Login") || enter_pressed) {
            if (password == "12345") logged_in = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Exit")) {
            ImServer::terminate_connection(ws, "Closed by user");
        }
       

        ImGui::End();
    }

}


void ClientSession::DrawGuiFull(void* ws) {
    ImGui::SetCurrentContext(imguiContext);
    ImPlot::SetCurrentContext(implotContext);
    ImGui::NewFrame();
    DrawGUI(ws);
    ImGui::Render();
}

void ClientSession::init_fonts_callback()
{
    ImGuiIO& io = ImGui::GetIO();
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);

    const ImWchar* cyrillic_ranges = io.Fonts->GetGlyphRangesCyrillic();
#ifdef _WIN32
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f, nullptr, cyrillic_ranges);
#else
    ImFont* font = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 16.0f, nullptr, cyrillic_ranges);
#endif

    if (font == nullptr) {
        std::cerr << "[Session] Error loading font, using default" << std::endl;
        io.Fonts->AddFontDefault();
    }
    else {
        io.FontDefault = font;
    }
    

    ImGuiStyle& style = ImGui::GetStyle();
    // Smooth, angular edges typical for hardware tooling
    style.WindowRounding = 3.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;

    // Tight padding for high information density
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(6, 4);

    // Color definitions (RGBA format)
    ImVec4 baseOrange = ImVec4(1.00f, 0.45f, 0.00f, 1.00f); // Bright FurMark Orange
    ImVec4 hoverOrange = ImVec4(1.00f, 0.55f, 0.15f, 1.00f); // Highlighted Orange
    ImVec4 activeOrange = ImVec4(0.85f, 0.35f, 0.00f, 1.00f); // Pressed Orange
    ImVec4 hoveredOrange = ImVec4(0.95f, 0.35f, 0.00f, 1.00f); // Pressed Orange

    ImVec4 deepBlack = ImVec4(0.05f, 0.05f, 0.05f, 1.00f); // Main background
    ImVec4 darkGray = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Component background
    ImVec4 lightGray = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // Interactive elements

    // Element color mapping
    style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Window panels
    style.Colors[ImGuiCol_WindowBg] = deepBlack;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = darkGray;
    style.Colors[ImGuiCol_Border] = lightGray;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Headers & Title Bars
    style.Colors[ImGuiCol_TitleBg] = darkGray;
    style.Colors[ImGuiCol_TitleBgActive] = darkGray;
    style.Colors[ImGuiCol_TitleBgCollapsed] = deepBlack;
    style.Colors[ImGuiCol_Header] = lightGray;
    style.Colors[ImGuiCol_HeaderHovered] = baseOrange;
    style.Colors[ImGuiCol_HeaderActive] = activeOrange;

    // Standard Inputs (Checkboxes, InputText, Sliders)
    style.Colors[ImGuiCol_FrameBg] = darkGray;
    style.Colors[ImGuiCol_FrameBgHovered] = lightGray;
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    // Buttons
    style.Colors[ImGuiCol_Button] = baseOrange; // lightGray;
    style.Colors[ImGuiCol_ButtonHovered] = hoveredOrange;
    style.Colors[ImGuiCol_ButtonActive] = activeOrange;

    // Controls (Sliders, Checkmark, Tabs)
    style.Colors[ImGuiCol_CheckMark] = baseOrange;
    style.Colors[ImGuiCol_SliderGrab] = baseOrange;
    style.Colors[ImGuiCol_SliderGrabActive] = activeOrange;
    style.Colors[ImGuiCol_Tab] = darkGray;
    style.Colors[ImGuiCol_TabHovered] = hoverOrange;
    style.Colors[ImGuiCol_TabActive] = baseOrange;
    style.Colors[ImGuiCol_TabUnfocused] = darkGray;
    style.Colors[ImGuiCol_TabUnfocusedActive] = lightGray;

    // Utility (Scrollbars, Resize handles, Plots)
    style.Colors[ImGuiCol_ScrollbarBg] = deepBlack;
    style.Colors[ImGuiCol_ScrollbarGrab] = lightGray;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = baseOrange;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = activeOrange;
    style.Colors[ImGuiCol_ResizeGrip] = lightGray;
    style.Colors[ImGuiCol_ResizeGripHovered] = baseOrange;
    style.Colors[ImGuiCol_ResizeGripActive] = activeOrange;
    style.Colors[ImGuiCol_PlotLines] = baseOrange;
    style.Colors[ImGuiCol_PlotLinesHovered] = hoverOrange;
    style.Colors[ImGuiCol_PlotHistogram] = baseOrange;
    style.Colors[ImGuiCol_PlotHistogramHovered] = hoverOrange;

}

void ClientSession::init_custom_textures_callback(void *ws)
{
    // Send test texture to server, packet type 6
    uint32_t my_image_id = 2; // ID = 1 is used for font, custom texture - ID = 2
    uint32_t img_w = 64, img_h = 64;
    std::vector<uint8_t> test_pixels((size_t)img_w * img_h * 4);
    // fill array with color gradient
    for (uint32_t y = 0; y < img_h; y++) {
        for (uint32_t x = 0; x < img_w; x++) {
            size_t idx = ((size_t)y * img_w + x) * 4ull;
            test_pixels[idx + 0] = static_cast<uint8_t>(x * 255 / img_w); // R
            test_pixels[idx + 1] = static_cast<uint8_t>(y * 255 / img_h); // G
            test_pixels[idx + 2] = 128;                                   // B
            test_pixels[idx + 3] = 255;                                   // A                    
        }
    }
    ImServer::send_texture_to_client(ws, my_image_id, img_w, img_h, test_pixels.data());
}


//file upload example hanler, urly to support both c++ standards, replace with more secure variant
void ClientSession::handle_file_upload(
    void* ws,
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    const std::u8string& filename,
#else
    const std::string& filename,
#endif
    const char* file_data,
    size_t file_size
) {
    std::error_code ec;
    std::filesystem::path uploads_dir = "uploads";
    std::filesystem::create_directories(uploads_dir, ec);

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    // C++20 
    std::u8string filename_u8 = ImServer::sanitize_filename(filename);

    std::u8string prefix = u8"upload_";
    std::filesystem::path out_path = uploads_dir / std::filesystem::path(prefix + filename_u8);
#else
    // C++17 
    std::string filename_u8 = ImServer::sanitize_filename(filename);
    std::string prefix = "upload_";
    std::filesystem::path out_path = uploads_dir / std::filesystem::path(prefix + filename_u8);
#endif

    std::ofstream outFile(out_path, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(file_data, file_size);
        outFile.close();
        std::cout << "[Session 0x" << ws << "] Binary file saved: " << out_path
            << " (" << file_size << " байт)" << std::endl;
    }
    else {
        std::cerr << "[Session 0x" << ws << "] Error saving file: " << out_path << std::endl;
    }
}