#include <Geode/Geode.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include <filesystem>

struct TimingPoint {
    int time;
    double beatLength;
    int meter;
    int sampleSet;
    int sampleIndex;
    int volume;
    bool uninherited;
    int effects;
};

struct ClickTime {
    double seconds;
    std::string type;
};

class OsuParser {
private:
    std::vector<TimingPoint> timingPoints;
    std::vector<ClickTime> clickTimes;
    
    void parseTimingPoints(const std::string& line) {
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> parts;
        
        while (std::getline(ss, token, ',')) {
            parts.push_back(token);
        }
        
        if (parts.size() >= 8) {
            TimingPoint tp;
            
            auto timeResult = geode::utils::numFromString<int>(parts[0]);
            if (timeResult.isErr()) {
                geode::log::error("Invalid timing point time: {}", line);
                return;
            }
            tp.time = timeResult.unwrap();
            
            auto beatLengthResult = geode::utils::numFromString<double>(parts[1]);
            if (beatLengthResult.isErr()) {
                geode::log::error("Invalid timing point beat length: {}", line);
                return;
            }
            tp.beatLength = beatLengthResult.unwrap();
            
            auto meterResult = geode::utils::numFromString<int>(parts[2]);
            if (meterResult.isErr()) {
                geode::log::error("Invalid timing point meter: {}", line);
                return;
            }
            tp.meter = meterResult.unwrap();
            
            auto sampleSetResult = geode::utils::numFromString<int>(parts[3]);
            if (sampleSetResult.isErr()) {
                geode::log::error("Invalid timing point sample set: {}", line);
                return;
            }
            tp.sampleSet = sampleSetResult.unwrap();
            
            auto sampleIndexResult = geode::utils::numFromString<int>(parts[4]);
            if (sampleIndexResult.isErr()) {
                geode::log::error("Invalid timing point sample index: {}", line);
                return;
            }
            tp.sampleIndex = sampleIndexResult.unwrap();
            
            auto volumeResult = geode::utils::numFromString<int>(parts[5]);
            if (volumeResult.isErr()) {
                geode::log::error("Invalid timing point volume: {}", line);
                return;
            }
            tp.volume = volumeResult.unwrap();
            
            auto uninheritedResult = geode::utils::numFromString<int>(parts[6]);
            if (uninheritedResult.isErr()) {
                geode::log::error("Invalid timing point uninherited: {}", line);
                return;
            }
            tp.uninherited = (uninheritedResult.unwrap() == 1);
            
            auto effectsResult = geode::utils::numFromString<int>(parts[7]);
            if (effectsResult.isErr()) {
                geode::log::error("Invalid timing point effects: {}", line);
                return;
            }
            tp.effects = effectsResult.unwrap();
            
            timingPoints.push_back(tp);
        }
    }
    
    double getCurrentBeatLength(int time) {
        double currentBeatLength = 500;
        
        for (const auto& tp : timingPoints) {
            if (tp.time <= time) {
                if (tp.uninherited) {
                    currentBeatLength = tp.beatLength;
                }
            } else {
                break;
            }
        }
        
        return currentBeatLength;
    }
    
    void parseHitObject(const std::string& line) {
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> parts;
        
        while (std::getline(ss, token, ',')) {
            parts.push_back(token);
        }
        
        if (parts.size() < 4) return;
        
        auto xResult = geode::utils::numFromString<int>(parts[0]);
        if (xResult.isErr()) {
            geode::log::error("Invalid hit object x coordinate: {}", line);
            return;
        }
        int x = xResult.unwrap();
        
        auto yResult = geode::utils::numFromString<int>(parts[1]);
        if (yResult.isErr()) {
            geode::log::error("Invalid hit object y coordinate: {}", line);
            return;
        }
        int y = yResult.unwrap();
        
        auto timeResult = geode::utils::numFromString<int>(parts[2]);
        if (timeResult.isErr()) {
            geode::log::error("Invalid hit object time: {}", line);
            return;
        }
        int time = timeResult.unwrap();
        
        auto typeResult = geode::utils::numFromString<int>(parts[3]);
        if (typeResult.isErr()) {
            geode::log::error("Invalid hit object type: {}", line);
            return;
        }
        int type = typeResult.unwrap();
        
        double seconds = time / 1000.0;
        
        if (type & 1) { // Hit Circle
            clickTimes.push_back({seconds, "HitCircle"});
        }
        else if (type & 2) { // Slider
            clickTimes.push_back({seconds, "Slider_Start"});
            
            if (parts.size() >= 8) {
                std::string sliderPath = parts[5];
                
                auto repeatsResult = geode::utils::numFromString<int>(parts[6]);
                if (repeatsResult.isErr()) {
                    geode::log::error("Invalid slider repeats: {}", line);
                    return;
                }
                int repeats = repeatsResult.unwrap();
                
                auto pixelLengthResult = geode::utils::numFromString<double>(parts[7]);
                if (pixelLengthResult.isErr()) {
                    geode::log::error("Invalid slider pixel length: {}", line);
                    return;
                }
                double pixelLength = pixelLengthResult.unwrap();
                
                double beatLength = getCurrentBeatLength(time);
                double sliderMultiplier = 1.4;
                
                double sliderDuration = (pixelLength / (100.0 * sliderMultiplier)) * beatLength;
                double totalDuration = sliderDuration * repeats;
                
                double endTime = seconds + (totalDuration / 1000.0);
                clickTimes.push_back({endTime, "Slider_End"});
                
                for (int i = 1; i < repeats; i++) {
                    double repeatTime = seconds + (sliderDuration * i / 1000.0);
                    clickTimes.push_back({repeatTime, "Slider_Repeat"});
                }
            }
        }
        else if (type & 8) { // Spinner
            clickTimes.push_back({seconds, "Spinner_Start"});
            
            if (parts.size() >= 6) {
                auto endTimeResult = geode::utils::numFromString<int>(parts[5]);
                if (endTimeResult.isErr()) {
                    geode::log::error("Invalid spinner end time: {}", line);
                    return;
                }
                int endTime = endTimeResult.unwrap();
                double endSeconds = endTime / 1000.0;
                clickTimes.push_back({endSeconds, "Spinner_End"});
            }
        }
        else if (type & 128) { // Hold note
            clickTimes.push_back({seconds, "Hold_Start"});
            
            if (parts.size() >= 6) {
                std::string endTimeStr = parts[5];
                size_t colonPos = endTimeStr.find(':');
                if (colonPos != std::string::npos) {
                    auto endTimeResult = geode::utils::numFromString<int>(endTimeStr.substr(0, colonPos));
                    if (endTimeResult.isErr()) {
                        geode::log::error("Invalid hold note end time: {}", line);
                        return;
                    }
                    int endTime = endTimeResult.unwrap();
                    double endSeconds = endTime / 1000.0;
                    clickTimes.push_back({endSeconds, "Hold_End"});
                }
            }
        }
    }
    
    bool isValidOsuFile(const std::filesystem::path& filepath) {
        if (filepath.extension() != ".osu") {
            return false;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        std::string firstLine;
        if (!std::getline(file, firstLine)) {
            return false;
        }
        
        if (firstLine.find("osu file format") == std::string::npos) {
            return false;
        }
        
        file.close();
        return true;
    }

public:
    bool parseFile(const std::filesystem::path& filepath) {
        if (!isValidOsuFile(filepath)) {
            geode::log::error("Invalid .osu file: {}", filepath.string());
            return false;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            geode::log::error("Failed to open file: {}", filepath.string());
            return false;
        }
        
        timingPoints.clear();
        clickTimes.clear();
        
        std::string line;
        std::string currentSection = "";
        
        while (std::getline(file, line)) {
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty() || line[0] == '/' || line[0] == '_') {
                continue;
            }
            
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }
            
            if (currentSection == "TimingPoints") {
                parseTimingPoints(line);
            }
            else if (currentSection == "HitObjects") {
                parseHitObject(line);
            }
        }
        
        file.close();
        
        std::sort(clickTimes.begin(), clickTimes.end(), 
                  [](const ClickTime& a, const ClickTime& b) {
                      return a.seconds < b.seconds;
                  });
        
        return true;
    }
    
    void printClickTimes() const {
        geode::log::debug("Click times (in seconds):");
        geode::log::debug("========================");
        
        for (const auto& click : clickTimes) {
            geode::log::debug("{:.3f}s - {}", click.seconds, click.type);
        }
        
        geode::log::debug("Total events: {}", clickTimes.size());
    }
    
    std::vector<double> getClickTimesOnly() const {
        std::vector<double> times;
        for (const auto& click : clickTimes) {
            times.push_back(click.seconds);
        }
        return times;
    }
    
    void printSimpleClickTimes() const {
        geode::log::debug("Click times:");
        for (const auto& click : clickTimes) {
            geode::log::debug("{:.3f}", click.seconds);
        }
    }
    
    std::string generateGuidelinesString(double msOffset = 0.0) const {
        std::stringstream ss;
        
        for (size_t i = 0; i < clickTimes.size(); ++i) {
            double milliseconds = (clickTimes[i].seconds * 1000.0) + msOffset;
            
            if (milliseconds < 0.0) milliseconds = 0.0;

            ss << std::fixed << std::setprecision(6) << milliseconds / 1000.0;
            ss << "~0";

            if (i < clickTimes.size() - 1) {
                ss << "~";
            }
        }

        ss << "~";
        return ss.str();
    }
    
    void printGuidelinesString() const {
        geode::log::debug("Guidelines string:");
        geode::log::debug("{}", generateGuidelinesString());
    }
};

// int main() {
//     OsuParser parser;
    
//     std::string filepathStr;
//     std::cout << "Enter path to .osu file: ";
//     std::getline(std::cin, filepathStr);
    
//     std::filesystem::path filepath(filepathStr);
    
//     if (parser.parseFile(filepath)) {
//         std::cout << "\nFile processed successfully!\n\n";
        
//         parser.printClickTimes();
        
//         std::cout << "\n" << std::string(50, '=') << "\n";
//         std::cout << "Simple times (for copying):\n";
//         std::cout << std::string(50, '=') << "\n";
        
//         parser.printSimpleClickTimes();
        
//         std::cout << "\n" << std::string(50, '=') << "\n";
//         std::cout << "Formatted string:\n";
//         std::cout << std::string(50, '=') << "\n";
        
//         parser.printGuidelinesString();
        
//     } else {
//         std::cout << "Error processing file!" << std::endl;
//         return 1;
//     }
    
//     return 0;
// }