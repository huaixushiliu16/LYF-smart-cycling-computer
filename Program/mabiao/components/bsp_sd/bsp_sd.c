/**
 * @file bsp_sd.c
 * @brief SD卡驱动实现（BSP层）
 * @note 阶段3：SD卡驱动开发
 */

#include "bsp_sd.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <unistd.h>  // For rmdir function
#include <dirent.h>  // For directory operations
#include <sys/stat.h>  // For S_ISDIR

static const char *TAG = "BSP_SD";

// SD卡引脚配置（SPI2_HOST独立总线）
#define BSP_SD_PIN_CS    1   // GPIO1
#define BSP_SD_PIN_SCLK  42  // GPIO42
#define BSP_SD_PIN_MOSI  41  // GPIO41
#define BSP_SD_PIN_MISO  40  // GPIO40

// 默认挂载点
#define BSP_SD_DEFAULT_MOUNT_POINT "/sdcard"

// 状态变量
static bool s_initialized = false;
static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;
static const char *s_mount_point = NULL;
static spi_host_device_t s_spi_host = SPI2_HOST;
static uint8_t s_cs_pin = BSP_SD_PIN_CS;  // 保存CS引脚配置

esp_err_t bsp_sd_init(const bsp_sd_config_t *config)
{
    esp_err_t ret = ESP_OK;

    if (s_initialized) {
        ESP_LOGW(TAG, "SD card driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card driver...");

    // 解析配置参数
    uint8_t cs_pin = BSP_SD_PIN_CS;
    spi_host_device_t spi_host = SPI2_HOST;
    
    if (config != NULL) {
        if (config->cs_pin != 0) {
            cs_pin = config->cs_pin;
        }
        if (config->spi_host != 0) {
            spi_host = (spi_host_device_t)config->spi_host;
        }
    }

    s_spi_host = spi_host;
    s_cs_pin = cs_pin;  // 保存CS引脚配置

    ESP_LOGI(TAG, "SPI Host: SPI%d_HOST", spi_host);
    ESP_LOGI(TAG, "Pins: CS=%d, SCLK=%d, MOSI=%d, MISO=%d",
             cs_pin, BSP_SD_PIN_SCLK, BSP_SD_PIN_MOSI, BSP_SD_PIN_MISO);

    // 配置SPI总线
    ESP_LOGI(TAG, "Initializing SPI bus...");
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SD_PIN_MOSI,
        .miso_io_num = BSP_SD_PIN_MISO,
        .sclk_io_num = BSP_SD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,  // 最大传输大小（字节）
    };

    ret = spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized with DMA");

    s_initialized = true;
    ESP_LOGI(TAG, "SD card driver initialized successfully");

    return ESP_OK;
}

esp_err_t bsp_sd_mount(const char *mount_point)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized) {
        ESP_LOGE(TAG, "SD card driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted at %s", s_mount_point);
        return ESP_OK;
    }

    // 使用默认挂载点或用户指定的挂载点
    const char *mp = (mount_point != NULL) ? mount_point : BSP_SD_DEFAULT_MOUNT_POINT;

    ESP_LOGI(TAG, "Mounting SD card filesystem at %s...", mp);

    // 配置FATFS挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 挂载失败时不格式化（避免误操作）
        .max_files = 5,                   // 最大同时打开文件数
        .allocation_unit_size = 16 * 1024, // 分配单元大小（16KB，适配大容量SD卡）
    };

    // 初始化SD卡主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = s_spi_host;

    // 配置SD卡槽位
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = s_cs_pin;  // 使用保存的CS引脚配置
    slot_config.host_id = s_spi_host;
    slot_config.gpio_cd = SDSPI_SLOT_NO_CD;
    slot_config.gpio_wp = SDSPI_SLOT_NO_WP;

    // 挂载FATFS文件系统
    ret = esp_vfs_fat_sdspi_mount(mp, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed to true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        // 挂载失败时不设置s_mount_point
        return ret;
    }

    // 挂载成功后才设置状态
    s_mount_point = mp;
    s_mounted = true;
    ESP_LOGI(TAG, "SD card filesystem mounted successfully");

    // 打印SD卡信息
    sdmmc_card_print_info(stdout, s_card);

    return ESP_OK;
}

esp_err_t bsp_sd_unmount(void)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized) {
        ESP_LOGE(TAG, "SD card driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card filesystem...");

    // 卸载FATFS文件系统
    ret = esp_vfs_fat_sdcard_unmount(s_mount_point, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem: %s", esp_err_to_name(ret));
        return ret;
    }

    s_card = NULL;
    s_mounted = false;
    s_mount_point = NULL;

    ESP_LOGI(TAG, "SD card filesystem unmounted successfully");

    return ESP_OK;
}

esp_err_t bsp_sd_get_info(bsp_sd_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    info->is_mounted = s_mounted;

    if (!s_mounted) {
        info->total_size_mb = 0;
        info->free_size_mb = 0;
        return ESP_OK;
    }

    // 使用esp_vfs_fat_info获取文件系统信息
    uint64_t bytes_total = 0;
    uint64_t bytes_free = 0;
    esp_err_t ret = esp_vfs_fat_info(s_mount_point, &bytes_total, &bytes_free);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get FATFS info: %s", esp_err_to_name(ret));
        info->total_size_mb = 0;
        info->free_size_mb = 0;
        return ret;
    }

    // 转换为MB
    info->total_size_mb = bytes_total / (1024 * 1024);
    info->free_size_mb = bytes_free / (1024 * 1024);

    return ESP_OK;
}

esp_err_t bsp_sd_deinit(void)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized) {
        return ESP_OK;
    }

    // 如果已挂载，先卸载
    if (s_mounted) {
        ret = bsp_sd_unmount();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to unmount during deinit: %s", esp_err_to_name(ret));
        }
    }

    // 释放SPI总线
    ret = spi_bus_free(s_spi_host);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
    }

    s_initialized = false;
    s_mounted = false;
    s_card = NULL;
    s_mount_point = NULL;

    ESP_LOGI(TAG, "SD card driver deinitialized");

    return ESP_OK;
}

// ==================== 文件操作接口 ====================

FILE* bsp_sd_fopen(const char *path, const char *mode)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return NULL;
    }

    if (path == NULL || mode == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: path or mode is NULL");
        return NULL;
    }

    FILE *f = fopen(path, mode);
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s (mode: %s)", path, mode);
    }

    return f;
}

size_t bsp_sd_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return 0;
    }

    if (ptr == NULL || stream == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: ptr or stream is NULL");
        return 0;
    }

    return fread(ptr, size, nmemb, stream);
}

size_t bsp_sd_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return 0;
    }

    if (ptr == NULL || stream == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: ptr or stream is NULL");
        return 0;
    }

    return fwrite(ptr, size, nmemb, stream);
}

int bsp_sd_fclose(FILE *stream)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return EOF;
    }

    if (stream == NULL) {
        ESP_LOGE(TAG, "Invalid argument: stream is NULL");
        return EOF;
    }

    return fclose(stream);
}

esp_err_t bsp_sd_remove(const char *path)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid argument: path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int ret = remove(path);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to remove file: %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// ==================== 目录操作接口 ====================

esp_err_t bsp_sd_mkdir(const char *path)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid argument: path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int ret = mkdir(path, 0755);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bsp_sd_rmdir(const char *path)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL) {
        ESP_LOGE(TAG, "Invalid argument: path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int ret = rmdir(path);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to remove directory: %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bsp_sd_stat(const char *path, struct stat *st)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL || st == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: path or st is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int ret = stat(path, st);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to get file stat: %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool bsp_sd_exists(const char *path)
{
    if (!s_mounted || path == NULL) {
        return false;
    }

    struct stat st;
    return (stat(path, &st) == 0);
}

int64_t bsp_sd_get_file_size(const char *path)
{
    if (!s_mounted || path == NULL) {
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }

    return (int64_t)st.st_size;
}

esp_err_t bsp_sd_list_dir(const char *dir_path, bsp_sd_dir_callback_t callback, void *user_data)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (dir_path == NULL || callback == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", dir_path);
        return ESP_FAIL;
    }

    struct dirent *entry;
    bool continue_scan = true;

    while ((entry = readdir(dir)) != NULL && continue_scan) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 判断是否为目录（d_type可能不可用，使用stat检查）
        bool is_dir = false;
        if (entry->d_type == DT_DIR) {
            is_dir = true;
        } else if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
            // d_type不可用，使用stat检查
            // 检查路径长度以避免截断
            size_t dir_path_len = strlen(dir_path);
            size_t name_len = strlen(entry->d_name);
            size_t total_len = dir_path_len + 1 + name_len + 1;  // +1 for '/', +1 for '\0'
            
            if (total_len <= 512) {  // 确保不超过缓冲区大小
                char full_path[512];
                // 手动构建路径以避免snprintf警告
                // 先复制目录路径
                if (dir_path_len < sizeof(full_path)) {
                    memcpy(full_path, dir_path, dir_path_len);
                    full_path[dir_path_len] = '/';
                    // 再复制文件名
                    if (name_len < sizeof(full_path) - dir_path_len - 1) {
                        memcpy(full_path + dir_path_len + 1, entry->d_name, name_len);
                        full_path[total_len - 1] = '\0';
                        
                        struct stat st;
                        if (stat(full_path, &st) == 0) {
                            is_dir = S_ISDIR(st.st_mode);
                        }
                    }
                }
            }
        }

        continue_scan = callback(entry->d_name, is_dir, user_data);
    }

    closedir(dir);
    return ESP_OK;
}

// 文件计数回调函数的上下文
struct count_context {
    uint32_t count;
};

// 文件计数回调函数
static bool count_files_callback(const char *filename, bool is_dir, void *user_data)
{
    struct count_context *ctx = (struct count_context *)user_data;
    if (!is_dir) {  // 只统计文件，不统计目录
        ctx->count++;
    }
    return true;  // 继续遍历
}

esp_err_t bsp_sd_count_files(const char *dir_path, uint32_t *count)
{
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;

    struct count_context ctx = {0};

    esp_err_t ret = bsp_sd_list_dir(dir_path, count_files_callback, &ctx);
    if (ret == ESP_OK) {
        *count = ctx.count;
    }

    return ret;
}

esp_err_t bsp_sd_get_dirname(const char *path, char *dir_buf, size_t buf_size)
{
    if (path == NULL || dir_buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 手动实现dirname功能
    size_t len = strlen(path);
    if (len == 0) {
        if (buf_size < 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        dir_buf[0] = '.';
        dir_buf[1] = '\0';
        return ESP_OK;
    }

    // 找到最后一个'/'的位置
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        // 没有'/'，返回"."
        if (buf_size < 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        strncpy(dir_buf, ".", buf_size - 1);
        dir_buf[buf_size - 1] = '\0';
        return ESP_OK;
    }

    // 如果最后一个'/'是第一个字符，返回"/"
    if (last_slash == path) {
        if (buf_size < 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        strncpy(dir_buf, "/", buf_size - 1);
        dir_buf[buf_size - 1] = '\0';
        return ESP_OK;
    }

    // 复制目录部分
    size_t dir_len = last_slash - path;
    if (dir_len >= buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    strncpy(dir_buf, path, dir_len);
    dir_buf[dir_len] = '\0';

    return ESP_OK;
}

esp_err_t bsp_sd_get_basename(const char *path, char *name_buf, size_t buf_size)
{
    if (path == NULL || name_buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 手动实现basename功能
    size_t len = strlen(path);
    if (len == 0) {
        if (buf_size < 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        strncpy(name_buf, ".", buf_size - 1);
        name_buf[buf_size - 1] = '\0';
        return ESP_OK;
    }

    // 找到最后一个'/'的位置
    const char *last_slash = strrchr(path, '/');
    const char *name_start;
    
    if (last_slash == NULL) {
        // 没有'/'，整个路径就是文件名
        name_start = path;
    } else {
        // 文件名从最后一个'/'之后开始
        name_start = last_slash + 1;
    }

    // 如果name_start为空（路径以'/'结尾），返回"."
    if (*name_start == '\0') {
        if (buf_size < 2) {
            return ESP_ERR_INVALID_SIZE;
        }
        strncpy(name_buf, ".", buf_size - 1);
        name_buf[buf_size - 1] = '\0';
        return ESP_OK;
    }

    size_t name_len = strlen(name_start);
    if (name_len >= buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    strncpy(name_buf, name_start, buf_size - 1);
    name_buf[buf_size - 1] = '\0';

    return ESP_OK;
}

esp_err_t bsp_sd_get_extension(const char *path, char *ext_buf, size_t buf_size)
{
    if (path == NULL || ext_buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *dot = strrchr(path, '.');
    if (dot == NULL || dot == path) {
        // 没有扩展名或只有点
        ext_buf[0] = '\0';
        return ESP_OK;
    }

    size_t len = strlen(dot + 1);
    if (len >= buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    strncpy(ext_buf, dot + 1, buf_size - 1);
    ext_buf[buf_size - 1] = '\0';

    return ESP_OK;
}

esp_err_t bsp_sd_path_join(const char *base, const char *sub, char *result_buf, size_t buf_size)
{
    if (base == NULL || sub == NULL || result_buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t base_len = strlen(base);
    
    // 跳过sub开头的'/'
    const char *sub_start = sub;
    while (*sub_start == '/') {
        sub_start++;
    }
    size_t sub_start_len = strlen(sub_start);
    
    // 计算所需的总长度（base + '/' + sub_start + '\0'）
    size_t need_slash = (base_len > 0 && base[base_len - 1] != '/') ? 1 : 0;
    size_t total_len = base_len + need_slash + sub_start_len + 1;  // +1 for '\0'

    if (total_len > buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    // 复制base
    strncpy(result_buf, base, buf_size - 1);
    result_buf[buf_size - 1] = '\0';
    size_t current_len = base_len;

    // 确保base以'/'结尾（如果需要）
    if (base_len > 0 && result_buf[base_len - 1] != '/') {
        result_buf[current_len] = '/';
        current_len++;
        result_buf[current_len] = '\0';
    }

    // 追加sub_start
    if (sub_start_len > 0) {
        if (current_len + sub_start_len < buf_size) {
            strncpy(result_buf + current_len, sub_start, buf_size - current_len - 1);
            result_buf[current_len + sub_start_len] = '\0';
        } else {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    return ESP_OK;
}

esp_err_t bsp_sd_ensure_dir_internal(const char *dir_path, int depth)
{
    // 防止递归过深（最多32层）
    if (depth > 32) {
        ESP_LOGE(TAG, "Directory path too deep: %s", dir_path);
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (dir_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查目录是否已存在
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;  // 目录已存在
        } else {
            ESP_LOGE(TAG, "Path exists but is not a directory: %s", dir_path);
            return ESP_ERR_INVALID_STATE;
        }
    }

    // 创建父目录（递归）
    char parent_path[256];
    esp_err_t ret = bsp_sd_get_dirname(dir_path, parent_path, sizeof(parent_path));
    if (ret == ESP_OK && strcmp(parent_path, dir_path) != 0 && strcmp(parent_path, "/") != 0) {
        // 递归创建父目录
        ret = bsp_sd_ensure_dir_internal(parent_path, depth + 1);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // 创建当前目录
    ret = bsp_sd_mkdir(dir_path);
    if (ret != ESP_OK && ret != ESP_FAIL) {
        // ESP_FAIL可能表示目录已存在（并发创建）
        struct stat st_check;
        if (stat(dir_path, &st_check) == 0 && S_ISDIR(st_check.st_mode)) {
            return ESP_OK;
        }
    }

    return ret;
}

esp_err_t bsp_sd_ensure_dir(const char *dir_path)
{
    return bsp_sd_ensure_dir_internal(dir_path, 0);
}
