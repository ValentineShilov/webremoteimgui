#pragma once
#include <limits>
#include <mutex>
#include <map>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXHttpServer.h>
#include <memory> 
#include <filesystem>
#include <random>
#include <cstdint>
class GlobalState;
class ClientSession;

inline std::string generate_secure_session_id(size_t length = 64) {
    // Base62 alphabet: safe for cookies, headers, and URLs without escaping
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = sizeof(charset) - 2; // Exclude null terminator
    thread_local std::random_device rd;
    thread_local std::uniform_int_distribution<size_t> dist(0, max_index);
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[dist(rd)]);
    }
    return result;
}


class ImServer {
public:
    ImServer(int port_a, const std::string& host_a, const std::string& url_path_a,
        const std::string& index_url_path_a,
        const std::filesystem::path& index_file_path_a, GlobalState* global_state_a);
    virtual ~ImServer() {};

    static void send_texture_to_client(void* ws, uint32_t texture_id, uint32_t width, uint32_t height, const uint8_t* rgba_pixels);

    static void delete_texture_on_client(void* ws, uint32_t texture_id);
    struct DrawCmdNetwork {
        uint32_t elemCount;
        float clipX, clipY, clipZ, clipW;
        uint32_t textureId;
    };

    std::string websocket_path;
    static void terminate_connection(void* ws, const std::string& reason);

private:
    int port;
    std::string host;
    GlobalState* global_state;
    std::string index_url_path;
    std::filesystem::path index_file_path;
    std::string index_content;
    std::string websocket_url;
    std::recursive_mutex g_sessions_mutex;
    std::map<ix::WebSocket*, std::unique_ptr<ClientSession>> g_sessions;
    std::map<std::string, std::unique_ptr<ClientSession>> g_saved_sessions;
    std::shared_ptr<ix::HttpServer> g_server;
    void handle_client_input(ix::WebSocket* ws, const ix::WebSocketMessagePtr& msg);
    virtual void handle_clipboard_paste(const std::string& text);
    virtual void handle_window_resize(float width, float height);
    void HandleInput(std::unique_ptr<ClientSession>& session, ix::WebSocket* ws, const ix::WebSocketMessagePtr& msg);
    static void terminate_connection(ix::WebSocket& webSocket, const std::string& reason);
    bool init_websocket_server();
    const static size_t max_payload_in_packet_size = (std::numeric_limits<uint32_t>::max)()-128;
public:
    bool run_server();
    // C++17,  C++20
    static std::string sanitize_filename(const std::string& input_name);
    //C++20
#if __cplusplus >= 202002L || (defined(__cpp_char8_t) && __cpp_char8_t >= 201811L)
    static std::u8string sanitize_filename(const std::u8string& input_name);
#endif

private:
    static std::vector<uint8_t> base64_decode(const std::string& in);
    static std::vector<uint8_t> compress_delta(const std::vector<uint8_t>& current, const std::vector<uint8_t>& previous);
    static ImGuiKey MapKeyStringToImGuiKey(const std::string& keyStr);
    static std::string get_url_trailing_path(const std::string& url);
    static std::string get_cookie_value(const std::string& cookie_header, const std::string& cookie_name);
    const size_t MAX_SESSIONS = 100;
    const size_t MAX_SAVED_SESSIONS = 100;
    const bool CREATE_NEW_SESSION_EACH_CONNECT = false; // enable for protection againist cookie fixation 
    const size_t MAX_UPLOAD_SIZE = 512 * 1024 * 1024;   // max file upload size - should fit in RAM
    static std::string_view strip_trailing_slash(std::string_view path);
    static bool match_path(std::string_view request_path, std::string_view route_path);

};

