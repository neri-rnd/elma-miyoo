#include "menu_external.h"
#include "menu_nav.h"
#include "fs_utils.h"
#include "LEJATSZO.H"
#include "LOAD.H"
#include "main.h"
#include "menu_nav.h"
#include "menu_pic.h"
#include "menu_play.h"
#include "platform_impl.h"
#include "platform_utils.h"
#include "state.h"
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void browse_directory(const std::string& current_subdir) {
    std::string dir_path = "lev/" + current_subdir;

    std::vector<std::string> dir_names;
    std::vector<std::string> lev_names;

    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().generic_string();
                dir_names.push_back(name);
            } else if (entry.is_regular_file()) {
                const auto& path = entry.path();
                if (strcmpi(path.extension().generic_string().c_str(), ".lev") == 0) {
                    std::string name = path.filename().generic_string();
                    lev_names.push_back(name);
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        return;
    }

    if (dir_names.empty() && lev_names.empty()) {
        return;
    }

    std::sort(dir_names.begin(), dir_names.end(),
              [](const std::string& a, const std::string& b) {
                  return strcmpi(a.c_str(), b.c_str()) < 0;
              });
    std::sort(lev_names.begin(), lev_names.end(),
              [](const std::string& a, const std::string& b) {
                  return strcmpi(a.c_str(), b.c_str()) < 0;
              });

    // Build nav entries
    int count = 0;
    int dotdot_index = -1;

    if (!current_subdir.empty()) {
        strcpy(NavEntriesLeft[count], "..");
        dotdot_index = count;
        count++;
    }

    for (const auto& name : dir_names) {
        if (count >= NavEntriesLeftMaxLength - 4) break;
        std::string display = name + "/";
        if (display.size() > NAV_ENTRY_TEXT_MAX_LENGTH) continue;
        strcpy(NavEntriesLeft[count++], display.c_str());
    }

    int first_lev_index = count;
    for (const auto& name : lev_names) {
        if (count >= NavEntriesLeftMaxLength - 4) break;
        if (name.size() > NAV_ENTRY_TEXT_MAX_LENGTH) continue;
        strcpy(NavEntriesLeft[count++], name.c_str());
    }

    if (count <= 0) {
        return;
    }

    // Position restore: only at root level, match against State->external_filename
    int previous_index = 0;
    if (current_subdir.empty()) {
        for (int i = first_lev_index; i < count; i++) {
            if (strcmpi(NavEntriesLeft[i], State->external_filename) == 0) {
                previous_index = i;
                break;
            }
        }
    }

    menu_nav val;
    val.search_pattern = SearchPattern::Sorted;
    val.selected_index = previous_index;

    if (current_subdir.empty()) {
        strcpy(val.title, "Select External File!");
    } else {
        std::string title = "lev/" + current_subdir;
        if (title.size() >= sizeof(val.title)) {
            title = title.substr(title.size() - sizeof(val.title) + 1);
        }
        strcpy(val.title, title.c_str());
    }

    val.setup(count);

    while (true) {
        int choice = val.navigate();
        if (choice < 0) {
            return;
        }

        nav_entry* entry = val.entry_left(choice);
        const char* entry_text = *entry;

        // ".." goes back to parent
        if (choice == dotdot_index) {
            return;
        }

        // Directory entry (ends with '/')
        size_t len = strlen(entry_text);
        if (len > 0 && entry_text[len - 1] == '/') {
            std::string subdir_name(entry_text, len - 1);
            browse_directory(current_subdir + subdir_name + "/");
            continue;
        }

        // .lev file: construct subpath for loading, bare name for state
        char load_path[40];
        std::string full_subpath = current_subdir + entry_text;
        if (full_subpath.size() >= sizeof(load_path)) {
            continue; // path too long, skip
        }
        strcpy(load_path, full_subpath.c_str());

        // Store bare filename in State (20-char limit)
        if (strlen(entry_text) < sizeof(State->external_filename)) {
            strcpy(State->external_filename, entry_text);
        }

        while (true) {
            loading_screen();
            if (!floadlevel_p(load_path)) {
                break;
            }
            Rec1->erase(load_path);
            Rec2->erase(load_path);
            int time = lejatszo(load_path, F1Pressed ? CameraMode::MapViewer : CameraMode::Normal);
            MenuPalette->set();
            char finish_msg[100] = "";
            update_top_ten(time, finish_msg, 0, load_path);
            if (menu_level(0, 0, finish_msg, load_path) == MenuLevel::Esc) {
                Rec1->erase(load_path);
                Rec2->erase(load_path);
                break;
            }
        }
    }
}

void menu_external_levels() {
    browse_directory("");
}
