/**
 * @file lv_port_fs.c
 * @brief LVGL文件系统驱动实现（适配ESP-IDF VFS）
 * @note 使用标准C库文件操作，支持ESP-IDF VFS挂载的文件系统
 */

#include "lv_port_fs.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"

static const char *TAG = "LV_FS_VFS";

// 文件句柄结构
typedef struct {
    FILE *fp;
} file_handle_t;

// 目录句柄结构
typedef struct {
    DIR *dir;
    char path[256];  // 保存目录路径，用于构建完整文件路径
} dir_handle_t;

/**
 * @brief 路径转换：将 /MAP/... 或 MAP/... 转换为 /sdcard/MAP/...
 * @param path 原始路径
 * @param out_path 输出路径缓冲区
 * @param out_size 输出路径缓冲区大小
 */
static void convert_path(const char *path, char *out_path, size_t out_size)
{
    if (path == NULL || out_path == NULL || out_size == 0) {
        return;
    }
    
    // 如果已经是 /sdcard 路径，直接使用（注意：需要检查完整匹配，避免 /sdcardxxx 也被匹配）
    if (strncmp(path, "/sdcard", 7) == 0 && (path[7] == '\0' || path[7] == '/')) {
        strncpy(out_path, path, out_size - 1);
        out_path[out_size - 1] = '\0';
        return;
    }
    
    // 如果路径以 sdcard 开头（没有前导斜杠），添加前导斜杠后直接使用
    if (strncmp(path, "sdcard", 6) == 0 && (path[6] == '\0' || path[6] == '/')) {
        snprintf(out_path, out_size, "/%s", path);
        return;
    }
    
    // 如果路径以 / 开头但不是 /sdcard，添加 /sdcard 前缀
    if (path[0] == '/') {
        snprintf(out_path, out_size, "/sdcard%s", path);
        return;
    }
    
    // 如果路径以 MAP 或 FONT 开头（不区分大小写），添加 /sdcard/ 前缀
    if (strncmp(path, "MAP", 3) == 0 || strncmp(path, "map", 3) == 0 ||
        strncmp(path, "FONT", 4) == 0 || strncmp(path, "font", 4) == 0) {
        snprintf(out_path, out_size, "/sdcard/%s", path);
        return;
    }
    
    // 其他情况，尝试添加 /sdcard/ 前缀
    snprintf(out_path, out_size, "/sdcard/%s", path);
}

/**
 * @brief 打开文件
 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);
    
    char real_path[256];
    convert_path(path, real_path, sizeof(real_path));
    
    const char *mode_str;
    if (mode == LV_FS_MODE_WR) {
        mode_str = "wb";
    } else if (mode == LV_FS_MODE_RD) {
        mode_str = "rb";
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        mode_str = "rb+";
    } else {
        return NULL;
    }
    
    FILE *f = fopen(real_path, mode_str);
    if (f == NULL) {
        ESP_LOGW(TAG, "Failed to open file: %s (converted: %s, mode: %s)", path, real_path, mode_str);
        return NULL;
    }
    
    ESP_LOGD(TAG, "File opened successfully: %s -> %s", path, real_path);
    
    file_handle_t *handle = (file_handle_t *)lv_mem_alloc(sizeof(file_handle_t));
    if (handle == NULL) {
        fclose(f);
        return NULL;
    }
    
    handle->fp = f;
    return handle;
}

/**
 * @brief 关闭文件
 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);
    
    file_handle_t *handle = (file_handle_t *)file_p;
    if (handle == NULL || handle->fp == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    fclose(handle->fp);
    lv_mem_free(handle);
    return LV_FS_RES_OK;
}

/**
 * @brief 读取文件
 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    LV_UNUSED(drv);
    
    file_handle_t *handle = (file_handle_t *)file_p;
    if (handle == NULL || handle->fp == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    size_t bytes_read = fread(buf, 1, btr, handle->fp);
    if (br != NULL) {
        *br = bytes_read;
    }
    
    if (ferror(handle->fp)) {
        return LV_FS_RES_UNKNOWN;
    }
    
    return LV_FS_RES_OK;
}

/**
 * @brief 写入文件
 */
static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    LV_UNUSED(drv);
    
    file_handle_t *handle = (file_handle_t *)file_p;
    if (handle == NULL || handle->fp == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    size_t bytes_written = fwrite(buf, 1, btw, handle->fp);
    if (bw != NULL) {
        *bw = bytes_written;
    }
    
    if (ferror(handle->fp)) {
        return LV_FS_RES_UNKNOWN;
    }
    
    return LV_FS_RES_OK;
}

/**
 * @brief 文件定位
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    LV_UNUSED(drv);
    
    file_handle_t *handle = (file_handle_t *)file_p;
    if (handle == NULL || handle->fp == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    int origin;
    switch (whence) {
        case LV_FS_SEEK_SET:
            origin = SEEK_SET;
            break;
        case LV_FS_SEEK_CUR:
            origin = SEEK_CUR;
            break;
        case LV_FS_SEEK_END:
            origin = SEEK_END;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }
    
    if (fseek(handle->fp, pos, origin) != 0) {
        return LV_FS_RES_UNKNOWN;
    }
    
    return LV_FS_RES_OK;
}

/**
 * @brief 获取文件位置
 */
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    LV_UNUSED(drv);
    
    file_handle_t *handle = (file_handle_t *)file_p;
    if (handle == NULL || handle->fp == NULL || pos_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    long pos = ftell(handle->fp);
    if (pos < 0) {
        return LV_FS_RES_UNKNOWN;
    }
    
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

/**
 * @brief 打开目录
 */
static void *fs_dir_open(lv_fs_drv_t *drv, const char *path)
{
    LV_UNUSED(drv);
    
    char real_path[256];
    convert_path(path, real_path, sizeof(real_path));
    
    DIR *d = opendir(real_path);
    if (d == NULL) {
        ESP_LOGW(TAG, "Failed to open directory: %s (real: %s)", path, real_path);
        return NULL;
    }
    
    dir_handle_t *handle = (dir_handle_t *)lv_mem_alloc(sizeof(dir_handle_t));
    if (handle == NULL) {
        closedir(d);
        return NULL;
    }
    
    handle->dir = d;
    strncpy(handle->path, real_path, sizeof(handle->path) - 1);
    handle->path[sizeof(handle->path) - 1] = '\0';
    
    return handle;
}

/**
 * @brief 读取目录项
 */
static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *dir_p, char *fn)
{
    LV_UNUSED(drv);
    
    dir_handle_t *handle = (dir_handle_t *)dir_p;
    if (handle == NULL || handle->dir == NULL || fn == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    struct dirent *entry = readdir(handle->dir);
    if (entry == NULL) {
        fn[0] = '\0';
        return LV_FS_RES_OK;
    }
    
    // 跳过 . 和 ..
    while (entry != NULL && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
        entry = readdir(handle->dir);
    }
    
    if (entry == NULL) {
        fn[0] = '\0';
        return LV_FS_RES_OK;
    }
    
    // 检查是否为目录
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", handle->path, entry->d_name);
    
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // 目录名以 / 开头
        fn[0] = '/';
        strncpy(&fn[1], entry->d_name, LV_FS_MAX_FN_LENGTH - 2);
        fn[LV_FS_MAX_FN_LENGTH - 1] = '\0';
    } else {
        strncpy(fn, entry->d_name, LV_FS_MAX_FN_LENGTH - 1);
        fn[LV_FS_MAX_FN_LENGTH - 1] = '\0';
    }
    
    return LV_FS_RES_OK;
}

/**
 * @brief 关闭目录
 */
static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *dir_p)
{
    LV_UNUSED(drv);
    
    dir_handle_t *handle = (dir_handle_t *)dir_p;
    if (handle == NULL || handle->dir == NULL) {
        return LV_FS_RES_INV_PARAM;
    }
    
    closedir(handle->dir);
    lv_mem_free(handle);
    return LV_FS_RES_OK;
}

/**
 * @brief 初始化LVGL文件系统驱动
 */
void lv_fs_vfs_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    
    // 使用 '/' 作为驱动字母（与version2保持一致）
    fs_drv.letter = '/';
    
    // 注册回调函数
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;
    
    lv_fs_drv_register(&fs_drv);
    
    ESP_LOGI(TAG, "LVGL VFS file system driver registered (path conversion: /MAP -> /sdcard/MAP)");
}
