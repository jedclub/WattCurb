#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string_view>

namespace wattcurb::core::fs {

// Implements REF-REQ-030 & REF-ARCH-020: Direct single-syscall existence check
[[nodiscard]] inline bool file_exists(const char* path) noexcept {
    if (!path || path[0] == '\0') return false;
    return ::access(path, F_OK) == 0;
}

[[nodiscard]] inline bool file_exists(std::string_view path) noexcept {
    if (path.empty() || path.size() >= 256) return false;
    char buf[256];
    std::memcpy(buf, path.data(), path.size());
    buf[path.size()] = '\0';
    return file_exists(buf);
}

// Implements REF-REQ-030: Zero-allocation POSIX directory iterator callback
template <typename Callback>
inline void for_each_dir_entry(const char* dir_path, Callback&& cb) noexcept {
    DIR* dir = ::opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        // Skip '.' and '..'
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        cb(std::string_view(entry->d_name));
    }
    ::closedir(dir);
}

// Implements REF-REQ-030: Zero-allocation small file reader (replaces std::ifstream)
inline bool read_small_file(const char* path, char* out_buf, size_t max_len, size_t* out_len = nullptr) noexcept {
    if (!path || !out_buf || max_len == 0) return false;

    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    ssize_t n = ::read(fd, out_buf, max_len - 1);
    ::close(fd);
    if (n < 0) return false;

    out_buf[n] = '\0';
    if (out_len) *out_len = static_cast<size_t>(n);
    return true;
}

} // namespace wattcurb::core::fs
