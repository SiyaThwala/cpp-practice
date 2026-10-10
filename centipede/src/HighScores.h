#pragma once

#include <filesystem>
#include <vector>

/** Loads, ranks and saves the five highest completed-game scores. */
class HighScores
{
public:
    explicit HighScores(std::filesystem::path file);
    /** Adds a positive score. Returns false if writing to disk fails. */
    bool Record(int score);
    const std::vector<int>& Entries() const { return entries_; }
    int Best() const { return entries_.empty() ? 0 : entries_.front(); }

private:
    void Load();
    void SortAndTrim();
    std::filesystem::path file_;
    std::vector<int> entries_;
};
