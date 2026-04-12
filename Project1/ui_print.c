#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include "ui_print.h"

/* 内容区内宽（与欢迎界面 +----...----+ 同款，52个字符）*/
#define UI_LINE_WIDTH 52

void ui_clear(void) {
    system("cls");
}

/* 内部辅助：打印一条边框线 +fill*52+ */
static void _border(char fill) {
    int i;
    printf("  +");
    for (i = 0; i < UI_LINE_WIDTH; ++i) putchar(fill);
    printf("+\n");
}

/*
 * ui_header
 * 顶部 +---+ 边框 + 标题行 + +---+ 分隔线
 */
void ui_header(const char* title) {
    _border('-');
    printf("  |  %s\n", title);
    _border('-');
}

/*
 * ui_divider
 * 水平 +---+ 分隔线
 */
void ui_divider(void) {
    _border('-');
}

/*
 * ui_footer
 * +---+ 分隔线 + 提示行
 */
void ui_footer(const char* hint) {
    _border('-');
    if (hint && hint[0])
        printf("  |  %s\n", hint);
}

/*
 * ui_pause
 * 按任意键继续
 */
void ui_pause(void) {
    printf("\n  按任意键继续...\n");
    _getch();
}

/*
 * ui_frame_begin — 等同于 ui_header，提供语义化别名
 */
void ui_frame_begin(const char* title) {
    ui_header(title);
}

/*
 * ui_frame_end — 等同于 ui_footer，提供语义化别名
 */
void ui_frame_end(const char* hint) {
    ui_footer(hint);
}
