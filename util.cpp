#include "util.h"
#include <stdarg.h>
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include "ggml_extend.hpp"

#if defined(__APPLE__) && defined(__MACH__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "ggml/ggml.h"
#include "stable-diffusion.h"

bool ends_with(const std::string& str, const std::string& ending) {
    if (str.length() >= ending.length()) {
        return (str.compare(str.length() - ending.length(), ending.length(), ending) == 0);
    } else {
        return false;
    }
}

bool starts_with(const std::string& str, const std::string& start) {
    if (str.find(start) == 0) {
        return true;
    }
    return false;
}

bool contains(const std::string& str, const std::string& substr) {
    if (str.find(substr) != std::string::npos) {
        return true;
    }
    return false;
}

void replace_all_chars(std::string& str, char target, char replacement) {
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == target) {
            str[i] = replacement;
        }
    }
}

std::string format(const char* fmt, ...) {
    va_list ap;
    va_list ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int size = vsnprintf(NULL, 0, fmt, ap);
    std::vector<char> buf(size + 1);
    int size2 = vsnprintf(buf.data(), size + 1, fmt, ap2);
    va_end(ap2);
    va_end(ap);
    return std::string(buf.data(), size);
}

#ifdef _WIN32  // code for windows
#include <windows.h>

bool file_exists(const std::string& filename) {
    DWORD attributes = GetFileAttributesA(filename.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}

bool is_directory(const std::string& path) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}

std::string get_full_path(const std::string& dir, const std::string& filename) {
    std::string full_path = dir + "\\" + filename;

    WIN32_FIND_DATA find_file_data;
    HANDLE hFind = FindFirstFile(full_path.c_str(), &find_file_data);

    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return full_path;
    } else {
        return "";
    }
}

// Windows code copied from https://github.com/leejet/stable-diffusion.cpp/pull/1059/changes
// but NOT TESTED, because this util.cpp is intended for the Jetson Nano running *Linux*
// akleine, Mar 2026
class MmapWrapperImpl : public MmapWrapper {
public:
    MmapWrapperImpl(void* data, size_t size, HANDLE hfile, HANDLE hmapping)
        : MmapWrapper(data, size), hfile_(hfile), hmapping_(hmapping) {}
    ~MmapWrapperImpl() override {
        UnmapViewOfFile(data_);
        CloseHandle(hmapping_);
        CloseHandle(hfile_);
    }

private:
    HANDLE hfile_;
    HANDLE hmapping_;
};
std::unique_ptr<MmapWrapper> MmapWrapper::create(const std::string& filename) {
    void* mapped_data  = nullptr;
    size_t file_size   = 0;
    HANDLE file_handle = CreateFileA(
        filename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_handle, &size)) {
        CloseHandle(file_handle);
        return nullptr;
    }
    file_size             = static_cast<size_t>(size.QuadPart);
    HANDLE mapping_handle = CreateFileMapping(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping_handle == NULL) {
        CloseHandle(file_handle);
        return nullptr;
    }
    mapped_data = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, file_size);
    if (mapped_data == NULL) {
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return nullptr;
    }
    return std::make_unique<MmapWrapperImpl>(mapped_data, file_size, file_handle, mapping_handle);
}

#else  // Unix
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

bool file_exists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}

bool is_directory(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

std::string get_full_path(const std::string& dir, const std::string& filename) {
    DIR* dp = opendir(dir.c_str());

    if (dp != nullptr) {
        struct dirent* entry;

        while ((entry = readdir(dp)) != nullptr) {
            if (strcasecmp(entry->d_name, filename.c_str()) == 0) {
                closedir(dp);
                return dir + "/" + entry->d_name;
            }
        }

        closedir(dp);
    }

    return "";
}

class MmapWrapperImpl : public MmapWrapper {
public:
    MmapWrapperImpl(void* data, size_t size)
        : MmapWrapper(data, size) {}
    ~MmapWrapperImpl() override {
        munmap(data_, size_);
    }
};

std::unique_ptr<MmapWrapper> MmapWrapper::create(const std::string& filename) {
    int file_descriptor = open(filename.c_str(), O_RDONLY);
    if (file_descriptor == -1) {
        return nullptr;
    }
    int mmap_flags = MAP_PRIVATE;

#ifdef __linux__
    // performance flags used by llama.cpp
    // posix_fadvise(file_descriptor, 0, 0, POSIX_FADV_SEQUENTIAL);
    // mmap_flags |= MAP_POPULATE;
#endif

    struct stat sb;
    if (fstat(file_descriptor, &sb) == -1) {
        close(file_descriptor);
        return nullptr;
    }
    size_t file_size = sb.st_size;

    void* mapped_data = mmap(NULL, file_size, PROT_READ, mmap_flags, file_descriptor, 0);
    close(file_descriptor);
    if (mapped_data == MAP_FAILED) {
        return nullptr;
    }
#ifdef __linux__
    // performance flags used by llama.cpp
    // posix_madvise(mapped_data, file_size, POSIX_MADV_WILLNEED);
#endif

    return std::make_unique<MmapWrapperImpl>(mapped_data, file_size);
}
#endif
bool MmapWrapper::copy_data(void* buf, size_t n, size_t offset) const {
    if (offset >= size_ || n > (size_ - offset)) {
        return false;
    }
    std::memcpy(buf, data() + offset, n);
    return true;
}

// get_num_physical_cores is copy from
// https://github.com/ggerganov/llama.cpp/blob/master/examples/common.cpp
// LICENSE: https://github.com/ggerganov/llama.cpp/blob/master/LICENSE
int32_t get_num_physical_cores() {
#ifdef __linux__
    // enumerate the set of thread siblings, num entries is num cores
    std::unordered_set<std::string> siblings;
    for (uint32_t cpu = 0; cpu < UINT32_MAX; ++cpu) {
        std::ifstream thread_siblings("/sys/devices/system/cpu" + std::to_string(cpu) + "/topology/thread_siblings");
        if (!thread_siblings.is_open()) {
            break;  // no more cpus
        }
        std::string line;
        if (std::getline(thread_siblings, line)) {
            siblings.insert(line);
        }
    }
    if (siblings.size() > 0) {
        return static_cast<int32_t>(siblings.size());
    }
#elif defined(__APPLE__) && defined(__MACH__)
    int32_t num_physical_cores;
    size_t len = sizeof(num_physical_cores);
    int result = sysctlbyname("hw.perflevel0.physicalcpu", &num_physical_cores, &len, NULL, 0);
    if (result == 0) {
        return num_physical_cores;
    }
    result = sysctlbyname("hw.physicalcpu", &num_physical_cores, &len, NULL, 0);
    if (result == 0) {
        return num_physical_cores;
    }
#elif defined(_WIN32)
    // TODO: Implement
#endif
    unsigned int n_threads = std::thread::hardware_concurrency();
    return n_threads > 0 ? (n_threads <= 4 ? n_threads : n_threads / 2) : 4;
}

static sd_progress_cb_t sd_progress_cb = NULL;
void* sd_progress_cb_data              = NULL;

std::u32string utf8_to_utf32(const std::string& utf8_str) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.from_bytes(utf8_str);
}

std::string utf32_to_utf8(const std::u32string& utf32_str) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.to_bytes(utf32_str);
}

std::u32string unicode_value_to_utf32(int unicode_value) {
    std::u32string utf32_string = {static_cast<char32_t>(unicode_value)};
    return utf32_string;
}

static std::string sd_basename(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    pos = path.find_last_of('\\');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string path_join(const std::string& p1, const std::string& p2) {
    if (p1.empty()) {
        return p2;
    }

    if (p2.empty()) {
        return p1;
    }

    if (p1[p1.length() - 1] == '/' || p1[p1.length() - 1] == '\\') {
        return p1 + p2;
    }

    return p1 + "/" + p2;
}

void pretty_progress(int step, int steps, float time) {
    if (sd_progress_cb) {
        sd_progress_cb(step, steps, time, sd_progress_cb_data);
        return;
    }
    if (step == 0) {
        return;
    }
    std::string progress = "  |";
    int max_progress     = 50;
    int32_t current      = (int32_t)(step * 1.f * max_progress / steps);
    for (int i = 0; i < 50; i++) {
        if (i > current) {
            progress += " ";
        } else if (i == current && i != max_progress - 1) {
            progress += ">";
        } else {
            progress += "=";
        }
    }
    progress += "|";

    const char* lf   = (step == steps ? "\n" : "");
    const char* unit = "s/it";
    float speed      = time;
    if (speed < 1.0f && speed > 0.f) {
        speed = 1.0f / speed;
        unit  = "it/s";
    }
    printf("\r%s %i/%i - %.2f%s\033[K%s", progress.c_str(), step, steps, speed, unit, lf);
    fflush(stdout);  // for linux
}

std::string ltrim(const std::string& s) {
    auto it = std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    });
    return std::string(it, s.end());
}

std::string rtrim(const std::string& s) {
    auto it = std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    });
    return std::string(s.begin(), it.base());
}

std::string trim(const std::string& s) {
    return rtrim(ltrim(s));
}

static sd_log_cb_t sd_log_cb = NULL;
void* sd_log_cb_data         = NULL;

#define LOG_BUFFER_SIZE 1024

void log_printf(sd_log_level_t level, const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);

    static char log_buffer[LOG_BUFFER_SIZE + 1];
    int written = snprintf(log_buffer, LOG_BUFFER_SIZE, "%s:%-4d - ", sd_basename(file).c_str(), line);

    if (written >= 0 && written < LOG_BUFFER_SIZE) {
        vsnprintf(log_buffer + written, LOG_BUFFER_SIZE - written, format, args);
    }
    strncat(log_buffer, "\n", LOG_BUFFER_SIZE - strlen(log_buffer));

    if (sd_log_cb) {
        sd_log_cb(level, log_buffer, sd_log_cb_data);
    }

    va_end(args);
}

void sd_set_log_callback(sd_log_cb_t cb, void* data) {
    sd_log_cb      = cb;
    sd_log_cb_data = data;
}
void sd_set_progress_callback(sd_progress_cb_t cb, void* data) {
    sd_progress_cb      = cb;
    sd_progress_cb_data = data;
}
const char* sd_get_system_info() {
    static char buffer[1024];
    std::stringstream ss;
    ss << "System Info: \n";
    ss << "    BLAS = " << ggml_cpu_has_blas() << std::endl;
    ss << "    SSE3 = " << ggml_cpu_has_sse3() << std::endl;
    ss << "    AVX = " << ggml_cpu_has_avx() << std::endl;
    ss << "    AVX2 = " << ggml_cpu_has_avx2() << std::endl;
    ss << "    AVX512 = " << ggml_cpu_has_avx512() << std::endl;
    ss << "    AVX512_VBMI = " << ggml_cpu_has_avx512_vbmi() << std::endl;
    ss << "    AVX512_VNNI = " << ggml_cpu_has_avx512_vnni() << std::endl;
    ss << "    FMA = " << ggml_cpu_has_fma() << std::endl;
    ss << "    NEON = " << ggml_cpu_has_neon() << std::endl;
    ss << "    ARM_FMA = " << ggml_cpu_has_arm_fma() << std::endl;
    ss << "    F16C = " << ggml_cpu_has_f16c() << std::endl;
    ss << "    FP16_VA = " << ggml_cpu_has_fp16_va() << std::endl;
    ss << "    WASM_SIMD = " << ggml_cpu_has_wasm_simd() << std::endl;
    ss << "    VSX = " << ggml_cpu_has_vsx() << std::endl;
    snprintf(buffer, sizeof(buffer), "%s", ss.str().c_str());
    return buffer;
}

const char* sd_type_name(enum sd_type_t type) {
    return ggml_type_name((ggml_type)type);
}
