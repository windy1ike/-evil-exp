#include <stdio.h>
#include <string.h>
#include "time_util.h"
#include "constants.h"
#include "input_util.h"

//内部函数

//固定使用2026年，实现简化，不依赖系统时间复杂处理


static int get_current_year(void) {
    return CURRENT_YEAR;
}

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

//返回给定年、月的这个月有几天，月份无效返回 0
 
static int days_in_month(int year, int month) {
    static const int days_normal[12] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days_normal[month - 1];
}

//对外函数实现

int parse_time(const char* str, SimpleTime* t) {
    if (!str || !t) return 0;

    int m, d, hh, mm;


    if (sscanf(str, "%d-%d %d:%d", &m, &d, &hh, &mm) != 4) {
        return 0;
    }

    int year = get_current_year();
    int dim = days_in_month(year, m);
    if (dim == 0) return 0;

    if (m < 1 || m > 12) return 0;
    if (d < 1 || d > dim) return 0;
    if (hh < 0 || hh > 23) return 0;
    if (mm < 0 || mm > 59) return 0;

    t->month = m;
    t->day = d;
    t->hour = hh;
    t->minute = mm;
    return 1;
}

int cmp_time(const SimpleTime* a, const SimpleTime* b) {
    if (!a || !b) return 0;
    if (a->month != b->month)  return a->month - b->month;
    if (a->day != b->day)    return a->day - b->day;
    if (a->hour != b->hour)   return a->hour - b->hour;
    return a->minute - b->minute;
}

//转换为分钟数，用于计算天数、时间差等

long long simpletime_to_minutes(const SimpleTime* t) {
    if (!t) return 0;
    int year = get_current_year();

    long long totalDays = 0;
    for (int m = 1; m < t->month; ++m) {
        totalDays += days_in_month(year, m);
    }
    // 当前月从第 1 日开始，day-1 是从 0 开始的天数偏移
    totalDays += (t->day - 1);

    long long totalMinutes = totalDays * 24LL * 60LL
        + t->hour * 60LL
        + t->minute;
    return totalMinutes;
}

/*
 * 将分钟数还原为 SimpleTime。
 * 要求 minutes 在 [0, 全年总分钟数)，否则返回 0。
 */
int minutes_to_simpletime(long long minutes, SimpleTime* t) {
    if (!t) return 0;
    if (minutes < 0) return 0;

    int year = get_current_year();

    // 计算一年总分钟数
    int totalDaysOfYear = is_leap_year(year) ? 366 : 365;
    long long maxMinutes = (long long)totalDaysOfYear * 24LL * 60LL;
    if (minutes >= maxMinutes) {
        return 0; // 超出年份范围
    }

    // 分解为 天 + 分钟
    long long dayIndex = minutes / (24LL * 60LL);       // 从0开始
    long long minuteOfDay = minutes % (24LL * 60LL);

    int hour = (int)(minuteOfDay / 60LL);
    int min = (int)(minuteOfDay % 60LL);

    // 从 dayIndex 换算 month/day
    int month = 1;
    int day = 0;
    while (month <= 12) {
        int dim = days_in_month(year, month);
        if (dayIndex < dim) {
            day = (int)dayIndex + 1;
            break;
        }
        else {
            dayIndex -= dim;
            month++;
        }
    }
    if (month > 12) return 0; // 理论上不会发生

    t->month = month;
    t->day = day;
    t->hour = hour;
    t->minute = min;
    return 1;
}

int days_between(const SimpleTime* start, const SimpleTime* end) {
    if (!start || !end) return 0;
    long long m1 = simpletime_to_minutes(start);
    long long m2 = simpletime_to_minutes(end);
    long long diff = m2 - m1;
    long long day = diff / (24LL * 60LL); // 向下取整
    return (int)day;
}

long long minutes_between(const SimpleTime* start, const SimpleTime* end) {
    if (!start || !end) return 0;
    long long m1 = simpletime_to_minutes(start);
    long long m2 = simpletime_to_minutes(end);
    return (m2 - m1);
}

void print_time(const SimpleTime* t) {
    if (!t) {
        printf("00-00 00:00");
        return;
    }
    printf("%02d-%02d %02d:%02d", t->month, t->day, t->hour, t->minute);
}

/* ==================== 输入辅助函数 ==================== */

int read_line(char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return 0;
    if (!fgets(buf, bufSize, stdin)) {
        return 0; // 读取失败，EOF 或错误
    }
    // 去除尾部换行
    size_t len = strlen(buf);
    if (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[len - 1] = '\0';
        // windows 可能输入 \r\n
        len--;
        if (len > 0 && buf[len - 1] == '\r') {
            buf[len - 1] = '\0';
        }
    }
    return 1;
}

int prompt_and_read_time(const char* prompt, SimpleTime* t) {
    if (!t) return 0;

    char buf[64];
    while (1) {
        if (prompt) {
            printf("%s", prompt);
        }

        if (!read_line(buf, sizeof(buf))) {
            // 读取失败，例如 Ctrl+Z，直接返回
            return 0;
        }

        if (parse_time(buf, t)) {
            // 解析成功
            return 1;
        }
        else {
            printf("时间格式无效，请使用 \"MM-DD HH:MM\"，确保各字段合法（示例：04-01 09:30）。\n");
        }
    }
}
/*
 * prompt_and_read_time_or_back
 * 支持 /q 或 :q 取消，/b 返回上一步。
 * 返回值：1=成功，0=用户取消(/q)，-1=EOF，-2=用户返回(/b)
 */
int prompt_and_read_time_or_back(const char* prompt, SimpleTime* t) {
    if (!t) return -1;

    char buf[64];
    for (;;) {
        int r = read_line_or_back(prompt, buf, (int)sizeof(buf));
        if (r != 1) return r;   /* 0=/q, -1=EOF, -2=/b */

        if (parse_time(buf, t)) {
            return 1;
        }
        printf("  [提示] 时间格式无效，请使用 \"MM-DD HH:MM\"（示例：04-01 09:30），\n"
               "         或输入 /b 返回上一步，/q 取消。\n");
    }
}
