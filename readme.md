# WebRemoteImgui

A lightweight, high-performance C++ library that brings **Dear ImGui** to the web. It executes all application logic on the server and streams the UI to any modern web browser using WebGL, while piping user interactions back in real-time.

Features **per-user sessions** and seamless **session restoration**, making it ideal for remote monitoring, headless server dashboards, and cloud-based C++ tools.

## Features

* **Server-Side Execution**: Your C++ code runs entirely on the server. No need to compile complex logic into WebAssembly or modify your existing backend architecture.
* **WebGL Rendering**: UI draw data is efficiently sent to the browser and rendered natively using WebGL, ensuring smooth visuals and crisp text.
* **Bi-directional Streaming**: Low-latency replication of mouse, keyboard, and touch inputs from the browser back to the server.
* **Per-User Isolation (Multi-session)**: Each connected browser gets its own independent ImGui context and state.
* **Session Restoration**: Disconnected? No problem. If a user loses connection or refreshes the page, they can resume exactly where they left off without losing their current UI state.
* **Headless Friendly**: Does not require a GPU or display server (like X11 or Wayland) on the host machine.
* **Bi-directional File Transfer**:
  * **File Downloads**: Trigger a native browser download to save files from the server onto the client's machine.
  * **File Uploads**: Open a browser file dialog to upload files directly from the client's machine to the server.
* **Full Clipboard Sync**: Seamless support for **Ctrl+C / Ctrl+V** (Copy & Paste). Copy text from the remote browser interface to your local machine and vice-versa.


## How It Works

1. **Backend**: Your C++ server-side application processes logic and generates ImGui draw commands (`ImDrawData`).
2. **Network**: The draw data is compressed and sent over WebSockets to the web client.
3. **Frontend**: A JavaScript frontend receives the data and renders it via WebGL.
4. **Interaction**: Input events (clicks, scrolls, keystrokes) are captured in the browser and instantly sent back to advance the ImGui state on the server.

API & Usage Example

The library manages connection routing internally. You only need to define your UI layout inside the `ClientSession` lifecycle:

```cpp
#include "client_session.h"
#include <server.h>

void ClientSession::DrawGUI(void* ws) {
    if (!logged_in) {
        ImGui::Begin("Login Window");
        ImGui::Text("Enter password:");
        ImGui::InputText("##Password", &password);
        
        if (ImGui::Button("Login") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (password == "12345") logged_in = true;
        }
        ImGui::End();
        return;
    }

    ImGui::Begin("Main Control Panel");
    ImGui::Text("Session ID: %s", session_id.c_str());
    ImGui::SliderFloat("Target Value", &sliderValue, 0.0f, 1.0f);

    // Dynamic file transfers initiated by the server
    if (ImGui::Button("Download Report")) {
        download_file_to_client("CMakeCache.txt");
    }
    ImGui::SameLine();
    if (ImGui::Button("Upload Config")) {
        trigger_file_upload_dialog();
    }

    // Directing client browser behavior
    if (ImGui::Button("Documentation")) {
        open_client_url("https://github.com/valentineshilov");
    }

    // Rendering server-pushed textures (ID 2 mapping)
    ImGui::Image((ImTextureID)(uintptr_t)2, { 64 , 64 });
    ImGui::End();
}
```

## Core Architecture & Network Flow

1. **Context Management**: On connection, `ClientSession::DrawGuiFull` binds the user's specific context (`ImGui::SetCurrentContext`), advances the frame, and safely dispatches the state.
2. **Texture Bridging**: Custom assets are registered via `ImServer::send_texture_to_client`, translating raw byte arrays (RGBA) straight into client WebGL textures.
3. **Event Pipeline**: Input telemetry (mouse, keys, window resizes) is mapped directly into the server's `ImGuiIO` layer before rendering.

## Building
```
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
## Screenshots:
Example login window with custom styles
<img width="1299" height="1032" alt="remote_imgui_example1" src="https://github.com/user-attachments/assets/753f9047-029d-4a00-8613-6413dcbe9a05" />
Example window with custom texture:
<img width="1299" height="1032" alt="example_image2" src="https://github.com/user-attachments/assets/bf0d9bc9-5e54-4671-865d-f9ecdee2fd90" />
