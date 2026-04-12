#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "input_util.h"

/*
 * read_line_or_back
 *
 * 读取一行输入，去除尾部换行。
 * 返回值：
 *   INPUT_OK   ( 1) — 正常输入，存入 out
 *   INPUT_QUIT ( 0) — 用户输入 /q 或 :q（取消整个流程）
 *   INPUT_BACK (-2) — 用户输入 /b（返回上一步）
 *   INPUT_EOF  (-1) — EOF 或读取错误
 */
int read_line_or_back(const char* prompt, char* out, int outSize) {
    if (prompt) printf("%s", prompt);
    if (!fgets(out, outSize, stdin)) return INPUT_EOF;

    /* 去除尾部 \r\n */
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[--n] = '\0';
    }

    if (strcmp(out, "/b") == 0)                          return INPUT_BACK;
    if (strcmp(out, "/q") == 0 || strcmp(out, ":q") == 0) return INPUT_QUIT;
    return INPUT_OK;
}

/*
 * read_int_or_back
 *
 * 循环读取，直到获得有效整数或用户取消/返回。
 * 返回值同 read_line_or_back（INPUT_OK / INPUT_QUIT / INPUT_BACK / INPUT_EOF）。
 */
int read_int_or_back(const char* prompt, int* out) {
    char buf[64];
    for (;;) {
        int r = read_line_or_back(prompt, buf, (int)sizeof(buf));
        if (r != INPUT_OK) return r;

        char* end = NULL;
        long v = strtol(buf, &end, 10);

        /* 跳过尾部空白 */
        while (end && *end && isspace((unsigned char)*end)) end++;

        if (end == buf || (end && *end != '\0')) {
            printf("  [提示] 输入无效，请输入整数，或输入 /b 返回。\n");
            continue;
        }
        *out = (int)v;
        return INPUT_OK;
    }
}

/*
 * read_double_or_back
 *
 * 循环读取，直到获得有效浮点数或用户取消/返回。
 */
int read_double_or_back(const char* prompt, double* out) {
    char buf[64];
    for (;;) {
        int r = read_line_or_back(prompt, buf, (int)sizeof(buf));
        if (r != INPUT_OK) return r;

        char* end = NULL;
        double v = strtod(buf, &end);

        /* 跳过尾部空白 */
        while (end && *end && isspace((unsigned char)*end)) end++;

        if (end == buf || (end && *end != '\0')) {
            printf("  [提示] 输入无效，请输入数字，或输入 /b 返回。\n");
            continue;
        }
        *out = v;
        return INPUT_OK;
    }
}

/*
 * read_bool01_or_back
 *
 * 循环读取，直到获得 '0' 或 '1'，或用户取消/返回。
 */
int read_bool01_or_back(const char* prompt, int* out) {
    char buf[64];
    for (;;) {
        int r = read_line_or_back(prompt, buf, (int)sizeof(buf));
        if (r != INPUT_OK) return r;

        if (buf[0] == '0' && buf[1] == '\0') { *out = 0; return INPUT_OK; }
        if (buf[0] == '1' && buf[1] == '\0') { *out = 1; return INPUT_OK; }

        printf("  [提示] 输入无效，只能输入 0（否）或 1（是），或输入 /b 返回。\n");
    }
}
