/*
This has been taken from https://github.com/metayeti/mINI/tree/master and
modified to fit the needs of this project. The original license is MIT, and the
original author is @metayeti.
The ini file generation/writing part has been removed.
*/
#ifndef MINI_INI_H_
#define MINI_INI_H_

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mINI {
namespace INIStringUtil {
const char* const whitespaceDelimiters = " \t\n\r\f\v";
inline void trim(std::string& str) {
    str.erase(str.find_last_not_of(whitespaceDelimiters) + 1);
    str.erase(0, str.find_first_not_of(whitespaceDelimiters));
}
#ifndef MINI_CASE_SENSITIVE
inline void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](const char c) {
        return static_cast<char>(std::tolower(c));
    });
}
#endif
inline void replace(std::string& str, std::string const& a,
                    std::string const& b) {
    if (!a.empty()) {
        std::size_t pos = 0;
        while ((pos = str.find(a, pos)) != std::string::npos) {
            str.replace(pos, a.size(), b);
            pos += b.size();
        }
    }
}

}  // namespace INIStringUtil

template <typename T>
class INIMap {
   private:
    using T_DataIndexMap = std::unordered_map<std::string, std::size_t>;
    using T_DataItem = std::pair<std::string, T>;
    using T_DataContainer = std::vector<T_DataItem>;
    using T_MultiArgs = typename std::vector<std::pair<std::string, T>>;

    T_DataIndexMap dataIndexMap;
    T_DataContainer data;

    std::size_t setEmpty(std::string& key) {
        const std::size_t index = data.size();
        dataIndexMap[key] = index;
        data.emplace_back(key, T());
        return index;
    }

   public:
    using const_iterator = typename T_DataContainer::const_iterator;

    INIMap() = default;

    INIMap(INIMap const& other)
        : dataIndexMap(other.dataIndexMap), data(other.data) {}

    T& operator[](std::string key) {
        INIStringUtil::trim(key);
#ifndef MINI_CASE_SENSITIVE
        INIStringUtil::toLower(key);
#endif
        auto it = dataIndexMap.find(key);
        const bool hasIt = (it != dataIndexMap.end());
        const std::size_t index = (hasIt) ? it->second : setEmpty(key);
        return data[index].second;
    }
    [[nodiscard]] T get(std::string key) const {
        INIStringUtil::trim(key);
#ifndef MINI_CASE_SENSITIVE
        INIStringUtil::toLower(key);
#endif
        auto it = dataIndexMap.find(key);
        if (it == dataIndexMap.end()) {
            return T();
        }
        return T(data[it->second].second);
    }
    [[nodiscard]] bool has(std::string key) const {
        INIStringUtil::trim(key);
#ifndef MINI_CASE_SENSITIVE
        INIStringUtil::toLower(key);
#endif
        return (dataIndexMap.count(key) == 1);
    }
    void set(std::string key, T obj) {
        INIStringUtil::trim(key);
#ifndef MINI_CASE_SENSITIVE
        INIStringUtil::toLower(key);
#endif
        auto it = dataIndexMap.find(key);
        if (it != dataIndexMap.end()) {
            data[it->second].second = obj;
        } else {
            dataIndexMap[key] = data.size();
            data.emplace_back(key, obj);
        }
    }
    void set(T_MultiArgs const& multiArgs) {
        for (auto const& it : multiArgs) {
            auto const& key = it.first;
            auto const& obj = it.second;
            set(key, obj);
        }
    }
    bool remove(std::string key) {
        INIStringUtil::trim(key);
#ifndef MINI_CASE_SENSITIVE
        INIStringUtil::toLower(key);
#endif
        auto it = dataIndexMap.find(key);
        if (it != dataIndexMap.end()) {
            std::size_t index = it->second;
            data.erase(data.begin() + index);
            dataIndexMap.erase(it);
            for (auto& it2 : dataIndexMap) {
                auto& vi = it2.second;
                if (vi > index) {
                    vi--;
                }
            }
            return true;
        }
        return false;
    }
    void clear() {
        data.clear();
        dataIndexMap.clear();
    }
    [[nodiscard]] std::size_t size() const { return data.size(); }
    [[nodiscard]] const_iterator begin() const { return data.begin(); }
    [[nodiscard]] const_iterator end() const { return data.end(); }
};

using INIStructure = INIMap<INIMap<std::string>>;

namespace INIParser {
using T_ParseValues = std::pair<std::string, std::string>;

enum class PDataType : char {
    PDATA_NONE,
    PDATA_COMMENT,
    PDATA_SECTION,
    PDATA_KEYVALUE,
    PDATA_UNKNOWN
};

inline PDataType parseLine(std::string line, T_ParseValues& parseData) {
    parseData.first.clear();
    parseData.second.clear();
    INIStringUtil::trim(line);
    if (line.empty()) {
        return PDataType::PDATA_NONE;
    }
    const char firstCharacter = line[0];
    if (firstCharacter == ';') {
        return PDataType::PDATA_COMMENT;
    }
    if (firstCharacter == '[') {
        auto commentAt = line.find_first_of(';');
        if (commentAt != std::string::npos) {
            line = line.substr(0, commentAt);
        }
        auto closingBracketAt = line.find_last_of(']');
        if (closingBracketAt != std::string::npos) {
            auto section = line.substr(1, closingBracketAt - 1);
            INIStringUtil::trim(section);
            parseData.first = section;
            return PDataType::PDATA_SECTION;
        }
    }
    auto lineNorm = line;
    INIStringUtil::replace(lineNorm, "\\=", "  ");
    auto equalsAt = lineNorm.find_first_of('=');
    if (equalsAt != std::string::npos) {
        auto key = line.substr(0, equalsAt);
        INIStringUtil::trim(key);
        INIStringUtil::replace(key, "\\=", "=");
        auto value = line.substr(equalsAt + 1);
        INIStringUtil::trim(value);
        parseData.first = key;
        parseData.second = value;
        return PDataType::PDATA_KEYVALUE;
    }
    return PDataType::PDATA_UNKNOWN;
}
}  // namespace INIParser

class INIReader {
   public:
    using T_LineData = std::vector<std::string>;
    using T_LineDataPtr = std::shared_ptr<T_LineData>;

    bool isBOM = false;

   private:
    std::ifstream fileReadStream;
    T_LineDataPtr lineData;

    T_LineData readFile() {
        fileReadStream.seekg(0, std::ios::end);
        const std::size_t fileSize =
            static_cast<std::size_t>(fileReadStream.tellg());
        fileReadStream.seekg(0, std::ios::beg);
        if (fileSize >= 3) {
            const char header[3] = {static_cast<char>(fileReadStream.get()),
                                    static_cast<char>(fileReadStream.get()),
                                    static_cast<char>(fileReadStream.get())};
            isBOM = (header[0] == static_cast<char>(0xEF) &&
                     header[1] == static_cast<char>(0xBB) &&
                     header[2] == static_cast<char>(0xBF));
        } else {
            isBOM = false;
        }
        std::string fileContents;
        fileContents.resize(fileSize);
        fileReadStream.seekg(isBOM ? 3 : 0, std::ios::beg);
        fileReadStream.read(fileContents.data(), fileSize);
        fileReadStream.close();
        T_LineData output;
        if (fileSize == 0) {
            return output;
        }
        std::string buffer;
        buffer.reserve(50);
        for (std::size_t i = 0; i < fileSize; ++i) {
            const char& c = fileContents[i];
            if (c == '\n') {
                output.emplace_back(buffer);
                buffer.clear();
                continue;
            }
            if (c != '\0' && c != '\r') {
                buffer += c;
            }
        }
        output.emplace_back(buffer);
        return output;
    }

   public:
    INIReader(std::filesystem::path const& filename,
              bool keepLineData = false) {
        fileReadStream.open(filename, std::ios::in | std::ios::binary);
        if (keepLineData) {
            lineData = std::make_shared<T_LineData>();
        }
    }
    ~INIReader() = default;

    bool operator>>(INIStructure& data) {
        if (!fileReadStream.is_open()) {
            return false;
        }
        const T_LineData fileLines = readFile();
        std::string section;
        bool inSection = false;
        INIParser::T_ParseValues parseData;
        for (auto const& line : fileLines) {
            auto parseResult = INIParser::parseLine(line, parseData);
            if (parseResult == INIParser::PDataType::PDATA_SECTION) {
                inSection = true;
                data[section = parseData.first];
            } else if (inSection &&
                       parseResult == INIParser::PDataType::PDATA_KEYVALUE) {
                auto const& key = parseData.first;
                auto const& value = parseData.second;
                data[section][key] = value;
            }
            if (lineData &&
                parseResult != INIParser::PDataType::PDATA_UNKNOWN) {
                if (parseResult == INIParser::PDataType::PDATA_KEYVALUE &&
                    !inSection) {
                    continue;
                }
                lineData->emplace_back(line);
            }
        }
        return true;
    }
    T_LineDataPtr getLines() { return lineData; }
};

class INIFile {
   private:
    std::filesystem::path filename;

   public:
    INIFile(std::filesystem::path filename) : filename(std::move(filename)) {}

    ~INIFile() = default;

    bool read(INIStructure& data) const {
        if (data.size() != 0U) {
            data.clear();
        }
        if (filename.empty()) {
            return false;
        }
        INIReader reader(filename);
        return reader >> data;
    }
};
}  // namespace mINI

#endif  // MINI_INI_H_
