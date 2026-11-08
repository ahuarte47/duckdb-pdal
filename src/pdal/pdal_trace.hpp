#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>

#include <cstring>

namespace duckdb {
namespace PdalTrace {

inline thread_local const char *current_checkpoint = "extension load";

inline LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS *) noexcept {
	const HANDLE error_handle = GetStdHandle(STD_ERROR_HANDLE);
	if (error_handle != INVALID_HANDLE_VALUE && error_handle != nullptr) {
		static constexpr char prefix[] = "Error: PDAL TRACE: ";
		static constexpr char suffix[] = "\r\n";
		DWORD written = 0;
		WriteFile(error_handle, prefix, sizeof(prefix) - 1, &written, nullptr);
		const char *checkpoint = current_checkpoint;
		WriteFile(error_handle, checkpoint, static_cast<DWORD>(std::strlen(checkpoint)), &written, nullptr);
		WriteFile(error_handle, suffix, sizeof(suffix) - 1, &written, nullptr);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

inline void Install() {
	SetUnhandledExceptionFilter(HandleUnhandledException);
}

inline void SetCheckpoint(const char *checkpoint) noexcept {
	current_checkpoint = checkpoint;
}

} // namespace PdalTrace
} // namespace duckdb
#else
namespace duckdb {
namespace PdalTrace {

inline void Install() {
}

inline void SetCheckpoint(const char *) noexcept {
}

} // namespace PdalTrace
} // namespace duckdb
#endif
