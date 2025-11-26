#pragma once
#include <vector>
#include <string>
#include <memory>

struct Task; // forward (if Task defined elsewhere)
using TaskPtr = std::shared_ptr<Task>;

class Persistence {
public:
    Persistence();
    bool Save(const std::vector<TaskPtr>& tasks);
    std::vector<TaskPtr> Load();
private:
    std::wstring path_;
};
