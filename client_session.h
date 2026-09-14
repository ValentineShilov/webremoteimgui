#include <imgui.h>
#include <filesystem>


// Support std::u8string for C++17 and C++20
#if __cplusplus >= 202002L
using u8string_t = std::u8string;
#define U8_LITERAL(str) u8##str
#else
using u8string_t = std::string;
#define U8_LITERAL(str) str
#endif

//class to store some global state accessible from all sessions
class GlobalState {
public:
    GlobalState() = default;
    virtual ~GlobalState() = default;
};


class ClientSession {
public:
    ImGuiContext* imguiContext = nullptr;      // Private use ImGui context
    GlobalState *global_state = nullptr;
    std::vector<uint8_t> prevFramePayload;    // Prevoius frame for delta
    std::vector<uint8_t> fontTexturePayload;  // font texture payload
    bool forceFullFrame = true;               // flag used to send full frame on initial connection
    std::string session_id;
    std::chrono::steady_clock::time_point last_seen;
    
    // Data for deferred commands to dispatch outside the mutex
    bool triggerOpenUrl = false;
    bool triggerDownloadFile = false;
    std::string targetUrl = "";
    std::filesystem::path targetFilename = "";
    bool triggerFileUploadDialog = false;
    bool triggerSetCookie = false;
    std::string setCookie;

    // Constructor
    ClientSession(GlobalState* global_state_a) : global_state(global_state_a) { };
    virtual ~ClientSession() = default;

    // HELPER METHODS FOR SENDING COMMANDS TO THE CLIENT 
    void open_client_url(const std::string& url) {
        targetUrl = url;
        triggerOpenUrl = true;
    }

    void download_file_to_client(const std::filesystem::path& filename) {
        targetFilename = filename;
        triggerDownloadFile = true;
    }

    void trigger_file_upload_dialog() {
        triggerFileUploadDialog = true;
    }

    void set_cookie(const std::string &cookie) {
        setCookie = cookie;
        triggerSetCookie = true;
    }

    void handle_file_upload(
        void* ws,
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
        const std::u8string& filename,
#else
        const std::string& filename,
#endif
        const char* file_data,
        size_t file_size
    );

    virtual void DrawGuiFull(void* ws);
    virtual void DrawGUI(void* ws);
    virtual void init_fonts_callback();
    virtual void init_custom_textures_callback(void *ws);
    virtual void close_callback() {

    }

private:
    // Interface state variables
    float sliderValue = 0.0f;
    int clickCounter = 0;
    char textBuffer[256] = "";
    bool logged_in = false;
    std::string password;
    
};