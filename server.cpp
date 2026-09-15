#include "imgui.h"
#include "server.h"
#include "client_session.h"
#include "json_utf8_fix.h"
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXHttpServer.h>
#include <vector>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <index_html.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

using json = nlohmann::json;


int main() {
    GlobalState global_state;
    ImServer server(8080, "0.0.0.0", "/ws", "/", "index.html", &global_state);
    server.run_server();
    return 0;
}

ImServer::ImServer(int port_a, const std::string& host_a, const std::string& url_path_a, const std::string& index_url_path_a, const std::filesystem::path& index_file_path_a, GlobalState* global_state_a) : port(port_a),
host(host_a), websocket_url(url_path_a),
index_url_path(index_url_path_a),
index_file_path(index_file_path_a),
global_state(global_state_a) {
    if(index_file_path.u8string().length()>0){
        std::ifstream file(index_file_path, std::ios::binary);
        if (file.is_open()) {
            index_content = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        else {
            std::cerr << "[server] Can't open \"index.html\" file: " << index_file_path << std::endl;
        }
    }
#ifdef HAS_INDEX_HTML_BLOB
    if (index_content.length() == 0) {
        index_content = (char*) INDEX_HTML_CONTENT;
        std::cerr << "[server] Using build-in \"index.html\"" << std::endl;
    }
#endif
    if (index_content.length() != 0) {
        std::string target = "ws://localhost:8080";
            size_t pos = index_content.find("ws://localhost:8080");
            if (websocket_url.size() > 0) {
                websocket_path = get_url_trailing_path(websocket_url);
                    if (pos != std::string::npos) {
                        index_content.replace(pos, target.length(), websocket_url);
                    }
            }
    }
}

void ImServer::send_texture_to_client(void* ws, uint32_t texture_id, uint32_t width, uint32_t height, const uint8_t* rgba_pixels) {
    uint32_t packet_type = 6;
    size_t pixel_data_size = (size_t)((size_t)width * height) * (size_t)4;
    std::vector<uint8_t> texture_packet;
    if (pixel_data_size > max_payload_in_packet_size || pixel_data_size == 0) {
        return;
    }
    texture_packet.reserve(4 + 4 + 4 + 4 + pixel_data_size);
    texture_packet.insert(texture_packet.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
    texture_packet.insert(texture_packet.end(), reinterpret_cast<uint8_t*>(&texture_id), reinterpret_cast<uint8_t*>(&texture_id) + 4);
    texture_packet.insert(texture_packet.end(), reinterpret_cast<uint8_t*>(&width), reinterpret_cast<uint8_t*>(&width) + 4);
    texture_packet.insert(texture_packet.end(), reinterpret_cast<uint8_t*>(&height), reinterpret_cast<uint8_t*>(&height) + 4);
    texture_packet.insert(texture_packet.end(), rgba_pixels, rgba_pixels + pixel_data_size);
    ((ix::WebSocket*)ws)->sendBinary(ix::IXWebSocketSendData(reinterpret_cast<char*>(texture_packet.data()), texture_packet.size()));
}

void ImServer::delete_texture_on_client(void* ws, uint32_t texture_id) {
    uint32_t packet_type = 7;

    std::vector<uint8_t> delete_packet;
    delete_packet.reserve(4 + 4);

    delete_packet.insert(delete_packet.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
    delete_packet.insert(delete_packet.end(), reinterpret_cast<uint8_t*>(&texture_id), reinterpret_cast<uint8_t*>(&texture_id) + 4);

    ((ix::WebSocket*)ws)->sendBinary(ix::IXWebSocketSendData(reinterpret_cast<char*>(delete_packet.data()), delete_packet.size()));
}

void ImServer::handle_client_input(ix::WebSocket* ws, const ix::WebSocketMessagePtr& msg) {
    std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
    auto it = g_sessions.find(ws);
    if (it == g_sessions.end()) return;
    this->HandleInput(it->second, ws, msg);
}

void ImServer::handle_clipboard_paste(const std::string& text) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharactersUTF8(text.c_str());
}

void ImServer::handle_window_resize(float width, float height) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
}

void ImServer::HandleInput(std::unique_ptr<ClientSession>& session, ix::WebSocket* ws, const ix::WebSocketMessagePtr& msg) {
    ImGui::SetCurrentContext(session->imguiContext);

    // binary packets handling
    if (msg->binary) {
        const std::string& bytes = msg->str;
        if (bytes.size() >= 8) {
            uint32_t packet_type = 0;
            std::memcpy(&packet_type, bytes.data(), 4);

            if (packet_type == 5) { // Binary file upload from client
                uint32_t name_len = 0;
                std::memcpy(&name_len, bytes.data() + 4, 4);

                if (bytes.size() >= (size_t)8 + name_len) {
                    std::u8string filename((char8_t *) bytes.data() + 8, name_len);
                    const char* file_data = bytes.data() + 8 + name_len;
                    size_t file_size = bytes.size() - (8ull + name_len);
                    // call session method handle_file_upload
                    session->handle_file_upload(ws, filename, file_data, file_size);
                }
            }
        }
        return;
    }

    //Json packets 
    try {
        auto j = json::parse(msg->str);
        ImGuiIO& io = ImGui::GetIO();
        std::string type = j.value("type", "");

        if (j.contains("source")) {
            std::string src = j.value("source", "mouse");
            io.MouseSource = (src == "touch") ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse;
        }

        if (type == "mousemove") {
            io.AddMousePosEvent(j.value("x", 0.0f), j.value("y", 0.0f));
        }
        else if (type == "mousedown") {
            io.AddMouseButtonEvent(j.value("button", 0), true);
        }
        else if (type == "mouseup") {
            io.AddMouseButtonEvent(j.value("button", 0), false);
        }
        else if (type == "wheel") {
            io.AddMouseWheelEvent(j.value("x", 0.0f), j.value("y", 0.0f));
        }
        else if (type == "keydown" || type == "keyup") {
            ImGuiKey key = MapKeyStringToImGuiKey(j.value("keyStr", ""));
            if (key != ImGuiKey_None) io.AddKeyEvent(key, (type == "keydown"));
        }
        else if (type == "text") {
            std::string charStr = j.value("charStr", "");
            if (!charStr.empty()) io.AddInputCharactersUTF8(charStr.c_str());
        }
        else if (type == "resize") {
            handle_window_resize(j.value("w", 1280.0f), j.value("h", 720.0f));
        }
        else if (type == "clipboard_paste") {
            handle_clipboard_paste(j.value("text", ""));
        }
        else if (type == "file_upload") {
            std::u8string filename = j.value("filename", std::u8string(u8"dropped_file.bin"));
            std::string b64data = j.value("data", "");
            // decode base64
            std::vector<uint8_t> fileBytes = base64_decode(b64data);
            // call session method handle_file_upload
            session->handle_file_upload(ws, filename, reinterpret_cast<const char*>(fileBytes.data()), fileBytes.size());
        }
    }
    catch (...) {}
}

void ImServer::terminate_connection(ix::WebSocket& webSocket, const std::string& reason) {
    webSocket.stop(ix::WebSocketCloseConstants::kNormalClosureCode, reason);
}

void ImServer::terminate_connection(void* ws, const std::string& reason)
{
    ImServer::terminate_connection(*((ix::WebSocket*)ws), reason);
}

bool ImServer::init_websocket_server() {
    std::cout << "Starting server on: http://" << host << ":" << port << index_url_path <<" Websocket: " << websocket_url << " Index.html: " << index_file_path <<std::endl;
    g_server = std::make_shared<ix::HttpServer>(port, host);
    g_server->setOnConnectionCallback(
        [&](ix::HttpRequestPtr request, std::shared_ptr<ix::ConnectionState> connectionState) -> ix::HttpResponsePtr {

            // "index.html"
            if (match_path(request->uri, index_url_path)) {
                return std::make_shared<ix::HttpResponse>(
                    200, "OK", ix::HttpErrorCode::Ok, ix::WebSocketHttpHeaders(), index_content
                    );
            }
            // Handle WebSocket path
            if (match_path(request->uri, websocket_path)) {
                return std::make_shared<ix::HttpResponse>(
                    200, "OK", ix::HttpErrorCode::Invalid, ix::WebSocketHttpHeaders(), "Not a websocket connection?"
                    );
            }
            // Default 404 Not Found for unhandled paths
            return std::make_shared<ix::HttpResponse>(
                404, "Not Found", ix::HttpErrorCode::Invalid, ix::WebSocketHttpHeaders(), "Not Found"
                );
        }
    );

    g_server->setOnClientMessageCallback([&](std::shared_ptr<ix::ConnectionState> connectionState,
        ix::WebSocket& webSocket,
        const ix::WebSocketMessagePtr& msg)  {
            if (msg->type == ix::WebSocketMessageType::Open) {
                if (!match_path(msg->openInfo.uri, websocket_path)) {
                    terminate_connection(webSocket, "Invalid path");
                    return;
                }

                std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
                if (g_sessions.size() >= MAX_SESSIONS) {
                    terminate_connection(webSocket, "Too many sessions");
                    return;
                }

                IMGUI_CHECKVERSION();
                ImGuiContext* ctx = ImGui::CreateContext();
                ImPlotContext* plctx = ImPlot::CreateContext();
                ImGui::SetCurrentContext(ctx);
                
                auto headers = msg->openInfo.headers;
                std::string cookie_header = "";
                if (headers.find("Cookie") != headers.end()) {
                    cookie_header = headers["Cookie"];
                }
                else if (headers.find("cookie") != headers.end()) {
                    cookie_header = headers["cookie"];
                }

                std::unique_ptr<ClientSession> session;
                std::string session_id = generate_secure_session_id();
                if (!cookie_header.empty()) {
                    std::string session_id_cookie = get_cookie_value(cookie_header, "sid");
                    if (session_id_cookie.length() == session_id.length()) {
                        if (!CREATE_NEW_SESSION_EACH_CONNECT) {
                            session_id = session_id_cookie; //less secure - cookie fixation is possible
                                                            // but makes session more stable
                        }
                        auto node = g_saved_sessions.extract(session_id_cookie);
                        if (!node.empty()) {
                            session = std::move(node.mapped());
                        }
                    }
                }
                if(!session)
                    session = std::make_unique<ClientSession>(global_state);

                session->session_id = session_id;
                
                session->imguiContext = ctx;
                session->implotContext = plctx;

                session->init_fonts_callback();

                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.IniFilename = nullptr;

                io.SetClipboardTextFn = set_clipboard_text_from_server;
                io.ClipboardUserData = session.get();

                unsigned char* pixels; int width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

                uint32_t packet_type = 1, w = width, h = height;
                if (((size_t) width * height * 4) > max_payload_in_packet_size || ((size_t)width * height * 4) == 0) {
                    terminate_connection(webSocket, "Invalid packet size in fontTexturePayload");
                    return;
                }
                session->fontTexturePayload.insert(session->fontTexturePayload.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
                session->fontTexturePayload.insert(session->fontTexturePayload.end(), reinterpret_cast<uint8_t*>(&w), reinterpret_cast<uint8_t*>(&w) + 4);
                session->fontTexturePayload.insert(session->fontTexturePayload.end(), reinterpret_cast<uint8_t*>(&h), reinterpret_cast<uint8_t*>(&h) + 4);
                session->fontTexturePayload.insert(session->fontTexturePayload.end(), pixels, pixels + ((size_t) width * height * 4));

                io.Fonts->SetTexID((ImTextureID)(uintptr_t)1);
                session->forceFullFrame = true;

                // save session to map
                ClientSession* session_ptr = session.get();
                g_sessions[&webSocket] = std::move(session);
                std::cout << "[Session] Created context for client. Total: " << g_sessions.size() << std::endl;

                // send font texture to client, packet type 1
                webSocket.sendBinary(ix::IXWebSocketSendData(reinterpret_cast<char*>(session_ptr->fontTexturePayload.data()), session_ptr->fontTexturePayload.size()));

                // set session cookie
                {
                    json cmd = { { "type", "set_cookie" },{ "cookie", std::string("sid=") + session_ptr->session_id  } };
                    webSocket.sendText(cmd.dump());
                }

                session_ptr->init_custom_textures_callback(&webSocket);

            }
            else if (msg->type == ix::WebSocketMessageType::Close) {
                //connection closed handler, clear old sessions
                std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
                auto it = g_sessions.find(&webSocket);
                if (it != g_sessions.end()) {
                    it->second->close_callback();
                    ImPlot::DestroyContext(it->second->implotContext);
                    ImGui::DestroyContext(it->second->imguiContext);
                    std::string saved_id = it->second->session_id;
                    g_saved_sessions[saved_id] = std::move(it->second);
                    g_sessions.erase(it);
                    std::cout << "[Session] Closed. Active left: " << g_sessions.size() << ". Saved: " << g_saved_sessions.size() << std::endl;

                    if (g_saved_sessions.size() > MAX_SAVED_SESSIONS) {
                        auto oldest_it = g_saved_sessions.end();
                        auto oldest_time = (std::chrono::steady_clock::time_point::max)();
                        for (auto it_saved = g_saved_sessions.begin(); it_saved != g_saved_sessions.end(); ++it_saved) {
                            if (it_saved->second->last_seen < oldest_time) {
                                oldest_time = it_saved->second->last_seen;
                                oldest_it = it_saved;
                            }
                        }
                        if (oldest_it != g_saved_sessions.end()) {
                            std::cout << "[Cleanup] Removed expired session due to limit: "
                                << oldest_it->first << std::endl;
                            g_saved_sessions.erase(oldest_it);
                        }
                    }
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Message) {
                handle_client_input(&webSocket, msg);
            }
        });

    auto res = g_server->listen();
    if (!res.first) {
        std::cerr << "Error starting server: " << res.second << std::endl;
        return false;
    }
    g_server->start();
    return true;
}

bool ImServer::run_server()
{
    ix::initNetSystem();

    if (!init_websocket_server()) {
        std::cerr << "Multi-Session ImGui Server could not be started, exiting" << std::endl;
        return false;
    }
   
    while (true) {
        std::vector<ix::WebSocket*> active_sockets;
        {
            std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
            for (auto const& [ws, session] : g_sessions) {
                active_sockets.push_back(ws);
            }
        }

        for (ix::WebSocket* ws : active_sockets) {
            std::vector<uint8_t> raw_payload;
            bool currentForceFullFrame = false;
            std::vector<uint8_t> prevFramePayloadCopy;

            bool sendUrl = false;
            bool sendFile = false;
            bool fileUploadDialog = false;
            std::string urlToTransmit;
            std::filesystem::path fileToTransmit;

            bool setCookie = false;
            std::string cookie_to_set;
            
            bool triggerCopy = false;
            std::string clipboard_to_send;
            // Local mutex scope STRICTLY for working with ImGui and the session.
            {
                std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
                auto it = g_sessions.find(ws);
                if (it == g_sessions.end()) continue;

                ClientSession* session = it->second.get();

                // Call the interface rendering method from the session class
                session->DrawGuiFull(ws);

                // Copy the flag state to handle sending outside the lock
                sendUrl = session->triggerOpenUrl;
                urlToTransmit = session->targetUrl;
                session->triggerOpenUrl = false;

                sendFile = session->triggerDownloadFile;
                fileToTransmit = session->targetFilename;
                session->triggerDownloadFile = false;

                currentForceFullFrame = session->forceFullFrame;
                prevFramePayloadCopy = session->prevFramePayload;

                fileUploadDialog = session->triggerFileUploadDialog;
                session->triggerFileUploadDialog = false;

                setCookie = session->triggerSetCookie;
                cookie_to_set = session->setCookie;
                session->setCookie = "";
                session->triggerSetCookie = false;

                triggerCopy = session->triggerCopy;
                clipboard_to_send = session->clipboard;
                session->clipboard = "";
                session->triggerCopy = false;

                // Gathering ImGui geometry into a buffer
                ImDrawData* draw_data = ImGui::GetDrawData();
                if (!draw_data || draw_data->CmdListsCount == 0) continue;

                uint32_t total_v_count = draw_data->TotalVtxCount;
                uint32_t total_i_count = draw_data->TotalIdxCount;
                uint32_t total_cmd_count = 0;
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    total_cmd_count += draw_data->CmdLists[n]->CmdBuffer.Size;
                }

                // Dynamically determine the index size (2 for ImDrawIdx=uint16_t, 4 for uint32_t)
                uint32_t idx_type_size = static_cast<uint32_t>(sizeof(ImDrawIdx));

                // Calculate the payload_size, accounting to the index size field (an additional 4 bytes)
                size_t payload_size = sizeof(total_v_count) + sizeof(total_i_count) + sizeof(total_cmd_count) +
                    sizeof(idx_type_size) +
                    (total_v_count * sizeof(ImDrawVert)) +
                    ((size_t)total_i_count * idx_type_size) +
                    (total_cmd_count * sizeof(DrawCmdNetwork));

                if (payload_size > max_payload_in_packet_size || payload_size==0) {
                    terminate_connection(*ws, "Invalid packet size in ImDrawVert, DrawCmdNetwork");
                    continue;
                }
                raw_payload.resize(payload_size);
                uint8_t* dst = raw_payload.data();

                auto append_fast = [&](const void* src, size_t size) {
                    std::memcpy(dst, src, size);
                    dst += size;
                };

                // Geometry packet header.
                append_fast(&total_v_count, sizeof(total_v_count));
                append_fast(&total_i_count, sizeof(total_i_count));
                append_fast(&total_cmd_count, sizeof(total_cmd_count));
                append_fast(&idx_type_size, sizeof(idx_type_size)); // index type size to the client.

                //Copy vertices
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    size_t size = draw_data->CmdLists[n]->VtxBuffer.Size * sizeof(ImDrawVert);
                    append_fast(draw_data->CmdLists[n]->VtxBuffer.Data, size);
                }
                // Copy the indices (the size will be automatically adjusted to uint16_t or uint32_t).
                uint32_t current_vtx_offset = 0;
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    const ImDrawList* cmd_list = draw_data->CmdLists[n];
                    for (int m = 0; m < cmd_list->IdxBuffer.Size; ++m) {
                        ImDrawIdx rebased_idx = (ImDrawIdx)(current_vtx_offset + cmd_list->IdxBuffer.Data[m] );
                        append_fast(&rebased_idx, sizeof(ImDrawIdx));
                    }
                    current_vtx_offset += (uint32_t)(cmd_list->VtxBuffer.Size);
                }
                // Copy the rendering commands.
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    const ImDrawList* cmd_list = draw_data->CmdLists[n];
                    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                        DrawCmdNetwork net_cmd{ pcmd->ElemCount, pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z, pcmd->ClipRect.w, static_cast<uint32_t>((uintptr_t)pcmd->GetTexID()) };
                        append_fast(&net_cmd, sizeof(DrawCmdNetwork));
                    }
                }
            } // Mutex released now

            // Processing of open_url command outside the mutex
            if (sendUrl) {
                json cmd = { { "type", "open_url" }, { "url", urlToTransmit } };
                ws->sendText(cmd.dump());
            }

            if (setCookie) {
                json cmd = { { "type", "set_cookie" }, { "cookie", cookie_to_set } };
                ws->sendText(cmd.dump());
            }

            if (triggerCopy) {
                json cmd = { { "type", "clipboard_copy" }, { "text", clipboard_to_send } };
                ws->sendText(cmd.dump());
            }

            // Handling client file downloads
            if (sendFile) {
                std::ifstream file(fileToTransmit, std::ios::binary);
                if (file.is_open()) {
                    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    file.close();

                    uint32_t packet_type = 4;
                    auto filename = fileToTransmit.u8string();
                    uint32_t name_len = static_cast<uint32_t>(filename.size());

                    std::vector<uint8_t> file_packet;
                    if ((name_len + file_data.size()) > max_payload_in_packet_size || (name_len + file_data.size())==0) {
                        terminate_connection(*ws, "Invalid packet size in sendFile");
                        continue;
                    }
                    file_packet.reserve(4ull + 4ull + name_len + file_data.size());
                    file_packet.insert(file_packet.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
                    file_packet.insert(file_packet.end(), reinterpret_cast<uint8_t*>(&name_len), reinterpret_cast<uint8_t*>(&name_len) + 4);
                    file_packet.insert(file_packet.end(), filename.begin(), filename.end());
                    file_packet.insert(file_packet.end(), file_data.begin(), file_data.end());

                    ws->sendBinary(ix::IXWebSocketSendData(reinterpret_cast<char*>(file_packet.data()), file_packet.size()));
                }
            }

            if (fileUploadDialog) {
                json cmd = { { "type", "trigger_file_dialog" } };
                ws->sendText(cmd.dump());
            }

            // Construct the render frame network packet (outside the mutex)
            std::vector<uint8_t> final_network_packet;
            if (currentForceFullFrame || prevFramePayloadCopy.empty()) {
                uint32_t packet_type = 2;
                if ((raw_payload.size()) > max_payload_in_packet_size || (raw_payload.size()) == 0) {
                    terminate_connection(*ws, "Invalid packet size in sendFile");
                    continue;
                }
                final_network_packet.reserve(4 + raw_payload.size());
                final_network_packet.insert(final_network_packet.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
                final_network_packet.insert(final_network_packet.end(), raw_payload.begin(), raw_payload.end());

                std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
                auto it = g_sessions.find(ws);
                if (it != g_sessions.end()) it->second->forceFullFrame = false;
            }
            else {
                std::vector<uint8_t> delta_payload = compress_delta(raw_payload, prevFramePayloadCopy);
                uint32_t packet_type = 3;
                if ((raw_payload.size()) > max_payload_in_packet_size || (raw_payload.size()) == 0) {
                    terminate_connection(*ws, "Invalid packet size in sendFile");
                    continue;
                }
                final_network_packet.reserve(4 + delta_payload.size());
                final_network_packet.insert(final_network_packet.end(), reinterpret_cast<uint8_t*>(&packet_type), reinterpret_cast<uint8_t*>(&packet_type) + 4);
                final_network_packet.insert(final_network_packet.end(), delta_payload.begin(), delta_payload.end());
            }

            {
                std::lock_guard<std::recursive_mutex> lock(g_sessions_mutex);
                auto it = g_sessions.find(ws);
                if (it != g_sessions.end()) it->second->prevFramePayload = std::move(raw_payload);
            }

            ws->sendBinary(final_network_packet);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Гц
    }
    ix::uninitNetSystem();
}

std::vector<uint8_t> ImServer::base64_decode(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }
    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

void ImServer::set_clipboard_text_from_server(void* user_data, const char* text) {
    ClientSession* session = (ClientSession*) user_data;
    if (session && text) {
        ImGuiContext* prevContext = ImGui::GetCurrentContext();
        if (prevContext != session->imguiContext) {
            ImGui::SetCurrentContext(session->imguiContext);
        }
        session->clipboard = text; 
        session->triggerCopy = true;
        if (prevContext != session->imguiContext) {
            ImGui::SetCurrentContext(prevContext);
        }
    }
}


std::vector<uint8_t> ImServer::compress_delta(const std::vector<uint8_t>& current, const std::vector<uint8_t>& previous) {
    std::vector<uint8_t> delta;
    uint32_t curr_size = static_cast<uint32_t>(current.size());
    uint32_t prev_size = static_cast<uint32_t>(previous.size());
    delta.insert(delta.end(), reinterpret_cast<const uint8_t*>(&curr_size), reinterpret_cast<const uint8_t*>(&curr_size) + 4);

    uint32_t i = 0;
    while (i < curr_size) {
        if (i >= prev_size) {
            uint32_t remaining = curr_size - i;
            delta.push_back(0xFF);
            delta.insert(delta.end(), reinterpret_cast<const uint8_t*>(&remaining), reinterpret_cast<const uint8_t*>(&remaining) + 4);
            delta.insert(delta.end(), current.begin() + i, current.end());
            break;
        }
        if (current[i] == previous[i]) {
            uint32_t match_len = 0;
            while (i < curr_size && i < prev_size && current[i] == previous[i] && match_len < 0xFFFFFFFF) {
                match_len++; i++;
            }
            delta.push_back(0x01);
            delta.insert(delta.end(), reinterpret_cast<const uint8_t*>(&match_len), reinterpret_cast<const uint8_t*>(&match_len) + 4);
        }
        else {
            std::vector<uint8_t> diff_bytes;
            while (i < curr_size && (i >= prev_size || current[i] != previous[i]) && diff_bytes.size() < 0xFFFFFFFF) {
                diff_bytes.push_back(current[i]); i++;
            }
            delta.push_back(0x02);
            uint32_t diff_len = static_cast<uint32_t>(diff_bytes.size());
            delta.insert(delta.end(), reinterpret_cast<const uint8_t*>(&diff_len), reinterpret_cast<const uint8_t*>(&diff_len) + 4);
            delta.insert(delta.end(), diff_bytes.begin(), diff_bytes.end());
        }
    }
    return delta;
}

ImGuiKey ImServer::MapKeyStringToImGuiKey(const std::string& keyStr) {
    if (keyStr == "KeyA") return ImGuiKey_A; if (keyStr == "KeyB") return ImGuiKey_B;
    if (keyStr == "KeyC") return ImGuiKey_C; if (keyStr == "KeyD") return ImGuiKey_D;
    if (keyStr == "KeyE") return ImGuiKey_E; if (keyStr == "KeyF") return ImGuiKey_F;
    if (keyStr == "KeyG") return ImGuiKey_G; if (keyStr == "KeyH") return ImGuiKey_H;
    if (keyStr == "KeyI") return ImGuiKey_I; if (keyStr == "KeyJ") return ImGuiKey_J;
    if (keyStr == "KeyK") return ImGuiKey_K; if (keyStr == "KeyL") return ImGuiKey_L;
    if (keyStr == "KeyM") return ImGuiKey_M; if (keyStr == "KeyN") return ImGuiKey_N;
    if (keyStr == "KeyO") return ImGuiKey_O; if (keyStr == "KeyP") return ImGuiKey_P;
    if (keyStr == "KeyQ") return ImGuiKey_Q; if (keyStr == "KeyR") return ImGuiKey_R;
    if (keyStr == "KeyS") return ImGuiKey_S; if (keyStr == "KeyT") return ImGuiKey_T;
    if (keyStr == "KeyU") return ImGuiKey_U; if (keyStr == "KeyV") return ImGuiKey_V;
    if (keyStr == "KeyW") return ImGuiKey_W; if (keyStr == "KeyX") return ImGuiKey_X;
    if (keyStr == "KeyY") return ImGuiKey_Y; if (keyStr == "KeyZ") return ImGuiKey_Z;
    if (keyStr == "Digit0") return ImGuiKey_0; if (keyStr == "Digit1") return ImGuiKey_1;
    if (keyStr == "Digit2") return ImGuiKey_2; if (keyStr == "Digit3") return ImGuiKey_3;
    if (keyStr == "Digit4") return ImGuiKey_4; if (keyStr == "Digit5") return ImGuiKey_5;
    if (keyStr == "Digit6") return ImGuiKey_6; if (keyStr == "Digit7") return ImGuiKey_7;
    if (keyStr == "Digit8") return ImGuiKey_8; if (keyStr == "Digit9") return ImGuiKey_9;
    if (keyStr == "Enter") return ImGuiKey_Enter; if (keyStr == "NumpadEnter") return ImGuiKey_KeypadEnter;
    if (keyStr == "Escape") return ImGuiKey_Escape; if (keyStr == "Backspace") return ImGuiKey_Backspace;
    if (keyStr == "Tab") return ImGuiKey_Tab; if (keyStr == "Space") return ImGuiKey_Space;
    if (keyStr == "ArrowLeft") return ImGuiKey_LeftArrow; if (keyStr == "ArrowRight") return ImGuiKey_RightArrow;
    if (keyStr == "ArrowUp") return ImGuiKey_UpArrow; if (keyStr == "ArrowDown") return ImGuiKey_DownArrow;
    if (keyStr == "Delete") return ImGuiKey_Delete; if (keyStr == "Home") return ImGuiKey_Home;
    if (keyStr == "End") return ImGuiKey_End;
    if (keyStr == "ControlLeft" || keyStr == "ControlRight") return ImGuiKey_ReservedForModCtrl;
    if (keyStr == "ShiftLeft" || keyStr == "ShiftRight") return ImGuiKey_ReservedForModShift;
    if (keyStr == "AltLeft" || keyStr == "AltRight") return ImGuiKey_ReservedForModAlt;
    return ImGuiKey_None;
}

std::string ImServer::get_url_trailing_path(const std::string& url) {
    if (url.empty()) return "/";
    size_t startPos = 0;
    size_t schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        startPos = schemeEnd + 3; 
    }
    size_t pathStart = url.find('/', startPos);
    if (pathStart == std::string::npos) {
        size_t queryStart = url.find_first_of("?#", startPos);
        if (queryStart != std::string::npos) {
            return "/" + url.substr(queryStart);
        }
        return "/"; // no path
    }
    return url.substr(pathStart);
}

std::string ImServer::get_cookie_value(const std::string& cookie_header, const std::string& cookie_name) {
    std::string search_str = cookie_name + "=";
    size_t pos = cookie_header.find(search_str);
    if (pos == std::string::npos) return "";

    pos += search_str.length();
    size_t end_pos = cookie_header.find(";", pos);

    if (end_pos == std::string::npos) {
        return cookie_header.substr(pos);
    }
    return cookie_header.substr(pos, end_pos - pos);
}

// Helper function to strip the trailing slash if it exists.
std::string_view ImServer::strip_trailing_slash(std::string_view path) {
    if (path.length() > 1 && path.back() == '/') {
        return path.substr(0, path.length() - 1);
    }
    return path;
}

// Matches two paths regardless of whether they have a trailing slash or not

// Matches two paths regardless of whether they have a trailing slash or not
bool ImServer::match_path(std::string_view request_path, std::string_view route_path) {
    // Handle edge case where paths are completely empty
    if (request_path.empty() || route_path.empty()) {
        return request_path == route_path;
    }

    // Compare the paths after stripping their trailing slashes
    return strip_trailing_slash(request_path) == strip_trailing_slash(route_path);
}

// c++ 17, 20
std::string ImServer::sanitize_filename(const std::string& input_name) {
    std::filesystem::path raw_path(input_name);
    std::string filename = raw_path.filename().string();

    filename.erase(std::remove(filename.begin(), filename.end(), '/'), filename.end());
    filename.erase(std::remove(filename.begin(), filename.end(), '\\'), filename.end());

    if (filename == "." || filename == ".." || filename.empty()) {
        return "safe_default_filename";
    }
    return filename;
}

// c++ > 20
#if __cplusplus >= 202002L || (defined(__cpp_char8_t) && __cpp_char8_t >= 201811L)
std::u8string ImServer::sanitize_filename(const std::u8string& input_name) {
    std::filesystem::path raw_path(input_name);
    std::u8string filename = raw_path.filename().u8string();

    filename.erase(std::remove(filename.begin(), filename.end(), u8'/'), filename.end());
    filename.erase(std::remove(filename.begin(), filename.end(), u8'\\'), filename.end());

    if (filename == u8"." || filename == u8".." || filename.empty()) {
        return u8"safe_default_filename";
    }
    return filename;
}
#endif
