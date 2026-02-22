#include "headers/main.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <dlfcn.h>
#endif

typedef struct {
    char **items;
    bool *is_dir;
    size_t count;
    size_t cap;
} BrowserList;

#if defined(__linux__)
typedef void X11Display;
typedef X11Display *(*x11_open_display_fn)(const char *);
typedef int (*x11_close_display_fn)(X11Display *);

static bool can_open_x11_display(const char *display_name) {
    if (!display_name || !*display_name) return false;

    void *libx11 = dlopen("libX11.so.6", RTLD_LAZY);
    if (!libx11) return true;

    x11_open_display_fn xopen = (x11_open_display_fn)dlsym(libx11, "XOpenDisplay");
    x11_close_display_fn xclose = (x11_close_display_fn)dlsym(libx11, "XCloseDisplay");

    bool ok = true;
    if (xopen && xclose) {
        X11Display *dpy = xopen(display_name);
        ok = (dpy != NULL);
        if (dpy) xclose(dpy);
    }

    dlclose(libx11);
    return ok;
}
#endif

static void ensure_log_dir(void) {
    if (mkdir("log", 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create log directory 'log': %s\n", strerror(errno));
    }
}

static bool has_nes_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot != NULL && strcmp(dot, ".nes") == 0;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static bool browser_list_push(BrowserList *list, const char *item, bool is_dir) {
    if (list -> count == list -> cap) {
        size_t new_cap = list -> cap ? list -> cap * 2 : 64;
        char **new_items = (char **)malloc(new_cap * sizeof(char *));
        bool *new_is_dir = (bool *)malloc(new_cap * sizeof(bool));
        if (!new_items || !new_is_dir) {
            free(new_items);
            free(new_is_dir);
            return false;
        }
        for (size_t i = 0; i < list -> count; ++i) {
            new_items[i] = list -> items[i];
            new_is_dir[i] = list -> is_dir[i];
        }
        free(list -> items);
        free(list -> is_dir);
        list -> items = new_items;
        list -> is_dir = new_is_dir;
        list -> cap = new_cap;
    }

    list -> items[list -> count] = xstrdup(item);
    if (!list -> items[list -> count]) return false;
    list -> is_dir[list -> count] = is_dir;
    list -> count++;
    return true;
}

static void browser_list_clear(BrowserList *list) {
    for (size_t i = 0; i < list -> count; ++i) free(list -> items[i]);
    list -> count = 0;
}

static void browser_list_free(BrowserList *list) {
    browser_list_clear(list);
    free(list -> items);
    free(list -> is_dir);
    list -> items = NULL;
    list -> is_dir = NULL;
    list -> cap = 0;
}

static void path_parent(const char *path, char *out, size_t out_len) {
    if (!path || !*path) {
        snprintf(out, out_len, "/");
        return;
    }

    if (strcmp(path, "/") == 0) {
        snprintf(out, out_len, "/");
        return;
    }

    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);

    size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    while (n > 1 && tmp[n - 1] != '/') tmp[--n] = '\0';
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';

    snprintf(out, out_len, "%s", n == 0 ? "/" : tmp);
}

static bool load_directory(const char *dir, BrowserList *list, char *status, size_t status_len) {
    DIR *dp = opendir(dir);
    browser_list_clear(list);

    if (!dp) {
        snprintf(status, status_len, "Cannot open: %s", dir);
        return false;
    }

    if (strcmp(dir, "/") != 0) {
        if (!browser_list_push(list, "..", true)) {
            closedir(dp);
            snprintf(status, status_len, "Out of memory");
            return false;
        }
    }

    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (strcmp(ent -> d_name, ".") == 0 || strcmp(ent -> d_name, "..") == 0) continue;

        char full[PATH_MAX];
        if (strcmp(dir, "/") == 0) snprintf(full, sizeof(full), "/%s", ent -> d_name);
        else snprintf(full, sizeof(full), "%s/%s", dir, ent -> d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (!browser_list_push(list, ent -> d_name, true)) {
                closedir(dp);
                snprintf(status, status_len, "Out of memory");
                return false;
            }
        } else if (S_ISREG(st.st_mode) && has_nes_extension(ent -> d_name)) {
            if (!browser_list_push(list, ent -> d_name, false)) {
                closedir(dp);
                snprintf(status, status_len, "Out of memory");
                return false;
            }
        }
    }

    closedir(dp);

    if (list -> count == 0) {
        snprintf(status, status_len, "No subfolders or .nes files in this directory");
    } else {
        snprintf(status, status_len, "Use arrows/mouse. Enter to open/select, Backspace to go up");
    }

    return true;
}

static void sort_directory_entries(BrowserList *list) {
    if (list -> count <= 2) return;

    size_t start = (list -> count > 0 && strcmp(list -> items[0], "..") == 0) ? 1 : 0;
    for (size_t i = start; i < list -> count; ++i) {
        for (size_t j = i + 1; j < list -> count; ++j) {
            bool di = list -> is_dir[i];
            bool dj = list -> is_dir[j];
            int cmp = strcmp(list -> items[i], list -> items[j]);
            bool swap = false;
            if (di != dj) swap = di < dj;
            else if (cmp > 0) swap = true;
            if (swap) {
                char *tmp_item = list -> items[i];
                bool tmp_dir = list -> is_dir[i];
                list -> items[i] = list -> items[j];
                list -> is_dir[i] = list -> is_dir[j];
                list -> items[j] = tmp_item;
                list -> is_dir[j] = tmp_dir;
            }
        }
    }
}

static bool build_selected_path(const char *cwd, const char *name, char *out, size_t out_len) {
    if (strcmp(name, "..") == 0) {
        path_parent(cwd, out, out_len);
        return true;
    }
    if (strcmp(cwd, "/") == 0) {
        return snprintf(out, out_len, "/%s", name) > 0;
    }
    return snprintf(out, out_len, "%s/%s", cwd, name) > 0;
}

static bool choose_rom_gui(char *out_path, size_t out_path_len) {
    if (!out_path || out_path_len == 0) return false;

#if defined(__linux__)
    const char *display = getenv("DISPLAY");
    if (!display || !*display || !can_open_x11_display(display)) {
        fprintf(stderr, "Cannot open graphical selector: invalid DISPLAY.\n");
        return false;
    }
#endif

    char cwd[PATH_MAX];
    const char *home = getenv("HOME");
    if (home && *home) snprintf(cwd, sizeof(cwd), "%s", home);
    else snprintf(cwd, sizeof(cwd), "/");

    BrowserList list = {0};
    char status[256] = {0};
    bool ok = load_directory(cwd, &list, status, sizeof(status));
    sort_directory_entries(&list);
    if (!ok) {
        browser_list_free(&list);
        return false;
    }

    const int win_w = 1100;
    const int win_h = 740;
    const int row_h = 28;
    const int top = 130;
    const int left = 36;
    const int view_h = 520;
    const int visible_rows = view_h / row_h;
    int selected = 0;
    int scroll = 0;
    bool confirmed = false;

    InitWindow(win_w, win_h, "EasyNES - Browse ROM");
    if (!IsWindowReady()) {
        browser_list_free(&list);
        fprintf(stderr, "Failed to initialize ROM selector window.\n");
        return false;
    }
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_BACKSPACE)) {
            char parent[PATH_MAX];
            path_parent(cwd, parent, sizeof(parent));
            snprintf(cwd, sizeof(cwd), "%s", parent);
            load_directory(cwd, &list, status, sizeof(status));
            sort_directory_entries(&list);
            selected = 0;
            scroll = 0;
        }

        int wheel = (int)GetMouseWheelMove();
        if (wheel != 0) {
            scroll -= wheel;
            if (scroll < 0) scroll = 0;
            int max_scroll = (int)list.count - visible_rows;
            if (max_scroll < 0) max_scroll = 0;
            if (scroll > max_scroll) scroll = max_scroll;
        }

        if (IsKeyPressed(KEY_DOWN) && selected < (int)list.count - 1) selected++;
        if (IsKeyPressed(KEY_UP) && selected > 0) selected--;
        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;

        bool activate = IsKeyPressed(KEY_ENTER);
        Vector2 mouse = GetMousePosition();
        Rectangle list_box = (Rectangle){(float)left, (float)top, (float)(win_w - 2 * left), (float)view_h};

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, list_box)) {
            int idx = scroll + (int)((mouse.y - (float)top) / (float)row_h);
            if (idx >= 0 && idx < (int)list.count) {
                if (idx == selected) activate = true;
                selected = idx;
            }
        }

        if (activate && list.count > 0 && selected >= 0 && selected < (int)list.count) {
            char full[PATH_MAX];
            if (!build_selected_path(cwd, list.items[selected], full, sizeof(full))) {
                snprintf(status, sizeof(status), "Path too long");
            } else if (list.is_dir[selected]) {
                snprintf(cwd, sizeof(cwd), "%s", full);
                load_directory(cwd, &list, status, sizeof(status));
                sort_directory_entries(&list);
                selected = 0;
                scroll = 0;
            } else {
                snprintf(out_path, out_path_len, "%s", full);
                confirmed = true;
                break;
            }
        }

        BeginDrawing();
        ClearBackground((Color){18, 22, 30, 255});

        DrawText("EasyNES ROM Browser", left, 22, 38, RAYWHITE);
        DrawText("Navigate folders and pick a .nes from anywhere", left, 70, 22, LIGHTGRAY);
        DrawText(TextFormat("Current path: %s", cwd), left, 98, 20, (Color){170, 200, 230, 255});
        DrawRectangleLinesEx(list_box, 2.0f, (Color){90, 110, 150, 255});

        for (int i = 0; i < visible_rows; ++i) {
            int idx = scroll + i;
            if (idx >= (int)list.count) break;
            int y = top + i * row_h;
            bool is_selected = (idx == selected);
            if (is_selected) {
                DrawRectangle(left + 2, y + 2, win_w - 2 * left - 4, row_h - 3, (Color){50, 95, 170, 255});
            }
            const char *tag = list.is_dir[idx] ? "[DIR]" : "[NES]";
            DrawText(tag, left + 10, y + 6, 18, list.is_dir[idx] ? SKYBLUE : GOLD);
            DrawText(list.items[idx], left + 80, y + 6, 18, is_selected ? RAYWHITE : LIGHTGRAY);
        }

        DrawText(status, left, win_h - 44, 18, GRAY);
        EndDrawing();
    }

    CloseWindow();
    browser_list_free(&list);
    return confirmed;
}

int main(int argc, char const *argv[]) {
    ensure_log_dir();
    log_init("log/easynes.log");

    char selected_rom[PATH_MAX] = {0};
    const char *rom_path = NULL;

    if (argc == 2) {
        rom_path = argv[1];
    } else {
        if (!choose_rom_gui(selected_rom, sizeof(selected_rom))) {
            log_stop();
            return EXIT_FAILURE;
        }
        rom_path = selected_rom;
    }

    Emulator emu;
    emulator_init(&emu);
    emulator_run(&emu, rom_path);
    emulator_dispose(&emu);

    log_stop();
    return EXIT_SUCCESS;
}
