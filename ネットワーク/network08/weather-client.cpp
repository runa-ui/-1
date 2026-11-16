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
    if (!body.empty()) std::cout << body << '\n';

    if (!appendLog(latitude, longitude, static_cast<int>(result),
                   httpStatus, elapsed, body.size()))
        std::cerr << "Warning: could not write weather-log.csv\n";

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    // HTTP errors are not automatically reported as libcurl transport errors.
    return (result == CURLE_OK && httpStatus >= 200 && httpStatus < 300) ? 0 : 1;
}
