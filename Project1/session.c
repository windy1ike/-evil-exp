#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>

#include "session.h"
#include "system.h"
#include "entities.h"
#include "input_util.h"
#include "ui_print.h"

/* 前向声明：menu_select 定义在 menu_nav.c */
int menu_select(const char* title, const char* const items[], int count, int defaultIndex);

// 全局会话实例
CurrentSession g_session;

// 管理员固定密码
static const char ADMIN_PASSWORD[] = "admin123";

/* ======================================================
 * 欢迎页面（程序启动时显示一次）
 * ====================================================== */
void show_welcome(void) {
    ui_clear();
    printf("\n");
    printf("  +----------------------------------------------------+\n");
    printf("  |                                                    |\n");
    printf("  |      欢 迎 使 用  医 院 医 疗 管 理 系 统             |\n");
    printf("  |    Hospital Medical Management System  v1.0        |\n");
    printf("  |                                                    |\n");
    printf("  +----------------------------------------------------+\n");
    printf("\n");
    printf("          [ Enter ] 进入系统\n");
    printf("          [ Q     ] 退出程序\n");
    printf("\n");
    for (;;) {
        int ch = _getch();
        if (ch == 13) return;                  /* Enter - 继续 */
        if (ch == 'q' || ch == 'Q') exit(0);   /* Q - 退出程序 */
    }
}

/* ======================================================
 * 登录流程（箭头选择角色）
 * ====================================================== */
int do_login(void) {
    static const char* const roleItems[] = {
        "患者登录",
        "医生/医护人员登录",
        "管理员登录",
        "退出系统"
    };

    for (;;) {
        int choice = menu_select(
            "医院医疗管理系统  -  请选择登录角色",
            roleItems, 4, 0);

        if (choice < 0 || choice == 3) return 0;   /* 返回 / 退出系统 */

        if (choice == 0) {
            /* 患者登录：输入患者编号，校验已存在 */
            ui_clear();
            ui_frame_begin("患者登录");
            printf("  [提示] 输入 /q 取消登录\n");
            ui_divider();
            int pid;
            if (read_int_or_back("  请输入患者编号（/q 取消）：", &pid) <= 0) continue;

            Patient* p = find_patient_by_id(g_sys.patients, pid);
            if (!p) {
                printf("\n  [提示] 未找到患者编号 %d，请确认后重试。\n", pid);
                ui_pause();
                continue;
            }

            g_session.role      = ROLE_PATIENT;
            g_session.patientId = pid;
            g_session.doctorId  = 0;
            g_session.deptId    = 0;
            printf("\n  登录成功！欢迎您，%s（患者编号：%d）\n", p->name, pid);
            ui_pause();
            return 1;
        }
        else if (choice == 1) {
            /* 医生/医护人员登录：输入医生工号，校验已存在 */
            ui_clear();
            ui_frame_begin("医生/医护人员登录");
            printf("  [提示] 输入 /q 取消登录\n");
            ui_divider();
            int did;
            if (read_int_or_back("  请输入医生工号（/q 取消）：", &did) <= 0) continue;

            Doctor* d = find_doctor_by_id(g_sys.doctors, did);
            if (!d) {
                printf("\n  [提示] 未找到工号 %d 的医生，请确认后重试。\n", did);
                ui_pause();
                continue;
            }

            g_session.role      = ROLE_DOCTOR;
            g_session.doctorId  = did;
            g_session.deptId    = (d->dept ? d->dept->id : 0);
            g_session.patientId = 0;
            printf("\n  登录成功！欢迎，%s 医生（工号：%d，科室 ID：%d）\n",
                d->name, did, g_session.deptId);
            ui_pause();
            return 1;
        }
        else if (choice == 2) {
            /* 管理员登录：验证固定密码 */
            ui_clear();
            ui_frame_begin("管理员登录");
            printf("  [提示] 输入 /q 取消登录\n");
            ui_divider();
            char pwd[64];
            if (read_line_or_back("  请输入管理员密码（/q 取消）：", pwd, sizeof(pwd)) <= 0) continue;

            if (strcmp(pwd, ADMIN_PASSWORD) != 0) {
                printf("\n  [提示] 密码错误，请重试。\n");
                ui_pause();
                continue;
            }

            g_session.role      = ROLE_ADMIN;
            g_session.patientId = 0;
            g_session.doctorId  = 0;
            g_session.deptId    = 0;
            printf("\n  管理员登录成功。\n");
            ui_pause();
            return 1;
        }
    }
}
