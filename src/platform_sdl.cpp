#include "platform_impl.h"
#include "editor_dialog.h"
#include "eol_settings.h"
#include "EDITUJ.H"
#include "sound_engine.h"
#include "keys.h"
#ifndef MIYOO_MINI
#include "gl_renderer.h"
#endif
#include "main.h"
#include "M_PIC.H"
#include <SDL.h>
#include <sdl/scancodes_windows.h>
#include <cstring>

static SDL_Window* SDLWindow = nullptr;
static SDL_Surface* SDLSurfaceMain = nullptr;
static SDL_Surface* SDLSurfacePaletted = nullptr;

static bool LeftMouseDownPrev = false;
static bool RightMouseDownPrev = false;
static bool LeftMouseDown = false;
static bool RightMouseDown = false;

// SDL keyboard state and mappings
static const Uint8* SDLKeyState = nullptr;
static Keycode SDLToKeycode[SDL_NUM_SCANCODES];

#ifdef MIYOO_MINI
static SDL_Renderer* SDLRenderer = nullptr;
static SDL_Texture* SDLTexture = nullptr;
static SDL_Surface* SDLSurfaceRGB = nullptr;
static SDL_Color MiyooPalette[256];
#endif

void message_box(const char* text) {
    // As per docs, can be called even before SDL_Init
    // SDLWindow will either be a handle to the window, or nullptr if no parent
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Message", text, SDLWindow);
}

static unsigned char** SurfaceBuffer;

static void create_window(int window_pos_x, int window_pos_y, int width, int height) {
#ifdef MIYOO_MINI
    // Miyoo: always software renderer, no OpenGL
    // SDL_WINDOW_SHOWN is required by the mmiyoo video driver
    (void)window_pos_x; (void)window_pos_y;
    SDLWindow = SDL_CreateWindow("Elasto Mania",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height, SDL_WINDOW_SHOWN);
#else
    if (EolSettings->renderer() == RendererType::OpenGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }

    int window_flags = EolSettings->renderer() == RendererType::OpenGL ? SDL_WINDOW_OPENGL : 0;

    SDLWindow =
        SDL_CreateWindow("Elasto Mania", window_pos_x, window_pos_y, width, height, window_flags);
#endif
    if (!SDLWindow) {
        internal_error(SDL_GetError());
        return;
    }
}

static void initialize_renderer() {
#ifdef MIYOO_MINI
    // Create SDL renderer for Miyoo (hardware accelerated with vsync, fallback to software)
    SDLRenderer = SDL_CreateRenderer(SDLWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!SDLRenderer) {
        SDLRenderer = SDL_CreateRenderer(SDLWindow, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!SDLRenderer) {
        internal_error(SDL_GetError());
        return;
    }

    // Create streaming texture for frame output (RGB565 — the format the mmiyoo driver supports)
    SDLTexture = SDL_CreateTexture(SDLRenderer, SDL_PIXELFORMAT_RGB565,
                                   SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!SDLTexture) {
        internal_error(SDL_GetError());
        return;
    }

    // Create an RGB565 surface for manual palette→RGB conversion
    // (SDL_BlitSurface INDEX8→RGB565 may not work in this stripped SDL2 build)
    SDLSurfaceRGB = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 16,
                                          0xF800, 0x07E0, 0x001F, 0x0000);
    if (!SDLSurfaceRGB) {
        internal_error(SDL_GetError());
        return;
    }

    // Initialize palette to all black
    memset(MiyooPalette, 0, sizeof(MiyooPalette));

    SDLSurfaceMain = nullptr; // Not used on Miyoo
#else
    if (EolSettings->renderer() == RendererType::OpenGL) {
        if (gl_init(SDLWindow, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
            internal_error("Failed to initialize OpenGL renderer");
            return;
        }

        SDLSurfaceMain = nullptr; // Not used in GL mode
    } else {
        SDLSurfaceMain = SDL_GetWindowSurface(SDLWindow);
        if (!SDLSurfaceMain) {
            internal_error(SDL_GetError());
            return;
        }
    }
#endif
}

static void create_palette_surface() {
    SDLSurfacePaletted =
        SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, SDL_PIXELFORMAT_INDEX8);
    if (!SDLSurfacePaletted) {
        internal_error(SDL_GetError());
        return;
    }
}

static void initialize_keyboard_mappings() {
    // Initialize keyboard state and mappings
    SDLKeyState = SDL_GetKeyboardState(nullptr);

    // Map SDL scancodes to Keycodes
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        SDLToKeycode[i] = SDL_SCANCODE_UNKNOWN;
    }

    SDLToKeycode[SDL_SCANCODE_ESCAPE] = KEY_ESC;
    SDLToKeycode[SDL_SCANCODE_RETURN] = KEY_ENTER;
    SDLToKeycode[SDL_SCANCODE_KP_ENTER] = KEY_ENTER; // KP = Keypad

    SDLToKeycode[SDL_SCANCODE_UP] = KEY_UP;
    SDLToKeycode[SDL_SCANCODE_KP_8] = KEY_UP;
    SDLToKeycode[SDL_SCANCODE_DOWN] = KEY_DOWN;
    SDLToKeycode[SDL_SCANCODE_KP_2] = KEY_DOWN;
    SDLToKeycode[SDL_SCANCODE_LEFT] = KEY_LEFT;
    SDLToKeycode[SDL_SCANCODE_KP_4] = KEY_LEFT;
    SDLToKeycode[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
    SDLToKeycode[SDL_SCANCODE_KP_6] = KEY_RIGHT;

    SDLToKeycode[SDL_SCANCODE_PAGEUP] = KEY_PGUP;
    SDLToKeycode[SDL_SCANCODE_KP_9] = KEY_PGUP;
    SDLToKeycode[SDL_SCANCODE_PAGEDOWN] = KEY_PGDOWN;
    SDLToKeycode[SDL_SCANCODE_KP_3] = KEY_PGDOWN;

    SDLToKeycode[SDL_SCANCODE_DELETE] = KEY_DEL;
    SDLToKeycode[SDL_SCANCODE_KP_PERIOD] = KEY_DEL;
    SDLToKeycode[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;

    SDLToKeycode[SDL_SCANCODE_MINUS] = KEY_LEFT;
    SDLToKeycode[SDL_SCANCODE_KP_MINUS] = KEY_LEFT;
    SDLToKeycode[SDL_SCANCODE_EQUALS] = KEY_RIGHT;
    SDLToKeycode[SDL_SCANCODE_KP_PLUS] = KEY_RIGHT;

#ifdef MIYOO_MINI
    // Parasyte SDL2 maps Miyoo physical buttons to keyboard scancodes.
    // No joystick events — just keyboard.
    SDLToKeycode[SDL_SCANCODE_SPACE] = KEY_ENTER;     // A button = confirm
    SDLToKeycode[SDL_SCANCODE_LCTRL] = KEY_ESC;       // B button = cancel/back
    SDLToKeycode[SDL_SCANCODE_E] = KEY_PGUP;          // L1 = page up
    SDLToKeycode[SDL_SCANCODE_T] = KEY_PGDOWN;        // R1 = page down
    // START (SDL_SCANCODE_RETURN) → KEY_ENTER already mapped above
    // D-pad arrows (SDL_SCANCODE_UP/DOWN/LEFT/RIGHT) already mapped above
#endif
}

void platform_init() {
#ifdef MIYOO_MINI
    // Enable double buffering for the mmiyoo framebuffer driver (must be set before SDL_Init)
    SDL_setenv("SDL_MMIYOO_DOUBLE_BUFFER", "1", 1);
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        internal_error(SDL_GetError());
        return;
    }

    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);

    SurfaceBuffer = new unsigned char*[SCREEN_HEIGHT];

    create_window(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT);
    initialize_renderer();
    create_palette_surface();
    initialize_keyboard_mappings();

#ifdef MIYOO_MINI
    // Hide cursor on Miyoo (no mouse)
    SDL_ShowCursor(SDL_DISABLE);
#endif
}

void platform_recreate_window() {
    int x;
    int y;
    SDL_GetWindowPosition(SDLWindow, &x, &y);

    int width;
    int height;
    SDL_GetWindowSize(SDLWindow, &width, &height);

#ifndef MIYOO_MINI
    gl_cleanup();
#endif

    if (SDLSurfacePaletted) {
        SDL_FreeSurface(SDLSurfacePaletted);
        SDLSurfacePaletted = nullptr;
    }

#ifdef MIYOO_MINI
    if (SDLSurfaceRGB) {
        SDL_FreeSurface(SDLSurfaceRGB);
        SDLSurfaceRGB = nullptr;
    }
    if (SDLTexture) {
        SDL_DestroyTexture(SDLTexture);
        SDLTexture = nullptr;
    }
    if (SDLRenderer) {
        SDL_DestroyRenderer(SDLRenderer);
        SDLRenderer = nullptr;
    }
#else
    if (SDLSurfaceMain) {
        SDL_DestroyWindowSurface(SDLWindow);
        SDLSurfaceMain = nullptr;
    }
#endif

    SDL_DestroyWindow(SDLWindow);
    SDLWindow = nullptr;

    create_window(x, y, width, height);
    initialize_renderer();
    create_palette_surface();
}

long long get_milliseconds() { return SDL_GetTicks64(); }

bool has_window() { return SDLWindow != nullptr; }

static bool SurfaceLocked = false;

unsigned char** lock_backbuffer(bool flipped) {
    if (SurfaceLocked) {
        internal_error("lock_backbuffer SurfaceLocked!");
    }
    SurfaceLocked = true;

    unsigned char* row = (unsigned char*)SDLSurfacePaletted->pixels;
    if (flipped) {
        // Set the row buffer bottom-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            SurfaceBuffer[SCREEN_HEIGHT - 1 - y] = row;
            row += SDLSurfacePaletted->w;
        }
    } else {
        // Set the row buffer top-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            SurfaceBuffer[y] = row;
            row += SDLSurfacePaletted->w;
        }
    }

    return SurfaceBuffer;
}

void unlock_backbuffer() {
    if (!SurfaceLocked) {
        internal_error("unlock_backbuffer !SurfaceLocked!");
    }
    SurfaceLocked = false;

#ifdef MIYOO_MINI
    // Miyoo: manually convert paletted surface to RGB565, then upload to texture and present.
    // We do manual conversion because SDL_BlitSurface INDEX8→RGB565 may not work
    // in the stripped mmiyoo SDL2 build.
    {
        const unsigned char* src = (const unsigned char*)SDLSurfacePaletted->pixels;
        Uint16* dst = (Uint16*)SDLSurfaceRGB->pixels;
        int src_pitch = SDLSurfacePaletted->pitch;
        int dst_pitch = SDLSurfaceRGB->pitch / 2; // pitch in Uint16 units
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            const unsigned char* src_row = src + y * src_pitch;
            Uint16* dst_row = dst + y * dst_pitch;
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                const SDL_Color& c = MiyooPalette[src_row[x]];
                dst_row[x] = ((Uint16)(c.r >> 3) << 11) |
                             ((Uint16)(c.g >> 2) << 5) |
                             ((Uint16)(c.b >> 3));
            }
        }
    }
    SDL_UpdateTexture(SDLTexture, NULL, SDLSurfaceRGB->pixels, SDLSurfaceRGB->pitch);
    SDL_RenderClear(SDLRenderer);
    SDL_RenderCopy(SDLRenderer, SDLTexture, NULL, NULL);
    SDL_RenderPresent(SDLRenderer);
#else
    if (EolSettings->renderer() == RendererType::OpenGL) {
        gl_upload_frame((unsigned char*)SDLSurfacePaletted->pixels);
        gl_present();
        SDL_GL_SwapWindow(SDLWindow);
    } else {
        SDL_BlitSurface(SDLSurfacePaletted, NULL, SDLSurfaceMain, NULL);
        SDL_UpdateWindowSurface(SDLWindow);
    }
#endif
}

unsigned char** lock_frontbuffer(bool flipped) {
    if (SurfaceLocked) {
        internal_error("lock_frontbuffer SurfaceLocked!");
    }

    return lock_backbuffer(flipped);
}

void unlock_frontbuffer() {
    if (!SurfaceLocked) {
        internal_error("unlock_frontbuffer !SurfaceLocked!");
    }

    unlock_backbuffer();
}

palette::palette(unsigned char* palette_data) {
    SDL_Color* pal = new SDL_Color[256];
    for (int i = 0; i < 256; i++) {
        pal[i].r = palette_data[3 * i];
        pal[i].g = palette_data[3 * i + 1];
        pal[i].b = palette_data[3 * i + 2];
        pal[i].a = 0xFF;
    }
    data = (void*)pal;
}

palette::~palette() { delete[] (SDL_Color*)data; }

void palette::set() {
#ifdef MIYOO_MINI
    // Store palette for manual RGB565 conversion in unlock_backbuffer
    memcpy(MiyooPalette, (const SDL_Color*)data, 256 * sizeof(SDL_Color));
    // Also set on the surface palette (needed for any SDL blit paths)
    SDL_SetPaletteColors(SDLSurfacePaletted->format->palette, (const SDL_Color*)data, 0, 256);
#else
    if (EolSettings->renderer() == RendererType::OpenGL) {
        gl_update_palette(data);
    } else {
        SDL_SetPaletteColors(SDLSurfacePaletted->format->palette, (const SDL_Color*)data, 0, 256);
    }
#endif
}

void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            // Exit request probably sent by user to terminate program
            if (InEditor && Valtozott) {
                // Disallow exiting if unsaved changes in editor
                break;
            }
            quit();
            break;
        case SDL_WINDOWEVENT:
            // Force editor redraw if focus gained/lost to fix editor sometimes blanking
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                invalidateegesz();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                invalidateegesz();
                break;
            }
            break;
        case SDL_KEYDOWN: {
            SDL_Scancode scancode = event.key.keysym.scancode;
            Keycode keycode = SDLToKeycode[scancode];
            if (keycode == SDL_SCANCODE_UNKNOWN) {
                break; // Not a control mapping - delivered through text input events.
            }

            if (event.key.repeat) {
                bool allow_repeat = (keycode != KEY_ESC && keycode != KEY_ENTER);
                if (!allow_repeat) {
                    break;
                }
            }

            add_key_to_buffer(keycode);
            break;
        }
        case SDL_TEXTINPUT:
#ifndef MIYOO_MINI
            // Skip text input on Miyoo — no real keyboard, parasyte SDL2 may
            // generate spurious text events from button presses
            add_text_to_buffer(event.text.text);
#endif
            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.y > 0) {
                add_key_to_buffer(KEY_UP);
            } else if (event.wheel.y < 0) {
                add_key_to_buffer(KEY_DOWN);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = false;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = false;
            }
            break;
        }
    }
}

void hide_cursor() { SDL_ShowCursor(SDL_DISABLE); }
void show_cursor() { SDL_ShowCursor(SDL_ENABLE); }

void get_mouse_position(int* x, int* y) { SDL_GetMouseState(x, y); }
void set_mouse_position(int x, int y) { SDL_WarpMouseInWindow(NULL, x, y); }

bool left_mouse_clicked() {
    handle_events();
    bool click = !LeftMouseDownPrev && LeftMouseDown;
    LeftMouseDownPrev = LeftMouseDown;
    return click;
}

bool right_mouse_clicked() {
    handle_events();
    bool click = !RightMouseDownPrev && RightMouseDown;
    RightMouseDownPrev = RightMouseDown;
    return click;
}

bool is_key_down(DikScancode code) {
    if (code < 0 || code >= MaxKeycode) {
        internal_error("code out of range in is_key_down()!");
        return false;
    }

    SDL_Scancode sdl_code = windows_scancode_table[code];

    return SDLKeyState[sdl_code] != 0;
}

bool is_fullscreen() {
    Uint32 flags = SDL_GetWindowFlags(SDLWindow);
    return flags & SDL_WINDOW_FULLSCREEN;
}

static SDL_AudioDeviceID SDLAudioDevice;
static bool SDLSoundInitialized = false;

static void audio_callback(void* udata, Uint8* stream, int len) {
    sound_mixer((short*)stream, len / 2);
}

void init_sound() {
    if (SDLSoundInitialized) {
        internal_error("Sound already initialized!");
    }
    SDLSoundInitialized = true;

    SDL_AudioSpec desired_spec;
    memset(&desired_spec, 0, sizeof(desired_spec));
    desired_spec.callback = audio_callback;
    desired_spec.freq = 11025;
    desired_spec.channels = 1;
    desired_spec.samples = 512;
    desired_spec.format = AUDIO_S16LSB;

#ifdef MIYOO_MINI
    // Graceful audio fallback on Miyoo — don't crash if audio fails
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        return; // Audio unavailable, continue without sound
    }
    SDL_AudioSpec obtained_spec;
    SDLAudioDevice = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (SDLAudioDevice == 0) {
        return; // No audio device, continue without sound
    }
    if (obtained_spec.format != desired_spec.format) {
        SDL_CloseAudioDevice(SDLAudioDevice);
        SDLAudioDevice = 0;
        return; // Wrong format, continue without sound
    }
    SDL_PauseAudioDevice(SDLAudioDevice, 0);
#else
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        internal_error("Failed to initialize audio subsystem", SDL_GetError());
    }
    SDL_AudioSpec obtained_spec;
    SDLAudioDevice = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (SDLAudioDevice == 0) {
        internal_error("Failed to open audio device", SDL_GetError());
    }
    if (obtained_spec.format != desired_spec.format) {
        internal_error("Failed to get correct audio format");
    }
    SDL_PauseAudioDevice(SDLAudioDevice, 0);
#endif
}
