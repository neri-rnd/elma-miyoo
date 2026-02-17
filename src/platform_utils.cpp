#include "platform_utils.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>

#ifndef _WIN32
#include <cassert>
#include <string>

void itoa(int value, char* str, int base) {
    assert(base == 10);
    std::string tmp2 = std::to_string(value);
    std::strcpy(str, tmp2.c_str());
}

int strcmpi(const char* a, const char* b) {
    char ca, cb;
    int v;
    do {
        ca = *a++;
        cb = *b++;
        v = (unsigned int)std::tolower(ca) - (unsigned int)std::tolower(cb);
    } while (!v && ca && cb);
    return v;
}

int strnicmp(const char* a, const char* b, size_t len) {
    char ca, cb;
    int v;
    do {
        ca = *a++;
        cb = *b++;
        v = (unsigned int)std::tolower(ca) - (unsigned int)std::tolower(cb);
        len--;
    } while (!v && ca && cb && len);
    return v;
}

void strupr(char* str) {
    while (*str) {
        *str = std::toupper((unsigned char)*str);
        str++;
    }
}

void strlwr(char* str) {
    while (*str) {
        *str = std::tolower((unsigned char)*str);
        str++;
    }
}
#endif

bool is_ascii_character(unsigned char c) { return (c >= 32 && c < 127); }

bool is_char_valid_for_filename(unsigned char c) {
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' ||
        c == '>' || c == '|') {
        return false;
    }

    return is_ascii_character(c);
}

FILE* fopen_icase(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    if (f) return f;

    // Extract directory and filename from path
    const char* slash = strrchr(path, '/');
    char dir_buf[256];
    const char* target;
    if (slash) {
        int dir_len = (int)(slash - path);
        if (dir_len >= (int)sizeof(dir_buf)) return nullptr;
        memcpy(dir_buf, path, dir_len);
        dir_buf[dir_len] = '\0';
        target = slash + 1;
    } else {
        strcpy(dir_buf, ".");
        target = path;
    }

    DIR* dir = opendir(dir_buf);
    if (!dir) return nullptr;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmpi(entry->d_name, target) == 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_buf, entry->d_name);
            closedir(dir);
            return fopen(full_path, mode);
        }
    }
    closedir(dir);
    return nullptr;
}
