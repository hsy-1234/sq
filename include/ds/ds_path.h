/**
 * @file    ds_path.h
 * @brief   工程资源路径定位。
 *
 * 直接双击 build/bin 下的 exe，或在 build 目录里运行 exe 时，当前工作
 * 目录并不是工程根目录，此时 "data/f1.txt" 会解析到错误位置。本模块
 * 根据可执行文件位置向上查找包含 data 目录的工程根目录，供各题目在
 * 启动时统一切换工作目录。
 */
#ifndef DS_PATH_H
#define DS_PATH_H

#include <stddef.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 路径缓冲区建议大小（Windows MAX_PATH 为 260，这里留出余量）。 */
#ifndef DS_PATH_MAX
#define DS_PATH_MAX 512
#endif

/**
 * @brief 查找工程根目录。
 *
 * 依次检查当前目录、可执行文件所在目录及其上三级，返回第一个包含
 * data 子目录的目录。
 *
 * @param buffer 输出缓冲区，接收绝对路径。
 * @param size   缓冲区字节数。
 * @return DS_OK；未找到返回 DS_ERR_NOT_FOUND；参数错误返回
 *         DS_ERR_NULL / DS_ERR_RANGE。
 */
DsStatus ds_path_project_root(char *buffer, size_t size);

/**
 * @brief 把当前工作目录切换到工程根目录。
 *
 * 各题目程序在启动时调用一次，之后 "data/xxx.txt" 等相对路径在
 * 工程根目录、build 目录、build/bin 目录下运行都能正确解析。
 *
 * @return DS_OK；找不到工程根目录返回 DS_ERR_NOT_FOUND；切换失败
 *         返回 DS_ERR_IO。
 */
DsStatus ds_path_set_project_root(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_PATH_H */
