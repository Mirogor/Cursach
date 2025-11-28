#include "Persistence.h"
#include "Task.h"
#include "Utils.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <Windows.h>

Persistence::Persistence() {
    path_ = util::GetAppDataDir() + L"\\tasks.json";
}

bool Persistence::Save(const std::vector<TaskPtr>& tasks) {
    std::wstring tmp = path_ + L".tmp";
    std::wofstream ofs(tmp, std::ios::trunc);
    if (!ofs) {
        g_Logger.Log(LogLevel::Error, L"Persistence", L"Cannot open temp file for writing: " + tmp);
        return false;
    }

    ofs << L"{\n  \"tasks\": [\n";
    for (size_t i = 0; i < tasks.size(); ++i) {
        auto& t = tasks[i];
        ofs << L"    {\n";
        ofs << L"      \"id\": \"" << util::EscapeJSON(t->id) << L"\",\n";
        ofs << L"      \"name\": \"" << util::EscapeJSON(t->name) << L"\",\n";
        ofs << L"      \"description\": \"" << util::EscapeJSON(t->description) << L"\",\n";
        ofs << L"      \"exePath\": \"" << util::EscapeJSON(t->exePath) << L"\",\n";
        ofs << L"      \"arguments\": \"" << util::EscapeJSON(t->arguments) << L"\",\n";
        ofs << L"      \"workingDirectory\": \"" << util::EscapeJSON(t->workingDirectory) << L"\",\n";
        ofs << L"      \"enabled\": " << (t->enabled ? L"true" : L"false") << L",\n";
        ofs << L"      \"triggerType\": " << (int)t->triggerType << L",\n";
        ofs << L"      \"intervalMinutes\": " << t->intervalMinutes << L",\n";
        ofs << L"      \"dailyHour\": " << (int)t->dailyHour << L",\n";
        ofs << L"      \"dailyMinute\": " << (int)t->dailyMinute << L",\n";
        ofs << L"      \"dailySecond\": " << (int)t->dailySecond << L",\n";

        unsigned long days = 0;
        for (int k = 0; k < 7; ++k)
            if (t->weeklyDays.test(k)) days |= (1 << k);

        ofs << L"      \"weeklyDays\": " << days << L",\n";
        ofs << L"      \"weeklyHour\": " << (int)t->weeklyHour << L",\n";
        ofs << L"      \"weeklyMinute\": " << (int)t->weeklyMinute << L",\n";
        ofs << L"      \"weeklySecond\": " << (int)t->weeklySecond << L",\n";
        ofs << L"      \"runIfMissed\": " << (t->runIfMissed ? L"true" : L"false") << L"\n";
        ofs << L"    }" << (i + 1 < tasks.size() ? L"," : L"") << L"\n";
    }
    ofs << L"  ]\n}\n";
    ofs.close();

    DeleteFileW(path_.c_str());
    if (!MoveFileW(tmp.c_str(), path_.c_str())) {
        g_Logger.Log(LogLevel::Error, L"Persistence", L"Failed to move temp file to final location");
        return false;
    }

    g_Logger.Log(LogLevel::Info, L"Persistence", L"Tasks saved: " + std::to_wstring(tasks.size()));
    return true;
}

std::vector<TaskPtr> Persistence::Load() {
    std::vector<TaskPtr> out;
    std::wifstream ifs(path_);
    if (!ifs) {
        g_Logger.Log(LogLevel::Info, L"Persistence", L"No tasks file found");
        return out;
    }

    std::wstringstream ss;
    ss << ifs.rdbuf();
    std::wstring content = ss.str();

    if (!util::IsValidJsonSimple(content)) {
        g_Logger.Log(LogLevel::Warn, L"Persistence", L"tasks.json appears invalid (simple check)");
        return out;
    }

    size_t pos = content.find(L"\"tasks\"");
    if (pos == std::wstring::npos) return out;
    pos = content.find(L"[", pos);
    if (pos == std::wstring::npos) return out;

    size_t cur = pos + 1;
    while (true) {
        size_t start = content.find(L"{", cur);
        if (start == std::wstring::npos) break;

        int depth = 1;
        bool inQuotes = false;
        bool escape = false;
        size_t i = start + 1;

        for (; i < content.size(); ++i) {
            wchar_t c = content[i];
            if (escape) { escape = false; continue; }
            if (c == L'\\') { escape = true; continue; }
            if (c == L'"') inQuotes = !inQuotes;

            if (!inQuotes) {
                if (c == L'{') ++depth;
                else if (c == L'}') --depth;
                if (depth == 0) break;
            }
        }

        if (i >= content.size()) break;

        std::wstring block = content.substr(start, i - start + 1);

        // ← ИСПРАВЛЕНО: Теперь применяем UnescapeJSON к строковым полям
        auto getString = [&](const std::wstring& key)->std::wstring {
            size_t p = block.find(L"\"" + key + L"\"");
            if (p == std::wstring::npos) return L"";
            size_t colon = block.find(L":", p);
            size_t q1 = block.find(L"\"", colon);
            size_t q2 = block.find(L"\"", q1 + 1);
            std::wstring raw = block.substr(q1 + 1, q2 - q1 - 1);
            return util::UnescapeJSON(raw);  // ← ДОБАВЛЕНО: разэкранирование
            };

        auto getInt = [&](const std::wstring& key)->long long {
            size_t p = block.find(L"\"" + key + L"\"");
            if (p == std::wstring::npos) return 0;
            size_t colon = block.find(L":", p);
            size_t s = block.find_first_of(L"-0123456789", colon);
            size_t e = s;
            while (e < block.size() && (iswdigit(block[e]) || block[e] == L'-')) ++e;
            return std::stoll(block.substr(s, e - s));
            };

        TaskPtr t = std::make_shared<Task>();
        t->id = getString(L"id");
        t->name = getString(L"name");
        t->description = getString(L"description");
        t->exePath = getString(L"exePath");
        t->arguments = getString(L"arguments");
        t->workingDirectory = getString(L"workingDirectory");

        size_t pEnabled = block.find(L"\"enabled\"");
        if (pEnabled != std::wstring::npos) {
            size_t colon = block.find(L":", pEnabled);
            size_t s = block.find_first_not_of(L" \t\r\n", colon + 1);
            t->enabled = (s != std::wstring::npos && block.compare(s, 4, L"true") == 0);
        }

        t->triggerType = (TriggerType)getInt(L"triggerType");
        t->intervalMinutes = (uint32_t)getInt(L"intervalMinutes");
        t->dailyHour = (uint8_t)getInt(L"dailyHour");
        t->dailyMinute = (uint8_t)getInt(L"dailyMinute");
        t->dailySecond = (uint8_t)getInt(L"dailySecond");

        unsigned long days = (unsigned long)getInt(L"weeklyDays");
        t->weeklyDays = (uint8_t)(days & 0x7F);

        t->weeklyHour = (uint8_t)getInt(L"weeklyHour");
        t->weeklyMinute = (uint8_t)getInt(L"weeklyMinute");
        t->weeklySecond = (uint8_t)getInt(L"weeklySecond");

        size_t pRunIf = block.find(L"\"runIfMissed\"");
        if (pRunIf != std::wstring::npos) {
            size_t colon = block.find(L":", pRunIf);
            size_t s = block.find_first_not_of(L" \t\r\n", colon + 1);
            t->runIfMissed = (s != std::wstring::npos && block.compare(s, 4, L"true") == 0);
        }

        if (t->id.empty()) t->id = util::GenerateGUID();
        out.push_back(t);

        cur = i + 1;
    }

    g_Logger.Log(LogLevel::Info, L"Persistence", L"Loaded tasks: " + std::to_wstring(out.size()));
    return out;
}