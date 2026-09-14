#pragma once
#include <nlohmann/json.hpp>
namespace nlohmann {
    template <>
    struct adl_serializer<std::u8string> {
        static void to_json(json& j, const std::u8string& opt) {
            // Превращаем u8string в обычную строку, так как json внутри хранит char
            j = std::string(reinterpret_cast<const char*>(opt.data()), opt.length());
        }

        static void from_json(const json& j, std::u8string& opt) {
            if (j.is_string()) {
                const auto& str = j.get_ref<const std::string&>();
                opt = std::u8string(reinterpret_cast<const char8_t*>(str.data()), str.length());
            }
        }
    };
}