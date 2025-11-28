#include "Utils.h"
#include <Windows.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>

namespace util {

    std::wstring GetAppDataDir() {
        wchar_t path[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            std::wstring dir = path;
            dir += L"\\MiniTaskScheduler";
            CreateDirectoryW(dir.c_str(), NULL);
            return dir;
        }
        return L".";
    }

    std::wstring GenerateGUID() {
        GUID guid;
        HRESULT hr = CoCreateGuid(&guid);
        if (FAILED(hr)) {
            // fallback: use tick count (deterministic but unique enough for session)
            wchar_t buf[32];
            swprintf_s(buf, L"fallback-%llu", (unsigned long long)GetTickCount64());
            return std::wstring(buf);
        }
        wchar_t buf[64];
        swprintf_s(buf, L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::wstring(buf);
    }

    std::wstring TimePointToWString(const std::chrono::system_clock::time_point& tp) {
        if (tp.time_since_epoch().count() == 0) return L"Никогда";
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm;
        localtime_s(&tm, &t);
        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::wstring(buf);
    }

    bool IsValidJsonSimple(const std::wstring& s) {
        int depth = 0;
        bool inQuotes = false;
        bool escape = false;
        for (wchar_t c : s) {
            if (escape) { escape = false; continue; }
            if (c == L'\\') { escape = true; continue; }
            if (c == L'"') inQuotes = !inQuotes;
            if (!inQuotes) {
                if (c == L'{') ++depth;
                else if (c == L'}') --depth;
                if (depth < 0) return false;
            }
        }
        return !inQuotes && depth == 0;
    }

    std::wstring EscapeJSON(const std::wstring& s) {
        std::wstring out; out.reserve(s.size());
        for (wchar_t c : s) {
            switch (c) {
            case L'\\': out += L"\\\\"; break;
            case L'"':  out += L"\\\""; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default: out.push_back(c); break;
            }
        }
        return out;
    }

    // ← ДОБАВЛЕНО: Обратная операция для EscapeJSON
    std::wstring UnescapeJSON(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());

        bool escape = false;
        for (wchar_t c : s) {
            if (escape) {
                switch (c) {
                case L'\\': out.push_back(L'\\'); break;
                case L'"':  out.push_back(L'"'); break;
                case L'n':  out.push_back(L'\n'); break;
                case L'r':  out.push_back(L'\r'); break;
                case L't':  out.push_back(L'\t'); break;
                default:    out.push_back(c); break;  // Неизвестная escape-последовательность
                }
                escape = false;
            }
            else if (c == L'\\') {
                escape = true;
            }
            else {
                out.push_back(c);
            }
        }

        return out;
    }

    // ← ДОБАВЛЕНО: Извлечение имени файла из полного пути
    std::wstring GetFileName(const std::wstring& path) {
        if (path.empty()) return L"";

        // Ищем последний слеш или обратный слеш
        size_t pos = path.find_last_of(L"\\/");

        if (pos == std::wstring::npos) {
            return path;  // Уже имя файла без пути
        }

        return path.substr(pos + 1);
    }

} // namespace util