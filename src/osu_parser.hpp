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
            tp.time = std::stoi(parts[0]);
            tp.beatLength = std::stod(parts[1]);
            tp.meter = std::stoi(parts[2]);
            tp.sampleSet = std::stoi(parts[3]);
            tp.sampleIndex = std::stoi(parts[4]);
            tp.volume = std::stoi(parts[5]);
            tp.uninherited = (std::stoi(parts[6]) == 1);
            tp.effects = std::stoi(parts[7]);
            
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
        
        int x = std::stoi(parts[0]);
        int y = std::stoi(parts[1]);
        int time = std::stoi(parts[2]);
        int type = std::stoi(parts[3]);
        
        double seconds = time / 1000.0;
        
        if (type & 1) { // Hit Circle
            clickTimes.push_back({seconds, "HitCircle"});
        }
        else if (type & 2) { // Slider
            clickTimes.push_back({seconds, "Slider_Start"});
            
            if (parts.size() >= 8) {
                std::string sliderPath = parts[5];
                int repeats = std::stoi(parts[6]);
                double pixelLength = std::stod(parts[7]);
                
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
                int endTime = std::stoi(parts[5]);
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
                    int endTime = std::stoi(endTimeStr.substr(0, colonPos));
                    double endSeconds = endTime / 1000.0;
                    clickTimes.push_back({endSeconds, "Hold_End"});
                }
            }
        }
    }
    
public:
    bool parseFile(const std::filesystem::path& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return false;
        }
        
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
        std::cout << "Click times (in seconds):\n";
        std::cout << "========================\n";
        
        for (const auto& click : clickTimes) {
            std::cout << std::fixed << std::setprecision(3) 
                      << click.seconds << "s - " << click.type << std::endl;
        }
        
        std::cout << "\nTotal events: " << clickTimes.size() << std::endl;
    }
    
    std::vector<double> getClickTimesOnly() const {
        std::vector<double> times;
        for (const auto& click : clickTimes) {
            times.push_back(click.seconds);
        }
        return times;
    }
    
    void printSimpleClickTimes() const {
        std::cout << "Click times:\n";
        for (const auto& click : clickTimes) {
            std::cout << std::fixed << std::setprecision(3) 
                      << click.seconds << std::endl;
        }
    }
    
    std::string generateFormattedString() const {
        std::stringstream ss;
        
        for (size_t i = 0; i < clickTimes.size(); ++i) {
            double milliseconds = clickTimes[i].seconds * 1000.0;
            
            ss << std::fixed << std::setprecision(2) << milliseconds / 1000.0;
            ss << "~0";
            
            if (i < clickTimes.size() - 1) {
                ss << "~";
            }
        }
        
        ss << "~";
        return ss.str();
    }
    
    void printFormattedString() const {
        std::cout << "Formatted string:\n";
        std::cout << generateFormattedString() << std::endl;
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
        
//         parser.printFormattedString();
        
//     } else {
//         std::cout << "Error processing file!" << std::endl;
//         return 1;
//     }
    
//     return 0;
// }