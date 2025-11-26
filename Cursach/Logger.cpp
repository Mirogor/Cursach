#include "Logger.h"
#include "Utils.h"       // для GetAppDataDir/TimePointToWString
#include <fstream>
#include <chrono>

Logger g_Logger;

Logger::Logger() {
    std::wstring dir = util::GetAppDataDir();
    logFilePath_ = dir + L"\\scheduler.log";
}

Logger::~Logger() = default;

void Logger::Log(LogLevel level, const std::wstring& tag, const std::wstring& message) {
    std::lock_guard<std::mutex> lock(mtx_);

    const wchar_t* levelNames[] = { L"DEBUG", L"INFO", L"WARN", L"ERROR" };

    auto now = std::chrono::system_clock::now();
    std::wstring ts = util::TimePointToWString(now);

    std::wofstream ofs(logFilePath_, std::ios::app);
    if (!ofs) return; // best-effort

    int idx = 0;
    switch (level) {
    case LogLevel::Debug: idx = 0; break;
    case LogLevel::Info:  idx = 1; break;
    case LogLevel::Warn:  idx = 2; break;
    case LogLevel::Error: idx = 3; break;
    default: idx = 1; break;
    }

    ofs << ts << L" [" << levelNames[idx] << L"] [" << tag << L"] " << message << L"\n";
    ofs.close();
}
