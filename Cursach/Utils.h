#pragma once
#include <string>
#include <chrono>

namespace util {

	std::wstring GetAppDataDir();
	std::wstring GenerateGUID(); // throws or returns fallback string on failure
	std::wstring TimePointToWString(const std::chrono::system_clock::time_point& tp);
	bool IsValidJsonSimple(const std::wstring& s);
	std::wstring EscapeJSON(const std::wstring& s);
	std::wstring UnescapeJSON(const std::wstring& s);  // ← ДОБАВЛЕНО
	std::wstring GetFileName(const std::wstring& path);  // ← ДОБАВЛЕНО: извлечь имя файла из пути

} // namespace util