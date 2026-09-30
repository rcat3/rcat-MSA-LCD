/*
 * The images that can be shown: images loaded from storage (the SD card)
 * followed by the ones built into the firmware.
 *
 * Images from storage are loaded into memory once at startup, so showing
 * them is as quick as the built-in ones:
 * - Static images (PNG, JPG, BMP) are decoded, scaled for the screen with
 *   the same rule as tools/img2c.py, and kept as RGB565.
 * - GIFs are read into memory as they are and decoded while they play,
 *   scaled while they're drawn. That costs some time on every frame, so
 *   tools/prepare_sd.py can write screen sized copies instead.
 *
 * Files written by tools/prepare_sd.py contain "rcat-msa-lcd:prepared:<size>"
 * near the start. They're already sized for a <size> screen, which is used
 * as the reference instead of the usual sizing rule, so they aren't scaled
 * again.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Loading from storage needs a filesystem (opendir, stat, ...). Platforms
// that have one define RCAT_HAVE_FILESYSTEM.
#if RCAT_HAVE_FILESYSTEM
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#endif

#include "lvgl.h"
#include "media/image_list.h"
#include "media/media_library.h"

// Images are drawn for a 240x240 screen and scaled up for bigger ones.
// Anything bigger than that is treated as full screen artwork.
#define DESIGN_SIZE         240

#define MAX_STORAGE_IMAGES  32
#define MAX_FOLDER_LEN      64
#define MAX_PATH_LEN        192

// Decoding needs the whole image in memory at 4 bytes per pixel.
#define MAX_SOURCE_PIXELS   (1024 * 1024)

// LVGL's drive letter for the C library file functions (LV_USE_FS_STDIO)
#define LVGL_DRIVE          "S:"

#define PREPARED_MARKER     "rcat-msa-lcd:prepared:"
#define MARKER_SEARCH_BYTES 4096

static media_item_t *items;
static size_t item_count;

static void set_name(media_item_t *item, const char *file_name)
{
    snprintf(item->name, sizeof(item->name), "%s", file_name);
    char *dot = strrchr(item->name, '.');
    if (dot != NULL)
    {
        *dot = '\0';
    }
}

void media_init(void)
{
    items = lv_malloc(builtin_image_count * sizeof(media_item_t));
    LV_ASSERT_MALLOC(items);

    for (size_t i = 0; i < builtin_image_count; i++)
    {
        snprintf(items[i].name, sizeof(items[i].name), "%s", builtin_images[i].name);
        items[i].src = builtin_images[i].img;
        items[i].is_gif = builtin_images[i].is_gif;
        items[i].scale = LV_SCALE_NONE;     // converted for this screen at build time
    }
    item_count = builtin_image_count;
}

size_t media_count(void)
{
    return item_count;
}

const media_item_t *media_get(size_t index)
{
    return (index < item_count) ? &items[index] : NULL;
}

int media_find(const char *name)
{
    for (size_t i = 0; i < item_count; i++)
    {
        if (strcmp(items[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

#if RCAT_HAVE_FILESYSTEM

static bool has_extension(const char *file_name, const char *ext)
{
    size_t len = strlen(file_name);
    size_t ext_len = strlen(ext);
    return len > ext_len && strcasecmp(file_name + len - ext_len, ext) == 0;
}

static bool is_gif_file(const char *file_name)
{
    return has_extension(file_name, ".gif");
}

static bool is_image_file(const char *file_name)
{
    // Skip hidden files, including the "._name" files macOS leaves on cards.
    return file_name[0] != '.'
        && (is_gif_file(file_name)
            || has_extension(file_name, ".png")
            || has_extension(file_name, ".jpg")
            || has_extension(file_name, ".jpeg")
            || has_extension(file_name, ".bmp"));
}

/* The screen size a file was prepared for by tools/prepare_sd.py, or 0. */
static int32_t prepared_size_in(const uint8_t *data, size_t len)
{
    size_t marker_len = strlen(PREPARED_MARKER);
    for (size_t i = 0; i + marker_len < len; i++)
    {
        if (memcmp(data + i, PREPARED_MARKER, marker_len) == 0)
        {
            return atoi((const char *)data + i + marker_len);
        }
    }
    return 0;
}

static int32_t prepared_size_of_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return 0;
    }
    uint8_t *head = lv_malloc(MARKER_SEARCH_BYTES + 1);
    int32_t size = 0;
    if (head != NULL)
    {
        size_t len = fread(head, 1, MARKER_SEARCH_BYTES, f);
        head[len] = '\0';     // so atoi stops at the end
        size = prepared_size_in(head, len);
        lv_free(head);
    }
    fclose(f);
    return size;
}

/* The screen size an image of this size (or prepared for prepared_size) was
 * made for: 240 for images up to 240x240, otherwise its own size. */
static int32_t reference_size(int32_t w, int32_t h, int32_t prepared_size)
{
    if (prepared_size > 0)
    {
        return prepared_size;
    }
    return LV_MAX(DESIGN_SIZE, LV_MAX(w, h));
}

static int compare_names(const void *a, const void *b)
{
    return strcasecmp(*(const char * const *)a, *(const char * const *)b);
}

/* Read a whole GIF into memory, wrapped in an image descriptor for lv_gif,
 * and work out the scale to show it at. */
static const void *load_gif(const char *path, int32_t screen_size, uint16_t *scale)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return NULL;
    }

    struct stat st;
    uint8_t *data = NULL;
    lv_image_dsc_t *dsc = NULL;

    if (fstat(fileno(f), &st) == 0 && st.st_size > 10
        && (data = lv_malloc(st.st_size)) != NULL
        && fread(data, 1, st.st_size, f) == (size_t)st.st_size
        && memcmp(data, "GIF", 3) == 0
        && (dsc = lv_malloc_zeroed(sizeof(*dsc))) != NULL)
    {
        dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc->header.cf = LV_COLOR_FORMAT_RAW;
        dsc->header.w = data[6] | (data[7] << 8);
        dsc->header.h = data[8] | (data[9] << 8);
        dsc->data_size = st.st_size;
        dsc->data = data;

        size_t search_len = LV_MIN((size_t)st.st_size - 1, (size_t)MARKER_SEARCH_BYTES);
        int32_t reference = reference_size(dsc->header.w, dsc->header.h, prepared_size_in(data, search_len));
        *scale = (uint16_t)(LV_SCALE_NONE * screen_size / reference);
    }
    else
    {
        lv_free(data);
    }
    fclose(f);
    return dsc;
}

/* Draw an image src_w x src_h in size, scaled about its center, into the
 * middle of buf. */
static void draw_into(lv_draw_buf_t *buf, const void *src, int32_t src_w, int32_t src_h,
                      int32_t scale_x, int32_t scale_y)
{
    lv_obj_t *canvas = lv_canvas_create(lv_layer_sys());
    lv_obj_set_hidden(canvas, true);
    lv_canvas_set_draw_buf(canvas, buf);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_image_dsc_t dsc;
    lv_draw_image_dsc_init(&dsc);
    dsc.src = src;
    dsc.pivot.x = src_w / 2;
    dsc.pivot.y = src_h / 2;
    dsc.scale_x = scale_x;
    dsc.scale_y = scale_y;

    lv_area_t area;
    area.x1 = ((int32_t)buf->header.w - src_w) / 2;
    area.y1 = ((int32_t)buf->header.h - src_h) / 2;
    area.x2 = area.x1 + src_w - 1;
    area.y2 = area.y1 + src_h - 1;
    lv_draw_image(&layer, &dsc, &area);

    lv_canvas_finish_layer(canvas, &layer);
    lv_obj_delete(canvas);
}

/* Decode a static image and draw it, scaled for the screen, into an RGB565
 * buffer. */
static const void *load_static(const char *path, int32_t screen_size)
{
    char lv_path[MAX_PATH_LEN + sizeof(LVGL_DRIVE)];
    if (snprintf(lv_path, sizeof(lv_path), LVGL_DRIVE "%s", path) >= (int)sizeof(lv_path))
    {
        return NULL;
    }

    // LVGL's BMP and JPEG decoders only recognize lower case extensions, but
    // cameras often write ".JPG". FAT file names aren't case sensitive, so
    // the lower case name still opens the file.
    for (char *c = strrchr(lv_path, '.'); c != NULL && *c != '\0'; c++)
    {
        *c = (char)tolower((unsigned char)*c);
    }

    lv_image_header_t header;
    if (lv_image_decoder_get_info(lv_path, &header) != LV_RESULT_OK
        || header.w == 0 || header.h == 0)
    {
        printf("images: can't read %s (BMP files must be 16, 24 or 32-bit)\n", path);
        return NULL;
    }
    // Some decoders report success for files they can't actually decode (the
    // BMP decoder does for 1/4/8-bit files), which crashes LVGL when drawn.
    switch (header.cf)
    {
        case LV_COLOR_FORMAT_RGB565:
        case LV_COLOR_FORMAT_RGB565A8:
        case LV_COLOR_FORMAT_RGB888:
        case LV_COLOR_FORMAT_XRGB8888:
        case LV_COLOR_FORMAT_ARGB8888:
            break;
        case LV_COLOR_FORMAT_RAW:
            // A JPEG decoder may report RAW here and decode to RGB888.
            if (has_extension(path, ".jpg") || has_extension(path, ".jpeg"))
            {
                break;
            }
            // fall through
        default:
            printf("images: can't read %s (BMP files must be 16, 24 or 32-bit)\n", path);
            return NULL;
    }
    if ((uint32_t)header.w * header.h > MAX_SOURCE_PIXELS)
    {
        printf("images: %s is too big (%dx%d), use 1024x1024 or less\n", path, header.w, header.h);
        return NULL;
    }

    int32_t reference = reference_size(header.w, header.h, prepared_size_of_file(path));
    int32_t w = LV_MAX(1, (header.w * screen_size + reference / 2) / reference);
    int32_t h = LV_MAX(1, (header.h * screen_size + reference / 2) / reference);

    // First decode the image at its own size onto black (the tile
    // background). The BMP decoder hands over the image a strip at a time,
    // which LVGL can copy but not scale, so scaling is a second step from the
    // decoded copy.
    lv_draw_buf_t *decoded = lv_draw_buf_create(header.w, header.h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (decoded == NULL)
    {
        printf("images: not enough memory for %s\n", path);
        return NULL;
    }
    lv_draw_buf_clear(decoded, NULL);
    draw_into(decoded, lv_path, header.w, header.h, LV_SCALE_NONE, LV_SCALE_NONE);

    if (w == (int32_t)header.w && h == (int32_t)header.h)
    {
        return decoded;
    }

    lv_draw_buf_t *scaled = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (scaled == NULL)
    {
        printf("images: not enough memory for %s\n", path);
        lv_draw_buf_destroy(decoded);
        return NULL;
    }
    lv_draw_buf_clear(scaled, NULL);
    draw_into(scaled, decoded, header.w, header.h,
              LV_SCALE_NONE * w / header.w, LV_SCALE_NONE * h / header.h);
    lv_draw_buf_destroy(decoded);
    return scaled;
}

size_t media_load_folder(const char *path, int32_t screen_size)
{
    // Use the "images" folder if there is one, otherwise the folder itself.
    char folder[MAX_FOLDER_LEN];
    DIR *dir = NULL;
    if (snprintf(folder, sizeof(folder), "%s/images", path) < (int)sizeof(folder))
    {
        dir = opendir(folder);
    }
    if (dir == NULL && snprintf(folder, sizeof(folder), "%s", path) < (int)sizeof(folder))
    {
        dir = opendir(folder);
    }
    if (dir == NULL)
    {
        return 0;
    }

    char *names[MAX_STORAGE_IMAGES];
    size_t name_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && name_count < MAX_STORAGE_IMAGES)
    {
        if (entry->d_type != DT_DIR && is_image_file(entry->d_name))
        {
            names[name_count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);
    qsort(names, name_count, sizeof(names[0]), compare_names);

    media_item_t *loaded = lv_malloc((name_count + item_count) * sizeof(media_item_t));
    LV_ASSERT_MALLOC(loaded);
    size_t loaded_count = 0;

    for (size_t i = 0; i < name_count; i++)
    {
        char file_path[MAX_PATH_LEN];
        if (snprintf(file_path, sizeof(file_path), "%s/%s", folder, names[i]) >= (int)sizeof(file_path))
        {
            printf("images: skipping %s, the name is too long\n", names[i]);
            free(names[i]);
            continue;
        }

        media_item_t *item = &loaded[loaded_count];
        item->is_gif = is_gif_file(names[i]);
        item->scale = LV_SCALE_NONE;
        item->src = item->is_gif ? load_gif(file_path, screen_size, &item->scale)
                                 : load_static(file_path, screen_size);
        if (item->src != NULL)
        {
            set_name(item, names[i]);
            printf("images: loaded %s\n", file_path);
            loaded_count++;
        }
        free(names[i]);
    }

    // The loaded images go before the built-in ones.
    memcpy(&loaded[loaded_count], items, item_count * sizeof(media_item_t));
    lv_free(items);
    items = loaded;
    item_count += loaded_count;
    return loaded_count;
}

#else

size_t media_load_folder(const char *path, int32_t screen_size)
{
    return 0;
}

#endif
