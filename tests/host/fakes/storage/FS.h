// Test-only filesystem/VFS dependencies; no simulated implementation is shipped.
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

inline std::map<std::string, std::vector<uint8_t>> testFiles;
inline bool testMounted = false, testUnavailable = false;
inline int testMountError = 0, testFormats = 0, testRegisters = 0;
inline int testWrites = 0, testRemoves = 0, testRenames = 0;
inline std::string testDiagnostics;
struct TestSerial {
    void println(const char* s) { testDiagnostics += std::string(s) + '\n'; }
    void printf(const char* fmt, ...) {
        char text[512]; va_list ap; va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
        testDiagnostics += text;
    }
};
inline TestSerial Serial;
#define log_w(...) Serial.printf(__VA_ARGS__)
#define log_e(...) Serial.printf(__VA_ARGS__)
using esp_err_t = int;
inline constexpr int ESP_OK = 0, ESP_FAIL = -1;
struct esp_vfs_littlefs_conf_t {
    const char* base_path;
    const char* partition_label;
    void* partition;
    bool format_if_mount_failed, read_only, dont_mount, grow_on_mount;
};
inline bool esp_littlefs_mounted(const char*) { return testMounted; }
inline int esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* c) {
    assert(!c->format_if_mount_failed);
    ++testRegisters;
    if (!testMountError) testMounted = true;
    return testMountError;
}

class File {
    std::vector<uint8_t>* data_ = nullptr;
    size_t offset_ = 0;
public:
    File() = default;
    explicit File(std::vector<uint8_t>* data) : data_(data) {}
    explicit operator bool() const { return data_ != nullptr; }
    void close() { data_ = nullptr; }
    size_t size() const { return data_ ? data_->size() : 0; }
    size_t read(uint8_t* dst, size_t n) {
        if (!data_) return 0;
        n = std::min(n, data_->size() - offset_);
        std::copy_n(data_->data() + offset_, n, dst); offset_ += n; return n;
    }
    size_t write(const uint8_t* src, size_t n) {
        ++testWrites;
        if (!data_) return 0;
        data_->insert(data_->end(), src, src + n); return n;
    }
    size_t write(uint8_t b) { return write(&b, 1); }
};
namespace fs { using File = ::File; }
class LittleFSFS {
    struct Impl { void mountpoint(const char*) {} } impl_;
public:
    char* partitionLabel_ = nullptr;
    Impl* _impl = &impl_;
    ~LittleFSFS() { free(partitionLabel_); }
    // Definition is extracted unchanged from the installed pinned Arduino API.
    bool begin(bool, const char* = "/littlefs", uint8_t = 10, const char* = "spiffs");
    bool format() { ++testFormats; testFiles.clear(); return true; }
    File open(const char* path, const char* mode) {
        if (*mode == 'w') ++testWrites;
        if (!testMounted || testUnavailable) return {};
        if (*mode == 'w') { testFiles[path].clear(); return File(&testFiles[path]); }
        auto it = testFiles.find(path);
        return it == testFiles.end() ? File{} : File(&it->second);
    }
    bool exists(const char* path) { return testMounted && !testUnavailable && testFiles.count(path); }
    bool remove(const char* path) { ++testRemoves; return testMounted && !testUnavailable && testFiles.erase(path); }
    bool rename(const char* a, const char* b) {
        ++testRenames;
        if (!exists(a)) return false;
        testFiles[b] = std::move(testFiles[a]); testFiles.erase(a); return true;
    }
};
inline LittleFSFS LittleFS;
