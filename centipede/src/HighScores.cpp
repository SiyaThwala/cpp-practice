#include "HighScores.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <utility>

HighScores::HighScores(std::filesystem::path file) : file_{std::move(file)}
{
    Load();
}

void HighScores::SortAndTrim()
{
    std::sort(entries_.begin(), entries_.end(), std::greater<int>{});
    if (entries_.size() > 5) entries_.resize(5);
}

void HighScores::Load()
{
    std::ifstream input{file_};
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        int value = 0;
        const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
        if (result.ec == std::errc{} && result.ptr == line.data() + line.size() && value > 0)
            entries_.push_back(value);
    }
    SortAndTrim();
}

bool HighScores::Record(int score)
{
    if (score <= 0) return true;
    entries_.push_back(score);
    SortAndTrim();
    std::error_code error;
    if (!file_.parent_path().empty())
        std::filesystem::create_directories(file_.parent_path(), error);
    if (error) return false;
    std::ofstream output{file_, std::ios::trunc};
    if (!output) return false;
    for (const auto entry : entries_) output << entry << '\n';
    output.close();
    return !output.fail();
}
