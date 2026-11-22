/**
 * @file    ds_path.c
 * @brief   工程资源路径定位的实现（Windows）。
 *
 * 设计目标只有一个：让从工程根目录、build 目录、build/bin 目录启动的
 * 程序都能用同一套 "data/xxx.txt" 相对路径。做法是先用可执行文件位置
 * 反推工程根目录，再调用 SetCurrentDirectoryA 切换过去；后续所有文件
 * 操作仍然保持简单的相对路径写法。
 */
#include "ds/ds_path.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

/**
 * @brief 拼接目录与文件名/相对路径。
 *
 * 自动避免 dir 末尾已有分隔符时产生 "dir\\/name" 这类重复分隔符。
 * 返回值表示结果是否完整放入缓冲区，成功时 out 以 '\0' 结尾。
 */
static bool path_join(char *out, size_t size, const char *dir, const char *name)
{
    int written;
    size_t length;

    if (out == NULL || dir == NULL || name == NULL || size == 0) {
        return false;
    }

    length = strlen(dir);
    if (length == 0) {
        written = snprintf(out, size, "%s", name);
    } else if (dir[length - 1] == '\\' || dir[length - 1] == '/') {
        written = snprintf(out, size, "%s%s", dir, name);
    } else {
        written = snprintf(out, size, "%s\\%s", dir, name);
    }

    return written >= 0 && (size_t)written < size;
}

/** @brief 判断路径是否存在且为目录。 */
static bool path_is_dir(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/**
 * @brief 把路径转换为绝对路径并写入 buffer。
 *
 * GetFullPathNameA 会统一分隔符、消解 "." 与 ".."，因此后续比较和
 * SetCurrentDirectoryA 都更稳定。
 */
static DsStatus path_normalize(const char *path, char *buffer, size_t size)
{
    DWORD length;

    if (path == NULL || buffer == NULL) {
        return DS_ERR_NULL;
    }
    if (size == 0 || size > (size_t)MAXDWORD) {
        return DS_ERR_RANGE;
    }

    length = GetFullPathNameA(path, (DWORD)size, buffer, NULL);
    if (length == 0) {
        return DS_ERR_IO;
    }
    if (length >= (DWORD)size) {
        return DS_ERR_RANGE;
    }

    return DS_OK;
}

/**
 * @brief 取当前可执行文件所在目录（绝对路径）。
 */
static DsStatus path_executable_dir(char *buffer, size_t size)
{
    char executable[DS_PATH_MAX];
    char *slash;
    char *forward_slash;
    DWORD length;

    if (buffer == NULL || size == 0) {
        return DS_ERR_NULL;
    }

    length = GetModuleFileNameA(NULL, executable, (DWORD)sizeof(executable));
    if (length == 0) {
        return DS_ERR_IO;
    }
    if (length >= (DWORD)sizeof(executable)) {
        return DS_ERR_RANGE;
    }

    /* 同时兼容 Windows 默认的反斜杠和少数工具传入的正斜杠。 */
    slash = strrchr(executable, '\\');
    forward_slash = strrchr(executable, '/');
    if (forward_slash != NULL && (slash == NULL || forward_slash > slash)) {
        slash = forward_slash;
    }
    if (slash == NULL) {
        return DS_ERR_IO;
    }
    *slash = '\0';

    return path_normalize(executable, buffer, size);
}

/** @brief 判断 base 下是否存在 data 子目录。 */
static bool path_has_data_dir(const char *base)
{
    char data_dir[DS_PATH_MAX];

    return path_join(data_dir, sizeof(data_dir), base, "data")
        && path_is_dir(data_dir);
}

DsStatus ds_path_project_root(char *buffer, size_t size)
{
    char bases[4][DS_PATH_MAX];
    size_t base_count = 0;
    size_t i;

    if (buffer == NULL) {
        return DS_ERR_NULL;
    }
    if (size == 0) {
        return DS_ERR_RANGE;
    }

    /*
     * 情况一：从工程根目录启动。先检查 "."，这样即使 exe 被复制到
     * 其他位置，只要用户当前就在工程根目录，仍然可以正常找到数据。
     */
    if (path_has_data_dir(".")) {
        return path_normalize(".", buffer, size);
    }

    /*
     * 情况二：从 build、build/bin 或更深一层的调试输出目录启动。
     * 以 exe 所在目录为起点，依次检查自身及上 1~3 级目录。
     */
    if (path_executable_dir(bases[0], sizeof(bases[0])) != DS_OK) {
        return DS_ERR_NOT_FOUND;
    }
    base_count = 1;

    for (i = 1; i < 4; ++i) {
        if (!path_join(bases[i], sizeof(bases[i]), bases[i - 1], "..")) {
            break;
        }
        ++base_count;
    }

    for (i = 0; i < base_count; ++i) {
        if (path_has_data_dir(bases[i])) {
            return path_normalize(bases[i], buffer, size);
        }
    }

    return DS_ERR_NOT_FOUND;
}

DsStatus ds_path_set_project_root(void)
{
    char root[DS_PATH_MAX];
    DsStatus status;

    status = ds_path_project_root(root, sizeof(root));
    if (status != DS_OK) {
        return status;
    }

    if (!SetCurrentDirectoryA(root)) {
        return DS_ERR_IO;
    }

    return DS_OK;
}
