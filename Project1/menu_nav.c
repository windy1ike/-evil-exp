#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include "ui_print.h"

static void console_set_color(int highlight) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (highlight) {
        SetConsoleTextAttribute(h,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }
    else {
        SetConsoleTextAttribute(h,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

/*
 * menu_select
 * 显示一个带页头的箭头选择菜单。
 * 返回选中项下标 [0..count-1]，← 或 Backspace 返回 -1。
 */
int menu_select(const char* title, const char* const items[], int count, int defaultIndex) {
    int cur = (defaultIndex >= 0 && defaultIndex < count) ? defaultIndex : 0;

    for (;;) {
        int i;
        ui_clear();

        /* 顶部边框 + 标题 + 分隔线 */
        ui_frame_begin(title);
        printf("\n");

        /* 菜单项 */
        for (i = 0; i < count; ++i) {
            if (i == cur) {
                console_set_color(1);
                printf("    > %s\n", items[i]);
                console_set_color(0);
            }
            else {
                printf("      %s\n", items[i]);
            }
        }

        printf("\n");

        /* 底部分隔线 + 按键提示 */
        ui_frame_end("上/下 选择  Enter 确认  ←/Backspace 返回");

        /* 读键 */
        int ch = _getch();

        /* 扩展键需再读前缀后再读功能码 */
        if (ch == 0 || ch == 224) {
            int code = _getch();
            if (code == 72) {               /* Up */
                cur = (cur - 1 + count) % count;
            }
            else if (code == 80) {          /* Down */
                cur = (cur + 1) % count;
            }
            else if (code == 75) return -1; /* Left */
            continue;
        }

        if (ch == 13) return cur;  /* Enter */
        if (ch == 8)  return -1;   /* Backspace */
    }
}
