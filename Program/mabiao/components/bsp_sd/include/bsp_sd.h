/**
 * @file bsp_sd.h
 * @brief SD卡驱动头文件（BSP层）
 * @note 阶段2.5：软件分层架构搭建
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD卡初始化配置结构体
 */
typedef struct {
    uint8_t spi_host;   // SPI主机编号（默认SPI2_HOST）
    uint8_t cs_pin;     // 片选引脚（默认GPIO2）
} bsp_sd_config_t;

/**
 * @brief SD卡信息结构体
 */
typedef struct {
    uint64_t total_size_mb;  // 总容量（MB）
    uint64_t free_size_mb;   // 可用容量（MB）
    bool is_mounted;         // 是否已挂载
} bsp_sd_info_t;

/**
 * @brief 初始化SD卡驱动
 * 
 * @param config SD卡配置参数（可为NULL使用默认配置）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_init(const bsp_sd_config_t *config);

/**
 * @brief 挂载SD卡文件系统
 * 
 * @param mount_point 挂载点路径（如"/sdcard"）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_mount(const char *mount_point);

/**
 * @brief 卸载SD卡文件系统
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_sd_unmount(void);

/**
 * @brief 获取SD卡信息
 * 
 * @param info 输出参数，返回SD卡信息
 * @return esp_err_t 
 */
esp_err_t bsp_sd_get_info(bsp_sd_info_t *info);

/**
 * @brief 反初始化SD卡驱动
 * 
 * @return esp_err_t 
 */
esp_err_t bsp_sd_deinit(void);

/**
 * @brief 打开文件
 * 
 * @param path 文件路径（需包含挂载点前缀，如"/sdcard/test.txt"）
 * @param mode 打开模式（"r"/"w"/"a"/"rb"/"wb"/"ab"等）
 * @return FILE* 文件指针，失败返回NULL
 */
FILE* bsp_sd_fopen(const char *path, const char *mode);

/**
 * @brief 读取文件
 * 
 * @param ptr 数据缓冲区
 * @param size 每个元素的大小（字节）
 * @param nmemb 元素数量
 * @param stream 文件流指针
 * @return size_t 成功读取的元素数量
 */
size_t bsp_sd_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief 写入文件
 * 
 * @param ptr 数据缓冲区
 * @param size 每个元素的大小（字节）
 * @param nmemb 元素数量
 * @param stream 文件流指针
 * @return size_t 成功写入的元素数量
 */
size_t bsp_sd_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/**
 * @brief 关闭文件
 * 
 * @param stream 文件流指针
 * @return int 成功返回0，失败返回EOF
 */
int bsp_sd_fclose(FILE *stream);

/**
 * @brief 删除文件
 * 
 * @param path 文件路径（需包含挂载点前缀）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_remove(const char *path);

/**
 * @brief 创建目录
 * 
 * @param path 目录路径（需包含挂载点前缀）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_mkdir(const char *path);

/**
 * @brief 删除目录
 * 
 * @param path 目录路径（需包含挂载点前缀）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_rmdir(const char *path);

/**
 * @brief 获取文件/目录信息
 * 
 * @param path 文件/目录路径（需包含挂载点前缀）
 * @param st 输出参数，返回文件信息
 * @return esp_err_t 
 */
esp_err_t bsp_sd_stat(const char *path, struct stat *st);

/**
 * @brief 检查文件或目录是否存在
 * 
 * @param path 文件/目录路径（需包含挂载点前缀）
 * @return true 存在
 * @return false 不存在或错误
 */
bool bsp_sd_exists(const char *path);

/**
 * @brief 获取文件大小（字节）
 * 
 * @param path 文件路径（需包含挂载点前缀）
 * @return int64_t 文件大小（字节），失败返回-1
 */
int64_t bsp_sd_get_file_size(const char *path);

/**
 * @brief 目录遍历回调函数类型
 * 
 * @param filename 文件名（不包含路径）
 * @param is_dir 是否为目录
 * @param user_data 用户数据指针
 * @return true 继续遍历
 * @return false 停止遍历
 */
typedef bool (*bsp_sd_dir_callback_t)(const char *filename, bool is_dir, void *user_data);

/**
 * @brief 遍历目录中的文件和子目录
 * 
 * @param dir_path 目录路径（需包含挂载点前缀）
 * @param callback 回调函数，对每个文件/目录调用
 * @param user_data 传递给回调函数的用户数据
 * @return esp_err_t 
 */
esp_err_t bsp_sd_list_dir(const char *dir_path, bsp_sd_dir_callback_t callback, void *user_data);

/**
 * @brief 获取目录中的文件数量
 * 
 * @param dir_path 目录路径（需包含挂载点前缀）
 * @param count 输出参数，返回文件数量
 * @return esp_err_t 
 */
esp_err_t bsp_sd_count_files(const char *dir_path, uint32_t *count);

/**
 * @brief 路径操作：获取目录部分
 * 
 * @param path 完整路径
 * @param dir_buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t bsp_sd_get_dirname(const char *path, char *dir_buf, size_t buf_size);

/**
 * @brief 路径操作：获取文件名部分（包含扩展名）
 * 
 * @param path 完整路径
 * @param name_buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t bsp_sd_get_basename(const char *path, char *name_buf, size_t buf_size);

/**
 * @brief 路径操作：获取文件扩展名
 * 
 * @param path 完整路径
 * @param ext_buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t bsp_sd_get_extension(const char *path, char *ext_buf, size_t buf_size);

/**
 * @brief 路径操作：连接路径
 * 
 * @param base 基础路径
 * @param sub 子路径
 * @param result_buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t bsp_sd_path_join(const char *base, const char *sub, char *result_buf, size_t buf_size);

/**
 * @brief 确保目录存在（如果不存在则创建，包括父目录）
 * 
 * @param dir_path 目录路径（需包含挂载点前缀）
 * @return esp_err_t 
 */
esp_err_t bsp_sd_ensure_dir(const char *dir_path);

#ifdef __cplusplus
}
#endif
