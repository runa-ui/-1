// Network Systems 08: HTTPS GET with libcurl and a small communication log.
// This sample shows the complete JSON response without using a JSON library.
// macOS/Linux/WSL, C++17. Credit: weather data from Open-Meteo (CC BY 4.0).
#include <curl/curl.h>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// Called by libcurl whenever a piece of the HTTP response body arrives.
std::size_t receiveBody(char* data, std::size_t size, std::size_t count, void* user) {
    const std::size_t bytes = size * count;
    auto* body = static_cast<std::string*>(user);
    body->append(data, bytes);
    return bytes; // Tell libcurl that all bytes were consumed.
}

// Parse user-supplied coordinates without silently accepting extra characters.
bool coordinate(const std::string& text, double minimum, double maximum, double& result) {
    try {
        std::size_t used = 0;
        result = std::stod(text, &used);
        return used == text.size() && std::isfinite(result) &&
               minimum <= result && result <= maximum;
    } catch (...) {
        return false;
    }
}


// Read the student's location from a simple text file (one "latitude longitude" line).
// Blank lines and lines beginning with # are comments.
bool loadLocation(const std::string& path, double& latitude, double& longitude) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Cannot open " << path << ". Run from the network08 folder.\n";
        return false;
    }
    std::string line;
    bool found = false;
    while (std::getline(file, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;

        std::istringstream input(line);
        std::string lat, lon, extra;
        if (found || !(input >> lat >> lon) || (input >> extra) ||
            !coordinate(lat, -90, 90, latitude) ||
            !coordinate(lon, -180, 180, longitude)) {
            std::cerr << "Invalid " << path
                      << ": use one line containing latitude longitude.\n";
            return false;
        }
        found = true;
    }
    if (!found || file.bad()) {
        std::cerr << "No valid location in " << path << ".\n";
        return false;
    }
    return true;
}

std::string utcTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

bool appendLog(double latitude, double longitude, int curlCode,
               long httpStatus, long long elapsedMs, std::size_t bodyBytes) {
    std::ofstream file("weather-log.csv", std::ios::app);
    if (!file) return false;
    if (file.tellp() == 0)
        file << "time_utc,latitude,longitude,curl_code,http_status,elapsed_ms,body_bytes\n";
    file << utcTimestamp() << ',' << std::fixed << std::setprecision(4)
         << latitude << ',' << longitude << ',' << curlCode << ','
         << httpStatus << ',' << elapsedMs << ',' << bodyBytes << '\n';
    return static_cast<bool>(file);
}


// Extract a named JSON object (for example "current" or "current_units").
// This small parser is sufficient for the flat objects used by this API response.
bool jsonObject(const std::string& json, const std::string& name, std::string& object) {
    const std::string key = "\"" + name + "\"";
    const auto keyPos = json.find(key);
    if (keyPos == std::string::npos) return false;
    const auto open = json.find('{', keyPos + key.size());
    if (open == std::string::npos) return false;

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t i = open; i < json.size(); ++i) {
        const char ch = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '{') ++depth;
        else if (ch == '}' && --depth == 0) {
            object = json.substr(open + 1, i - open - 1);
            return true;
        }
    }
    return false;
}

bool jsonString(const std::string& object, const std::string& name, std::string& value) {
    const std::string key = "\"" + name + "\"";
    const auto keyPos = object.find(key);
    if (keyPos == std::string::npos) return false;
    const auto colon = object.find(':', keyPos + key.size());
    if (colon == std::string::npos) return false;
    const auto first = object.find('"', colon + 1);
    if (first == std::string::npos) return false;

    value.clear();
    bool escaped = false;
    for (std::size_t i = first + 1; i < object.size(); ++i) {
        const char ch = object[i];
        if (escaped) {
            if (ch == 'n') value += '\n';
            else if (ch == 'r') value += '\r';
            else if (ch == 't') value += '\t';
            else value += ch;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            return true;
        } else {
            value += ch;
        }
    }
    return false;
}

bool jsonNumber(const std::string& object, const std::string& name, double& value) {
    const std::string key = "\"" + name + "\"";
    const auto keyPos = object.find(key);
    if (keyPos == std::string::npos) return false;
    const auto colon = object.find(':', keyPos + key.size());
    if (colon == std::string::npos) return false;
    const auto first = object.find_first_not_of(" \t\r\n", colon + 1);
    if (first == std::string::npos) return false;
    try {
        std::size_t used = 0;
        value = std::stod(object.substr(first), &used);
        return used > 0 && std::isfinite(value);
    } catch (...) {
        return false;
    }
}

std::string weatherDescription(int code) {
    switch (code) {
        case 0: return "快晴";
        case 1: return "主に晴れ";
        case 2: return "一部曇り";
        case 3: return "曇り";
        case 45: case 48: return "霧";
        case 51: return "弱い霧雨";
        case 53: return "霧雨";
        case 55: return "強い霧雨";
        case 56: case 57: return "着氷性の霧雨";
        case 61: return "弱い雨";
        case 63: return "雨";
        case 65: return "強い雨";
        case 66: case 67: return "着氷性の雨";
        case 71: return "弱い雪";
        case 73: return "雪";
        case 75: return "強い雪";
        case 77: return "霧雪";
        case 80: return "弱いにわか雨";
        case 81: return "にわか雨";
        case 82: return "強いにわか雨";
        case 85: return "弱いにわか雪";
        case 86: return "強いにわか雪";
        case 95: return "雷雨";
        case 96: case 99: return "ひょうを伴う雷雨";
        default: return "不明";
    }
}

void displayWeather(const std::string& json, double latitude, double longitude) {
    std::string current, units;
    if (!jsonObject(json, "current", current) ||
        !jsonObject(json, "current_units", units)) {
        std::cout << "\n天気情報をJSONから読み取れませんでした。\n";
        return;
    }

    std::string time, temperatureUnit, humidityUnit;
    double temperature = 0.0, humidity = 0.0, codeValue = 0.0;
    if (!jsonString(current, "time", time) ||
        !jsonNumber(current, "temperature_2m", temperature) ||
        !jsonNumber(current, "relative_humidity_2m", humidity) ||
        !jsonNumber(current, "weather_code", codeValue) ||
        !jsonString(units, "temperature_2m", temperatureUnit) ||
        !jsonString(units, "relative_humidity_2m", humidityUnit)) {
        std::cout << "\n天気情報の必要な項目が不足しています。\n";
        return;
    }

    const int weatherCode = static_cast<int>(codeValue);
    std::cout << "\n========== 現在の天気 ==========\n"
              << "取得地点：緯度 " << latitude << " / 経度 " << longitude << '\n'
              << "時刻：" << time << '\n'
              << "天気：" << weatherDescription(weatherCode)
              << "（コード " << weatherCode << "）\n"
              << std::fixed << std::setprecision(1)
              << "気温：" << temperature << temperatureUnit << '\n'
              << "湿度：" << std::setprecision(0) << humidity << humidityUnit << '\n'
              << "================================\n";
}

int main(int argc, char* argv[]) {
    double latitude = 0.0;
    double longitude = 0.0;
    if (argc == 1) {
        // Students change location.txt once, without editing or recompiling C++.
        if (!loadLocation("location.txt", latitude, longitude)) return 1;
    } else if (argc == 3) {
        // Optional: override the file for this one run.
        if (!coordinate(argv[1], -90, 90, latitude) ||
            !coordinate(argv[2], -180, 180, longitude)) {
            std::cerr << "Invalid latitude or longitude.\n";
            return 1;
        }
    } else {
        std::cerr << "Usage: ./weather-client [latitude longitude]\n";
        return 1;
    }

    std::ostringstream url;
    url << std::fixed << std::setprecision(4)
        << "https://api.open-meteo.com/v1/forecast?latitude=" << latitude
        << "&longitude=" << longitude
        << "&current=temperature_2m,relative_humidity_2m,weather_code"
        << "&timezone=Asia%2FTokyo";

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "curl_global_init failed.\n";
        return 1;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "curl_easy_init failed.\n";
        curl_global_cleanup();
        return 1;
    }

    std::string body;
    const std::string requestUrl = url.str();
    curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // Seconds to connect.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);       // Total seconds.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "network-systems-08/1.0");

    std::cout << "GET " << requestUrl << '\n';
    const auto start = std::chrono::steady_clock::now();
    const CURLcode result = curl_easy_perform(curl);
    const auto finish = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        finish - start).count();

    long httpStatus = 0; // Stays 0 if no HTTP response was obtained.
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    std::cout << "curl=" << static_cast<int>(result)
              << " HTTP=" << httpStatus << " elapsed=" << elapsed << "ms\n";
    if (result != CURLE_OK)
        std::cerr << "Network error: " << curl_easy_strerror(result) << '\n';
    if (!body.empty()) {
        std::cout << "\n--- JSON response ---\n" << body << '\n';
        if (result == CURLE_OK && httpStatus >= 200 && httpStatus < 300)
            displayWeather(body, latitude, longitude);
    }

    if (!appendLog(latitude, longitude, static_cast<int>(result),
                   httpStatus, elapsed, body.size()))
        std::cerr << "Warning: could not write weather-log.csv\n";

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    // HTTP errors are not automatically reported as libcurl transport errors.
    return (result == CURLE_OK && httpStatus >= 200 && httpStatus < 300) ? 0 : 1;
}
