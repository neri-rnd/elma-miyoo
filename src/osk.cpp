#ifdef MIYOO_MINI

#include "osk.h"
#include "keys.h"
#include "menu_pic.h"
#include "M_PIC.H"
#include "abc8.h"
#include "pic8.h"
#include "platform_impl.h"
#include <cstring>
#include <cstdio>

// Keyboard grid layout
static const char* const KeyboardRows[] = {
    "ABCDEFGHIJKLM",
    "NOPQRSTUVWXYZ",
    "0123456789",
};
static constexpr int NUM_CHAR_ROWS = 3;

// Special button row (row index 3)
static constexpr int SPECIAL_ROW = 3;
static constexpr int SPECIAL_SPACE = 0;
static constexpr int SPECIAL_BACK = 1;
static constexpr int SPECIAL_OK = 2;
static constexpr int NUM_SPECIALS = 3;

// Layout constants — matches menu_nav spacing
static constexpr int PROMPT_Y = 40;
static constexpr int INPUT_Y = 85;
static constexpr int GRID_Y = 150;
static constexpr int GRID_ROW_DY = 38;
static constexpr int SPECIAL_Y = GRID_Y + NUM_CHAR_ROWS * GRID_ROW_DY + 10;
static constexpr int HINT_Y = 420;

// Spacing between characters in grid (3 spaces worth)
static const char* CHAR_SEP = " ";

// Build a spaced-out row string: "A   B   C   ..."
static int build_row_string(char* out, int out_size, const char* row) {
    int pos = 0;
    int len = (int)strlen(row);
    for (int i = 0; i < len && pos < out_size - 4; i++) {
        if (i > 0) {
            int sep_len = (int)strlen(CHAR_SEP);
            for (int s = 0; s < sep_len && pos < out_size - 2; s++)
                out[pos++] = CHAR_SEP[s];
        }
        out[pos++] = row[i];
    }
    out[pos] = 0;
    return pos;
}

// Compute x offset of the Nth character in a spaced row string
static int char_x_offset(const char* row, int col) {
    char prefix[128];
    int pos = 0;
    for (int i = 0; i < col; i++) {
        if (i > 0) {
            int sep_len = (int)strlen(CHAR_SEP);
            for (int s = 0; s < sep_len; s++)
                prefix[pos++] = CHAR_SEP[s];
        }
        prefix[pos++] = row[i];
    }
    if (col > 0) {
        int sep_len = (int)strlen(CHAR_SEP);
        for (int s = 0; s < sep_len; s++)
            prefix[pos++] = CHAR_SEP[s];
    }
    prefix[pos] = 0;
    return MenuFont->len(prefix);
}

bool osk_get_text(char* result, int max_len, const char* prompt) {
    int cursor_row = 0;
    int cursor_col = 0;
    int text_len = (int)strlen(result);

    empty_keypress_buffer();

    // Pre-build row strings and compute their widths for centering
    char row_strings[NUM_CHAR_ROWS][128];
    int row_widths[NUM_CHAR_ROWS];
    int row_x[NUM_CHAR_ROWS]; // left x for each row (in 640-wide coords)
    for (int r = 0; r < NUM_CHAR_ROWS; r++) {
        build_row_string(row_strings[r], sizeof(row_strings[r]), KeyboardRows[r]);
        row_widths[r] = MenuFont->len(row_strings[r]);
        row_x[r] = 320 - row_widths[r] / 2;
    }

    // Build special row labels and compute positions
    const char* special_labels[] = { "SPACE", "BACK", "OK" };
    char special_row[128];
    {
        int pos = 0;
        for (int i = 0; i < NUM_SPECIALS; i++) {
            if (i > 0) {
                const char* sep = "     "; // wider separation for specials
                int sep_len = (int)strlen(sep);
                for (int s = 0; s < sep_len; s++)
                    special_row[pos++] = sep[s];
            }
            int lbl_len = (int)strlen(special_labels[i]);
            for (int j = 0; j < lbl_len; j++)
                special_row[pos++] = special_labels[i][j];
        }
        special_row[pos] = 0;
    }
    int special_row_width = MenuFont->len(special_row);
    int special_row_x = 320 - special_row_width / 2;

    // Pre-compute x offsets for each special button
    int special_offsets[NUM_SPECIALS];
    {
        for (int i = 0; i < NUM_SPECIALS; i++) {
            char prefix[128];
            int pp = 0;
            for (int j = 0; j < i; j++) {
                if (j > 0) {
                    const char* sep = "     ";
                    int sep_len = (int)strlen(sep);
                    for (int s = 0; s < sep_len; s++)
                        prefix[pp++] = sep[s];
                }
                int lbl_len = (int)strlen(special_labels[j]);
                for (int k = 0; k < lbl_len; k++)
                    prefix[pp++] = special_labels[j][k];
            }
            if (i > 0) {
                const char* sep = "     ";
                int sep_len = (int)strlen(sep);
                for (int s = 0; s < sep_len; s++)
                    prefix[pp++] = sep[s];
            }
            prefix[pp] = 0;
            special_offsets[i] = MenuFont->len(prefix);
        }
    }

    while (true) {
        // Clamp cursor_col to row length
        if (cursor_row < NUM_CHAR_ROWS) {
            int rlen = (int)strlen(KeyboardRows[cursor_row]);
            if (cursor_col >= rlen) cursor_col = rlen - 1;
        } else {
            if (cursor_col >= NUM_SPECIALS) cursor_col = NUM_SPECIALS - 1;
        }
        if (cursor_col < 0) cursor_col = 0;

        // Handle input
        while (has_keypress()) {
            Keycode c = get_keypress();

            if (c == KEY_UP) {
                if (cursor_row > 0) cursor_row--;
            } else if (c == KEY_DOWN) {
                if (cursor_row < SPECIAL_ROW) cursor_row++;
            } else if (c == KEY_LEFT) {
                if (cursor_col > 0) cursor_col--;
            } else if (c == KEY_RIGHT) {
                int max_col;
                if (cursor_row < NUM_CHAR_ROWS)
                    max_col = (int)strlen(KeyboardRows[cursor_row]) - 1;
                else
                    max_col = NUM_SPECIALS - 1;
                if (cursor_col < max_col) cursor_col++;
            } else if (c == KEY_ENTER) {
                // A button or START = select
                if (cursor_row < NUM_CHAR_ROWS) {
                    int rlen = (int)strlen(KeyboardRows[cursor_row]);
                    if (cursor_col < rlen && text_len < max_len) {
                        result[text_len] = KeyboardRows[cursor_row][cursor_col];
                        text_len++;
                        result[text_len] = 0;
                    }
                } else {
                    if (cursor_col == SPECIAL_SPACE) {
                        if (text_len < max_len) {
                            result[text_len] = ' ';
                            text_len++;
                            result[text_len] = 0;
                        }
                    } else if (cursor_col == SPECIAL_BACK) {
                        if (text_len > 0) {
                            text_len--;
                            result[text_len] = 0;
                        }
                    } else if (cursor_col == SPECIAL_OK) {
                        if (text_len > 0) return true;
                    }
                }
            } else if (c == KEY_ESC) {
                // B button = delete last char, or cancel if empty
                if (text_len > 0) {
                    text_len--;
                    result[text_len] = 0;
                } else {
                    result[0] = 0;
                    return false;
                }
            }
        }

        // Re-clamp cursor_col after input — row may have changed
        if (cursor_row < NUM_CHAR_ROWS) {
            int rlen = (int)strlen(KeyboardRows[cursor_row]);
            if (cursor_col >= rlen) cursor_col = rlen - 1;
        } else {
            if (cursor_col >= NUM_SPECIALS) cursor_col = NUM_SPECIALS - 1;
        }
        if (cursor_col < 0) cursor_col = 0;

        // --- Render ---
        // Tile menu background onto BufferMain (same as main menu)
        tile_menu_background(BufferMain);

        // Prompt
        MenuFont->write_centered(BufferMain, SCREEN_WIDTH / 2, PROMPT_Y, prompt);

        // Current input text with cursor
        char display[64];
        if (text_len < max_len)
            snprintf(display, sizeof(display), "%s_", result);
        else
            snprintf(display, sizeof(display), "%s", result);
        MenuFont->write_centered(BufferMain, SCREEN_WIDTH / 2, INPUT_Y, display);

        // Draw keyboard character rows
        for (int r = 0; r < NUM_CHAR_ROWS; r++) {
            int y = GRID_Y + r * GRID_ROW_DY;
            MenuFont->write(BufferMain, row_x[r], y, row_strings[r]);
        }

        // Draw special row
        MenuFont->write(BufferMain, special_row_x, SPECIAL_Y, special_row);

        // Draw highlight box behind selected character
        {
            int sel_x, sel_y, sel_w;
            if (cursor_row < NUM_CHAR_ROWS) {
                sel_y = GRID_Y + cursor_row * GRID_ROW_DY;
                sel_x = row_x[cursor_row] + char_x_offset(KeyboardRows[cursor_row], cursor_col);
                // Width of one character
                char ch[2] = { KeyboardRows[cursor_row][cursor_col], 0 };
                sel_w = MenuFont->len(ch);
            } else {
                sel_y = SPECIAL_Y;
                sel_x = special_row_x + special_offsets[cursor_col];
                sel_w = MenuFont->len(special_labels[cursor_col]);
            }
            // Draw a dark box behind the selected item for contrast
            int pad = 3;
            int font_h = 22; // approximate menu font height
            BufferMain->fill_box(sel_x - pad, sel_y - pad,
                                 sel_x + sel_w + pad, sel_y + font_h + pad,
                                 BLACK_PALETTE_ID);
            // Re-draw the selected text on top of the dark box
            if (cursor_row < NUM_CHAR_ROWS) {
                char ch[2] = { KeyboardRows[cursor_row][cursor_col], 0 };
                MenuFont->write(BufferMain, sel_x, sel_y, ch);
            } else {
                MenuFont->write(BufferMain, sel_x, sel_y, special_labels[cursor_col]);
            }
        }

        // Instructions at bottom
        MenuFont->write_centered(BufferMain, SCREEN_WIDTH / 2, HINT_Y,
                                 "A:Select  B:Delete/Cancel");

        // Display
        MenuPalette->set();
        bltfront(BufferMain);
    }
}

#endif // MIYOO_MINI
