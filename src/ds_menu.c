/**
 * @file    ds_menu.c
 * @brief   通用控制台菜单循环。
 *
 * 菜单只负责“显示编号、读入选择、调用回调”，不包含任何业务逻辑。
 * 四道上机题各自定义 DsMenuItem 数组，把函数指针传进来即可复用同一套
 * 交互流程。约定：
 *   - 编号从 1 开始，0 固定表示退出；
 *   - 编号可以不连续，查找时按 id 线性扫描；
 *   - action 为 NULL 的菜单项表示“尚未实现”，便于先搭骨架再逐项填充。
 */
#include "ds/ds_menu.h"
#include "ds/ds_console.h"
#include "ds/ds_input.h"

#include <stdio.h>

/**
 * @brief 运行菜单主循环，直到用户选择 0 或输入流结束。
 *
 * @param menu    菜单描述（标题、菜单项数组、项数）。
 * @param context 透传给每个 action 的上下文指针，可为 NULL。
 *
 * 每一轮都重新清屏并打印菜单，因此某个功能输出很多内容后不会污染
 * 下一轮界面。输入 EOF（例如重定向文件结束）时直接返回，避免死循环。
 *
 * 复杂度：每轮 O(n) 打印和查找，n 为菜单项数；相对于用户交互可视为 O(1)。
 */
void ds_menu_run(const DsMenu *menu, void *context)
{
    if (menu == NULL) {
        return;
    }

    for (;;) {
        size_t i;
        int max_id = 0;
        int choice = 0;
        const DsMenuItem *selected = NULL;
        DsStatus status;

        /* 1. 清屏并显示标题，保证每轮界面从固定位置开始。 */
        ds_console_clear();
        ds_console_title(menu->title != NULL ? menu->title : "菜单");

        /* 2. 打印所有有效菜单项，同时记录最大编号作为输入上界。 */
        for (i = 0; i < menu->count; ++i) {
            const DsMenuItem *item = &menu->items[i];
            if (item->id <= 0) {
                /* 0 和负数保留给“退出”，不允许作为功能编号。 */
                continue;
            }
            printf("  %2d. %s\n", item->id, item->text != NULL ? item->text : "");
            if (item->id > max_id) {
                max_id = item->id;
            }
        }
        printf("   0. 退出\n");
        ds_console_rule('-', 60);

        /* 3. 读取选择；输入失败（EOF）时结束菜单，而不是反复重试。 */
        status = ds_input_int_range("请选择功能: ", 0, max_id, &choice);
        if (status != DS_OK) {
            putchar('\n');
            return;
        }
        if (choice == 0) {
            return;
        }

        /* 4. 按编号查找菜单项。编号允许不连续，所以不能直接用下标。 */
        for (i = 0; i < menu->count; ++i) {
            if (menu->items[i].id == choice) {
                selected = &menu->items[i];
                break;
            }
        }

        /* 5. 分发执行。NULL action 只提示，不视为错误。 */
        if (selected == NULL) {
            ds_console_error("没有编号为 %d 的功能。", choice);
        } else if (selected->action == NULL) {
            ds_console_info("“%s”尚未实现。", selected->text != NULL ? selected->text : "");
        } else {
            selected->action(context);
        }

        /* 6. 暂停，让用户看清结果后再进入下一轮。 */
        ds_console_pause(NULL);
    }
}
