#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include "app.h"          /* system.h, menu.h, session.h */
#include "model.h"        /* constants.h, time_util.h, entities.h, record.h, drug.h, ward.h */
#include "validate.h"
#include "reports.h"      /* query.h, stats.h */
#include "persistence.h"  /* io.h */
#include "input.h"        /* input_util.h */
#include "ui_print.h"      /* ui_clear, ui_header, ui_divider, ui_footer, ui_pause */

// ================== 全局系统数据 ==================
HospitalSystem g_sys;

// ================== ID 计数器（供 io.c 使用） ==================
int next_patient_id = 10001;
int next_doctor_id = 20001;
int next_dept_id = 30001;
int next_record_id = 40001;
int next_drug_id = 50001;
int next_bed_id = 60001;
int next_drug_stock_id = 70001;

// ================== 初始化 / 销毁 ==================

void init_system() {
    g_sys.fund = 0.0;
    g_sys.patients = NULL;
    g_sys.doctors = NULL;
    g_sys.departments = NULL;
    g_sys.drugs = NULL;
    g_sys.drugStocks = NULL;
    g_sys.wards = NULL;
    g_sys.beds = NULL;
    g_sys.records = NULL;
    g_sys.todayTotalReg = 0;
}

void destroy_system() {
    free_patients(g_sys.patients);
    free_doctors(g_sys.doctors);
    free_departments(g_sys.departments);
    free_drugs(g_sys.drugs);
    free_drug_stock_records(g_sys.drugStocks);
    free_wards(g_sys.wards);
    free_beds(g_sys.beds);
    free_records(g_sys.records);

    g_sys.patients = NULL;
    g_sys.doctors = NULL;
    g_sys.departments = NULL;
    g_sys.drugs = NULL;
    g_sys.drugStocks = NULL;
    g_sys.wards = NULL;
    g_sys.beds = NULL;
    g_sys.records = NULL;
}

// ================== 挂号地编号生成 ==================

static long long generate_reg_id_full(const SimpleTime* regTime,
    int deptId, int doctorId, int seqOfDay) {
    int year2 = CURRENT_YEAR % 100;  // 年份后两位

    long long id = 0;
    id = year2;
    id = id * 100 + regTime->month;
    id = id * 100 + regTime->day;
    id = id * 10000 + (deptId % 10000);
    id = id * 10000 + (doctorId % 10000);
    id = id * 10000 + (seqOfDay % 10000);
    return id;
}

static int get_seq_no_of_day(const SimpleTime* regTime) {
    int totalToday = 0;
    MedicalRecord* r = g_sys.records;
    while (r) {
        if (!r->isCancelled &&
            r->regInfo.regTime.month == regTime->month &&
            r->regInfo.regTime.day == regTime->day) {
            totalToday++;
        }
        r = r->next;
    }
    return totalToday + 1;
}

// ================== 子步骤：患者/科室/医生信息 ==================

/*
 * ensure_patient_by_id_or_new
 *
 * 让用户选择已有患者或新建患者。
 * 返回值：
 *   INPUT_OK   ( 1) — *out 已填入有效患者指针
 *   INPUT_QUIT ( 0) — 用户输入 /q 取消整个流程
 *   INPUT_BACK (-2) — 用户输入 /b，请求返回上一步
 *   INPUT_EOF  (-1) — EOF
 */
static int ensure_patient_by_id_or_new(Patient** out) {
    *out = NULL;

    for (;;) {
        ui_divider();
        printf("  [步骤] 患者信息  [/b 上一步, /q 取消]\n");
        printf("  1. 使用已有患者编号\n");
        printf("  2. 新建患者\n");

        int choice, r;
        r = read_int_or_back("  请选择(1/2)：", &choice);
        if (r == INPUT_QUIT) return INPUT_QUIT;
        if (r == INPUT_BACK) return INPUT_BACK;
        if (r == INPUT_EOF)  return INPUT_QUIT;

        if (choice == 1) {
            /* --- 查找已有患者 --- */
            for (;;) {
                int id;
                r = read_int_or_back("  请输入患者编号：", &id);
                if (r == INPUT_QUIT) return INPUT_QUIT;
                if (r == INPUT_BACK) break;   /* /b → 返回选择 1/2 */
                if (r == INPUT_EOF)  return INPUT_QUIT;

                Patient* p = find_patient_by_id(g_sys.patients, id);
                if (!p) {
                    printf("  [提示] 未找到编号 %d 的患者，请重新输入。\n", id);
                    continue;  /* 校验失败：重新输入 */
                }
                *out = p;
                return INPUT_OK;
            }
            /* 从内层 break 出来 → 回到外层 for 重新选择 1/2 */

        } else if (choice == 2) {
            /* --- 新建患者（逐字段输入，支持 /b 回上一字段） --- */
            char name[64] = {0};
            int  age = 0;
            int  gender = 0;  /* 0=男, 1=女 */
            int  fstep = 0;   /* 0=姓名, 1=年龄, 2=性别 */

            while (fstep < 3) {
                int back = 0;

                if (fstep == 0) {
                    for (;;) {
                        r = read_line_or_back("  患者姓名：", name, sizeof(name));
                        if (r == INPUT_QUIT) return INPUT_QUIT;
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  return INPUT_QUIT;
                        if (name[0] == '\0') {
                            printf("  [提示] 姓名不能为空，请重新输入。\n");
                            continue;
                        }
                        break;
                    }
                } else if (fstep == 1) {
                    for (;;) {
                        r = read_int_or_back("  年龄（正整数）：", &age);
                        if (r == INPUT_QUIT) return INPUT_QUIT;
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  return INPUT_QUIT;
                        if (!validate_age(age)) {
                            printf("  [提示] 年龄无效（请输入 1~150），请重新输入。\n");
                            continue;
                        }
                        break;
                    }
                } else {
                    char gbuf[8] = {0};
                    for (;;) {
                        r = read_line_or_back("  性别 (0=男, 1=女)：", gbuf, sizeof(gbuf));
                        if (r == INPUT_QUIT) return INPUT_QUIT;
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  return INPUT_QUIT;
                        if (gbuf[0] != '0' && gbuf[0] != '1') {
                            printf("  [提示] 请输入 0（男）或 1（女）。\n");
                            continue;
                        }
                        gender = (gbuf[0] == '1') ? 1 : 0;
                        break;
                    }
                }

                if (back) {
                    if (fstep > 0) fstep--;
                    else           break;  /* 在第一个字段按 /b → 回到选择 1/2 */
                } else {
                    fstep++;
                }
            }

            if (fstep < 3) continue;  /* 未完成全部字段 → 重新从 1/2 开始 */

            int newId = next_patient_id++;
            if (!is_unique_patient_id(newId)) {
                printf("  [错误] 患者 ID 冲突，请重试。\n");
                next_patient_id--;
                continue;
            }
            Patient* p = create_patient(newId, name, age,
                gender == 0 ? GENDER_MALE : GENDER_FEMALE);
            if (!p) {
                printf("  [错误] 创建患者失败（内存不足）。\n");
                next_patient_id--;
                continue;
            }
            add_patient(&g_sys.patients, p);
            printf("  新患者已创建，编号：%d\n", newId);
            *out = p;
            return INPUT_OK;

        } else {
            printf("  [提示] 请输入 1 或 2。\n");
        }
    }
}

/*
 * ensure_dept_by_id_or_new
 * 让用户选择已有科室或新建科室。
 * 返回值同 ensure_patient_by_id_or_new。
 */
static int ensure_dept_by_id_or_new(Department** out) {
    *out = NULL;

    for (;;) {
        ui_divider();
        printf("  [步骤] 科室信息  [/b 上一步, /q 取消]\n");
        printf("  1. 使用已有科室编号\n");
        printf("  2. 新建科室\n");

        int choice, r;
        r = read_int_or_back("  请选择(1/2)：", &choice);
        if (r == INPUT_QUIT) return INPUT_QUIT;
        if (r == INPUT_BACK) return INPUT_BACK;
        if (r == INPUT_EOF)  return INPUT_QUIT;

        if (choice == 1) {
            for (;;) {
                int id;
                r = read_int_or_back("  请输入科室编号：", &id);
                if (r == INPUT_QUIT) return INPUT_QUIT;
                if (r == INPUT_BACK) break;
                if (r == INPUT_EOF)  return INPUT_QUIT;

                Department* d = find_department_by_id(g_sys.departments, id);
                if (!d) {
                    printf("  [提示] 未找到科室编号 %d，请重新输入。\n", id);
                    continue;
                }
                *out = d;
                return INPUT_OK;
            }

        } else if (choice == 2) {
            for (;;) {
                char name[64] = {0};
                r = read_line_or_back("  科室名称：", name, sizeof(name));
                if (r == INPUT_QUIT) return INPUT_QUIT;
                if (r == INPUT_BACK) break;
                if (r == INPUT_EOF)  return INPUT_QUIT;
                if (name[0] == '\0') {
                    printf("  [提示] 科室名称不能为空，请重新输入。\n");
                    continue;
                }
                int newId = next_dept_id++;
                Department* d = create_department(newId, name);
                if (!d) {
                    printf("  [错误] 创建科室失败（内存不足）。\n");
                    next_dept_id--;
                    continue;
                }
                add_department(&g_sys.departments, d);
                printf("  新科室已创建，编号：%d\n", newId);
                *out = d;
                return INPUT_OK;
            }

        } else {
            printf("  [提示] 请输入 1 或 2。\n");
        }
    }
}

/*
 * ensure_doctor_by_id_or_new
 * 让用户选择已有医生或新建医生。
 * 返回值同 ensure_patient_by_id_or_new。
 */
static int ensure_doctor_by_id_or_new(Department* dept, Doctor** out) {
    *out = NULL;

    for (;;) {
        ui_divider();
        printf("  [步骤] 医生信息  [/b 上一步, /q 取消]\n");
        printf("  1. 使用已有医生工号\n");
        printf("  2. 新建医生\n");

        int choice, r;
        r = read_int_or_back("  请选择(1/2)：", &choice);
        if (r == INPUT_QUIT) return INPUT_QUIT;
        if (r == INPUT_BACK) return INPUT_BACK;
        if (r == INPUT_EOF)  return INPUT_QUIT;

        if (choice == 1) {
            for (;;) {
                int id;
                r = read_int_or_back("  请输入医生工号：", &id);
                if (r == INPUT_QUIT) return INPUT_QUIT;
                if (r == INPUT_BACK) break;
                if (r == INPUT_EOF)  return INPUT_QUIT;

                Doctor* d = find_doctor_by_id(g_sys.doctors, id);
                if (!d) {
                    printf("  [提示] 未找到工号 %d 的医生，请重新输入。\n", id);
                    continue;
                }
                if (d->dept != dept) {
                    printf("  [提示] 该医生所属科室与本次挂号科室不同，建议核实保持一致。\n");
                }
                *out = d;
                return INPUT_OK;
            }

        } else if (choice == 2) {
            char name[64] = {0};
            int titleChoice = 0;
            int fstep = 0;  /* 0=姓名, 1=职称 */

            while (fstep < 2) {
                int back = 0;

                if (fstep == 0) {
                    for (;;) {
                        r = read_line_or_back("  医生姓名：", name, sizeof(name));
                        if (r == INPUT_QUIT) return INPUT_QUIT;
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  return INPUT_QUIT;
                        if (name[0] == '\0') {
                            printf("  [提示] 姓名不能为空，请重新输入。\n");
                            continue;
                        }
                        break;
                    }
                } else {
                    printf("  请选择医生职称：\n");
                    printf("  0. 实习医生  1. 住院医师  2. 主治医师  3. 住院医生\n");
                    for (;;) {
                        r = read_int_or_back("  请输入职称编号(0-3)：", &titleChoice);
                        if (r == INPUT_QUIT) return INPUT_QUIT;
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  return INPUT_QUIT;
                        if (titleChoice < 0 || titleChoice > 3) {
                            printf("  [提示] 请输入 0~3 之间的数字。\n");
                            continue;
                        }
                        break;
                    }
                }

                if (back) {
                    if (fstep > 0) fstep--;
                    else           break;
                } else {
                    fstep++;
                }
            }

            if (fstep < 2) continue;

            int newId = next_doctor_id++;
            if (!is_unique_doctor_id(newId)) {
                printf("  [错误] 医生 ID 冲突，请重试。\n");
                next_doctor_id--;
                continue;
            }
            Doctor* d = create_doctor(newId, name, (DoctorTitle)titleChoice, dept);
            if (!d) {
                printf("  [错误] 创建医生失败（内存不足）。\n");
                next_doctor_id--;
                continue;
            }
            add_doctor(&g_sys.doctors, d);
            printf("  新医生已创建，工号：%d\n", newId);
            *out = d;
            return INPUT_OK;

        } else {
            printf("  [提示] 请输入 1 或 2。\n");
        }
    }
}

// ================== 新增诊疗记录（向导式多步流程）==================

static void add_record_manual() {
    /* 步骤枚举 */
    enum {
        STEP_PATIENT  = 0,
        STEP_DEPT     = 1,
        STEP_DOCTOR   = 2,
        STEP_REGTIME  = 3,
        STEP_DIAG     = 4,
        STEP_DISPOSAL = 5,
        STEP_DONE     = 6
    };

    int step = STEP_PATIENT;

    /* 各步骤的结果变量 */
    Patient*    p    = NULL;
    Department* dept = NULL;
    Doctor*     doc  = NULL;
    SimpleTime  regTime;
    char symptom[64]    = {0};
    char result_[64]    = {0};
    char suggestion[64] = {0};
    int hasExam = 0, hasDrug = 0, hasHosp = 0;

    ui_clear();
    ui_frame_begin("新增诊疗记录");
    printf("  [提示] 输入 /b 返回上一步，/q 取消整个操作\n");
    ui_divider();

    for (;;) {
        if (step == STEP_DONE) break;

        int nav;

        switch (step) {

        /* ---- 步骤0：患者 ---- */
        case STEP_PATIENT: {
            nav = ensure_patient_by_id_or_new(&p);
            if (nav == INPUT_QUIT) { printf("  已取消。\n"); return; }
            if (nav == INPUT_BACK) {
                /* 已是第一步，停留 */
                printf("  [提示] 已是第一步，无法继续返回。\n");
                continue;
            }
            if (p->recordCount >= MAX_RECORDS_PER_PAT) {
                printf("  [提示] 该患者诊疗记录已达上限（%d条），无法继续。\n", MAX_RECORDS_PER_PAT);
                p = NULL;
                continue;
            }
            step++;
            break;
        }

        /* ---- 步骤1：科室 ---- */
        case STEP_DEPT: {
            nav = ensure_dept_by_id_or_new(&dept);
            if (nav == INPUT_QUIT) { printf("  已取消。\n"); return; }
            if (nav == INPUT_BACK) { step--; continue; }
            step++;
            break;
        }

        /* ---- 步骤2：医生 ---- */
        case STEP_DOCTOR: {
            nav = ensure_doctor_by_id_or_new(dept, &doc);
            if (nav == INPUT_QUIT) { printf("  已取消。\n"); return; }
            if (nav == INPUT_BACK) { step--; continue; }
            step++;
            break;
        }

        /* ---- 步骤3：挂号时间 ---- */
        case STEP_REGTIME: {
            ui_divider();
            printf("  [步骤] 挂号时间  [/b 上一步, /q 取消]\n");
            nav = prompt_and_read_time_or_back(
                "  挂号时间 (MM-DD HH:MM)：", &regTime);
            if (nav == INPUT_QUIT) { printf("  已取消。\n"); return; }
            if (nav == INPUT_BACK) { step--; continue; }
            if (nav == INPUT_EOF)  { printf("  已取消。\n"); return; }

            if (!check_registration_rules(p, doc, dept, &regTime)) {
                /* 业务规则校验失败：重新输入，不退出 */
                continue;
            }
            step++;
            break;
        }

        /* ---- 步骤4：诊断信息 ---- */
        case STEP_DIAG: {
            ui_divider();
            printf("  [步骤] 诊断信息  [/b 上一步, /q 取消]\n");

            int back_to_prev = 0;
            int dfstep = 0;  /* 0=症状, 1=结果, 2=建议 */

            while (dfstep < 3 && !back_to_prev) {
                int back = 0, r;

                if (dfstep == 0) {
                    for (;;) {
                        r = read_line_or_back("  症状描述：", symptom, sizeof(symptom));
                        if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                        if (symptom[0] == '\0') {
                            printf("  [提示] 症状描述不能为空。\n");
                            continue;
                        }
                        break;
                    }
                } else if (dfstep == 1) {
                    for (;;) {
                        r = read_line_or_back("  诊断结论：", result_, sizeof(result_));
                        if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                        if (result_[0] == '\0') {
                            printf("  [提示] 诊断结论不能为空。\n");
                            continue;
                        }
                        break;
                    }
                } else {
                    for (;;) {
                        r = read_line_or_back("  医嘱建议：", suggestion, sizeof(suggestion));
                        if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                        if (r == INPUT_BACK) { back = 1; break; }
                        if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                        if (suggestion[0] == '\0') {
                            printf("  [提示] 医嘱建议不能为空。\n");
                            continue;
                        }
                        break;
                    }
                }

                if (back) {
                    if (dfstep > 0) dfstep--;
                    else             back_to_prev = 1;
                } else {
                    dfstep++;
                }
            }

            if (back_to_prev) { step--; continue; }
            step++;
            break;
        }

        /* ---- 步骤5：处置信息 ---- */
        case STEP_DISPOSAL: {
            ui_divider();
            printf("  [步骤] 处置信息  [/b 上一步, /q 取消]\n");

            hasExam = 0; hasDrug = 0; hasHosp = 0;
            int r;

            /* 是否检查 */
            for (;;) {
                r = read_bool01_or_back("  是否检查项目 (1=是,0=否)：", &hasExam);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) break;
                if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                break;
            }
            if (r == INPUT_BACK) { step--; continue; }

            /* 是否开药 */
            for (;;) {
                r = read_bool01_or_back("  是否开具药品 (1=是,0=否)：", &hasDrug);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { hasExam = 0; break; }
                if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                break;
            }
            if (r == INPUT_BACK) { step--; continue; }

            /* 是否住院 */
            for (;;) {
                r = read_bool01_or_back("  是否需要住院 (1=是,0=否)：", &hasHosp);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { hasDrug = 0; break; }
                if (r == INPUT_EOF)  { printf("  已取消。\n"); return; }
                break;
            }
            if (r == INPUT_BACK) { step--; continue; }

            if (!hasExam && !hasDrug && !hasHosp) {
                printf("  [提示] 处置信息不能全为空，请至少选择一项。\n");
                continue;
            }
            step++;
            break;
        }

        default:
            step++;
            break;
        }
    }

    /* ---- 收集检查项目 ---- */
    DisposalInfo disp;
    memset(&disp, 0, sizeof(disp));
    disp.hasExam     = hasExam;
    disp.hasDrug     = hasDrug;
    disp.hasHospital = hasHosp;

    if (disp.hasExam) {
        ui_divider();
        printf("  [步骤] 检查项目录入  [/q 取消]\n");
        int examCount;
        for (;;) {
            int r = read_int_or_back("  检查项目数量（1~5）：", &examCount);
            if (r <= 0) { printf("  已取消。\n"); return; }
            if (examCount < 1 || examCount > 5) {
                printf("  [提示] 数量需在 1~5 之间。\n");
                continue;
            }
            break;
        }

        ExamItem* head = NULL;
        double totalExamFee = 0.0;
        for (int i = 0; i < examCount; ++i) {
            printf("\n  第 %d 个检查项目：\n", i + 1);
            char examName[64] = {0};
            double fee = 0.0;

            for (;;) {
                int r = read_line_or_back("  项目名称：", examName, sizeof(examName));
                if (r <= 0) { printf("  已取消。\n"); return; }
                if (examName[0] == '\0') {
                    printf("  [提示] 项目名称不能为空。\n");
                    continue;
                }
                break;
            }
            for (;;) {
                int r = read_double_or_back("  项目费用：", &fee);
                if (r <= 0) { printf("  已取消。\n"); return; }
                if (!validate_money(fee)) {
                    printf("  [提示] 费用无效，请重新输入。\n");
                    fee = 0.0;
                    continue;
                }
                break;
            }

            ExamItem* item = (ExamItem*)malloc(sizeof(ExamItem));
            if (!item) {
                printf("  内存不足，停止添加检查项目。\n");
                break;
            }
            strncpy(item->name, examName, MAX_NAME_LEN);
            item->name[MAX_NAME_LEN - 1] = '\0';
            item->fee = fee;
            item->next = head;
            head = item;
            totalExamFee += fee;
        }
        disp.exam.items    = head;
        disp.exam.totalFee = totalExamFee;
    }

    /* ---- 收集药品信息 ---- */
    if (disp.hasDrug) {
        ui_divider();
        printf("  [步骤] 药品信息录入  [/q 取消]\n");
        int drugCount;
        for (;;) {
            int r = read_int_or_back("  本次开药种类数（最多 10 种）：", &drugCount);
            if (r <= 0) { printf("  已取消。\n"); return; }
            if (!validate_drug_count_per_visit(drugCount)) {
                printf("  [提示] 药品种数无效（1~10）。\n");
                continue;
            }
            break;
        }

        DrugItem* head = NULL;
        double totalDrugFee = 0.0;
        int drug_alloc_ok = 1;

        for (int i = 0; i < drugCount && drug_alloc_ok; ++i) {
            printf("\n  第 %d 种药品：\n", i + 1);
            int drugId;
            for (;;) {
                int r = read_int_or_back("  药品编号：", &drugId);
                if (r <= 0) { printf("  已取消。\n"); return; }
                Drug* d = find_drug_by_id(g_sys.drugs, drugId);
                if (!d) {
                    printf("  [提示] 未找到该药品，请重新输入。\n");
                    continue;
                }
                int quantity;
                for (;;) {
                    int r2 = read_int_or_back("  数量（1~100）：", &quantity);
                    if (r2 <= 0) { printf("  已取消。\n"); return; }
                    if (!validate_box_per_drug(quantity)) {
                        printf("  [提示] 数量无效（1~100）。\n");
                        continue;
                    }
                    break;
                }
                if (d->stock < quantity) {
                    printf("  [提示] 药品 %s（ID %d）库存不足（当前库存 %d 盒），请重新输入。\n",
                        d->name, d->id, d->stock);
                    continue;
                }
                DrugItem* it = (DrugItem*)malloc(sizeof(DrugItem));
                if (!it) {
                    printf("  内存不足，停止添加药品。\n");
                    drug_alloc_ok = 0;
                    break;
                }
                it->drugId    = d->id;
                it->quantity  = quantity;
                it->unitPrice = d->price;
                it->next      = head;
                head          = it;
                d->stock -= quantity;
                {
                    DrugStockRecord* sr = create_drug_stock_record(
                        next_drug_stock_id++, d->id, quantity, 0, regTime);
                    if (sr) {
                        add_drug_stock_record(&g_sys.drugStocks, sr);
                        append_drug_stock_record_to_file(sr, "data/drug_stock_records.txt");
                    }
                }
                totalDrugFee += d->price * quantity;
                break;
            }
        }
        disp.prescription.items    = head;
        disp.prescription.totalFee = totalDrugFee;
    }

    /* ---- 收集住院信息 ---- */
    if (disp.hasHospital) {
        ui_divider();
        printf("  [步骤] 住院信息录入  [/q 取消]\n");
        HospitalizationInfo* h = &disp.hospitalization;
        int r;
        int hosp_ok = 1;

        r = prompt_and_read_time_or_back("  开始时间 (MM-DD HH:MM)：", &h->startTime);
        if (r <= 0) {
            printf("  [提示] 未填写开始时间，取消住院设置。\n");
            hosp_ok = 0;
        }

        if (hosp_ok) {
            r = prompt_and_read_time_or_back("  预计出院时间 (MM-DD HH:MM)：", &h->expectedLeaveTime);
            if (r <= 0) {
                printf("  [提示] 未填写出院时间，取消住院设置。\n");
                hosp_ok = 0;
            }
        }

        if (hosp_ok) {
            double deposit = 0.0;
            for (;;) {
                r = read_double_or_back("  住院押金（元，建议 >=200*天数）：", &deposit);
                if (r <= 0) {
                    printf("  [提示] 未填写押金，取消住院设置。\n");
                    hosp_ok = 0;
                    break;
                }
                if (!validate_deposit(deposit, &h->startTime, &h->expectedLeaveTime)) {
                    printf("  [提示] 住院押金不合规，请重新输入。\n");
                    continue;
                }
                h->deposit     = deposit;
                break;
            }
        }

        if (hosp_ok) {
            h->plannedDays  = days_between(&h->startTime, &h->expectedLeaveTime);
            h->inHospital   = 1;
            Bed* bed        = allocate_free_bed(g_sys.beds, dept->id);
            if (!bed) {
                printf("  [提示] 无可用床位，住院设置失败。\n");
                hosp_ok = 0;
            } else {
                h->bed               = bed;
                h->actualLeaveTime.month = 0;
                h->totalFee          = 0.0;
                printf("  住院床位分配成功，床位号：%d\n", bed->id);
            }
        }

        if (!hosp_ok) {
            disp.hasHospital = 0;
        }
    }

    /* ---- 生成挂号信息 ---- */
    int seqNo = get_seq_no_of_day(&regTime);
    long long regId = generate_reg_id_full(&regTime, dept->id, doc->id, seqNo);
    if (!is_unique_reg_id(regId)) {
        printf("  挂号单号冲突，取消新增记录。\n");
        return;
    }

    printf("  挂号成功：");
    print_time(&regTime);
    printf("  顺序号=%d  挂号单号=%lld\n", seqNo, regId);

    /* ---- 创建诊疗记录 ---- */
    MedicalRecord* rec = create_record();
    if (!rec) {
        printf("  创建诊疗记录失败（内存不足）。\n");
        return;
    }

    rec->id           = next_record_id++;
    rec->patient      = p;
    rec->doctorInfo   = doc;
    rec->isCancelled  = 0;

    rec->regInfo.regId       = regId;
    rec->regInfo.regTime     = regTime;
    rec->regInfo.dept        = dept;
    rec->regInfo.doctor      = doc;
    rec->regInfo.seqNoOfDay  = seqNo;

    strncpy(rec->diagInfo.symptom,    symptom,    MAX_STR_LEN);
    strncpy(rec->diagInfo.result,     result_,    MAX_STR_LEN);
    strncpy(rec->diagInfo.suggestion, suggestion, MAX_STR_LEN);
    rec->diagInfo.symptom[MAX_STR_LEN - 1]    = '\0';
    rec->diagInfo.result[MAX_STR_LEN - 1]     = '\0';
    rec->diagInfo.suggestion[MAX_STR_LEN - 1] = '\0';

    rec->dispInfo = disp;
    add_record(&g_sys.records, rec);
    p->recordCount++;

    double income = 0.0;
    if (disp.hasExam)  income += disp.exam.totalFee;
    if (disp.hasDrug)  income += disp.prescription.totalFee;
    /* 住院费用在出院结算时再计算 */
    g_sys.fund += income;

    ui_divider();
    printf("  新增诊疗记录成功！记录ID=%d  挂号单号=%lld  本次费用=%.2f  营业额=%.2f\n",
        rec->id, rec->regInfo.regId, income, g_sys.fund);
}

// ================== 修改 / 删除 ==================

static void modify_record() {
    ui_clear();
    ui_frame_begin("修改诊疗记录");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();

    int id;
    if (read_int_or_back("  请输入要修改的诊疗记录 ID（/q 取消）：", &id) <= 0) return;

    MedicalRecord* rec = find_record_by_id(g_sys.records, id);
    if (!rec) {
        printf("  未找到该记录。\n");
        ui_pause();
        return;
    }

    /* 医生权限：只能修改自己诊治的记录 */
    if (g_session.role == ROLE_DOCTOR) {
        if (!rec->doctorInfo || rec->doctorInfo->id != g_session.doctorId) {
            printf("  权限不足：只能修改本人诊治的记录。\n");
            ui_pause();
            return;
        }
    }

    rec->isCancelled = 1;
    printf("  已将原记录 ID=%d 标记为已撤销，接下来录入新记录……\n", id);
    ui_pause();
    add_record_manual();
}

static void delete_record_cli() {
    ui_clear();
    ui_frame_begin("删除诊疗记录");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();

    int id;
    if (read_int_or_back("  请输入要删除的诊疗记录 ID（/q 取消）：", &id) <= 0) return;

    MedicalRecord* rec = find_record_by_id(g_sys.records, id);
    if (rec && rec->dispInfo.hasHospital && rec->dispInfo.hospitalization.inHospital) {
        release_bed(rec->dispInfo.hospitalization.bed);
    }
    delete_record(&g_sys.records, id);
    printf("  已将记录 ID=%d 从系统中删除。\n", id);
    ui_pause();
}

// ================== 菜单中的查询/统计封装 ==================

static void handle_query_department() {
    ui_clear();
    ui_frame_begin("按科室查询记录");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    int deptId;
    if (read_int_or_back("  请输入科室编号（/q 取消）：", &deptId) <= 0) return;
    query_records_by_department(deptId, 1);
    ui_pause();
}

/* 医生专用：直接使用登录医生所在科室 */
static void handle_query_dept_own() {
    if (g_session.deptId == 0) {
        printf("  当前未关联任何科室，无法执行本科室查询。\n");
        return;
    }
    printf("  自动使用本科室（科室ID=%d）\n", g_session.deptId);
    query_records_by_department(g_session.deptId, 1);
}

static void handle_query_doctor() {
    ui_clear();
    ui_frame_begin("按医生查询记录");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    int doctorId;
    if (read_int_or_back("  请输入医生工号（/q 取消）：", &doctorId) <= 0) return;
    query_records_by_doctor(doctorId, 1);
    ui_pause();
}

/* 医生专用：只能查询本科室医生 */
static void handle_query_doctor_in_dept() {
    ui_clear();
    ui_frame_begin("按医生查询记录（本科室）");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    int doctorId;
    if (read_int_or_back("  请输入医生工号（/q 取消）：", &doctorId) <= 0) return;
    Doctor* d = find_doctor_by_id(g_sys.doctors, doctorId);
    if (!d) {
        printf("  未找到该医生。\n");
        return;
    }
    if (!d->dept || d->dept->id != g_session.deptId) {
        printf("  权限不足：医生 %d 不属于本科室（科室ID=%d）。\n",
            doctorId, g_session.deptId);
        return;
    }
    query_records_by_doctor(doctorId, 1);
    ui_pause();
}

static void handle_query_patient() {
    ui_clear();
    ui_frame_begin("按患者查询记录");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    int patId;
    if (read_int_or_back("  请输入患者编号（/q 取消）：", &patId) <= 0) return;
    query_records_by_patient(patId, 1);
    ui_pause();
}

/* 患者专用：自动使用本人编号 */
static void handle_query_patient_self() {
    printf("  自动查询本人（患者编号=%d）\n", g_session.patientId);
    query_records_by_patient(g_session.patientId, 1);
    ui_pause();
}

static void handle_time_range_stat() {
    ui_clear();
    ui_frame_begin("时间段统计");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    SimpleTime start, end;
    int r;
    for (;;) {
        r = prompt_and_read_time_or_back("  开始时间 (MM-DD HH:MM)：", &start);
        if (r <= 0) { printf("  已取消。\n"); return; }
        break;
    }
    for (;;) {
        r = prompt_and_read_time_or_back("  结束时间 (MM-DD HH:MM)：", &end);
        if (r <= 0) { printf("  已取消。\n"); return; }
        break;
    }
    stat_records_in_timerange(&start, &end);
    ui_pause();
}

/* 患者专用：时间段统计仅显示本人记录 */
static void handle_time_range_stat_patient() {
    ui_clear();
    ui_frame_begin("时间段统计（个人）");
    printf("  [提示] 输入 /q 取消\n");
    ui_divider();
    SimpleTime start, end;
    int r;
    for (;;) {
        r = prompt_and_read_time_or_back("  开始时间 (MM-DD HH:MM)：", &start);
        if (r <= 0) { printf("  已取消。\n"); return; }
        break;
    }
    for (;;) {
        r = prompt_and_read_time_or_back("  结束时间 (MM-DD HH:MM)：", &end);
        if (r <= 0) { printf("  已取消。\n"); return; }
        break;
    }

    int count = 0;
    double totalFee = 0.0;
    MedicalRecord* p = g_sys.records;
    while (p) {
        if (!p->isCancelled && p->patient &&
            p->patient->id == g_session.patientId &&
            cmp_time(&p->regInfo.regTime, &start) >= 0 &&
            cmp_time(&p->regInfo.regTime, &end)   <= 0) {

            double fee = 0.0;
            if (p->dispInfo.hasExam)     fee += p->dispInfo.exam.totalFee;
            if (p->dispInfo.hasDrug)     fee += p->dispInfo.prescription.totalFee;
            if (p->dispInfo.hasHospital) fee += p->dispInfo.hospitalization.totalFee;

            printf("  [记录ID] %d  时间: %02d-%02d %02d:%02d  费用: %.2f 元\n",
                p->id,
                p->regInfo.regTime.month, p->regInfo.regTime.day,
                p->regInfo.regTime.hour,  p->regInfo.regTime.minute,
                fee);
            printf("    科室: %s  诊断: %s\n",
                p->regInfo.dept ? p->regInfo.dept->name : "未知",
                p->diagInfo.result);
            printf("  --------------------------------------------\n");

            totalFee += fee;
            count++;
        }
        p = p->next;
    }
    printf("  时间段内本人共 %d 条记录，总费用 %.2f 元\n", count, totalFee);
    ui_pause();
}

/* 医生专用：本科室医生工作量统计 */
static void handle_doctor_workload_in_dept() {
    int myDeptId = g_session.deptId;
    ui_clear();
    ui_frame_begin("本科室医生工作量统计");
    ui_divider();
    printf("  科室ID：%d\n", myDeptId);
    ui_divider();

    int found = 0;
    Doctor* d = g_sys.doctors;
    while (d) {
        if (d->dept && d->dept->id == myDeptId) {
            int visits = 0;
            MedicalRecord* r = g_sys.records;
            while (r) {
                if (!r->isCancelled && r->doctorInfo && r->doctorInfo->id == d->id)
                    visits++;
                r = r->next;
            }
            printf("  工号:%d  姓名:%s  接诊次数:%d\n", d->id, d->name, visits);
            found++;
        }
        d = d->next;
    }
    if (!found) printf("  本科室暂无医生数据。\n");
    ui_pause();
}

static void menu_admin_stats(void) {
    const char* const items[] = {
        "营业额统计",
        "住院统计",
        "综合报表",
        "返回"
    };

    for (;;) {
        int idx = menu_select("医院医疗管理系统  [统计与报表中心]", items, 4, 0);
        if (idx < 0 || idx == 3) return;
        ui_clear();
        switch (idx) {
        case 0: stat_total_revenue();    break;
        case 1: stat_inpatient_report(); break;
        case 2: report_for_admin_view(); break;
        }
        ui_pause();
    }
}


// ================== 次辅助：列表打印 ==================

static void print_gender_str(int g) {
    printf("%s", g == 0 ? "男" : "女");
}

static void print_doctor_title_str(int t) {
    const char* titles[] = {"主任医师", "副主任医师", "主治医师", "住院医师"};
    if (t >= 0 && t <= 3) printf("%s", titles[t]);
    else printf("未知");
}

static void print_room_type_str(int t) {
    const char* types[] = {"普通间", "标准间", "VIP间"};
    if (t >= 0 && t <= 2) printf("%s", types[t]);
    else printf("未知");
}

static void print_bed_status_str(int s) {
    const char* statuses[] = {"空闲", "已占用", "维护中"};
    if (s >= 0 && s <= 2) printf("%s", statuses[s]);
    else printf("未知");
}

// ================== 患者管理中心 ==================

static void list_patients_cli(void) {
    ui_clear();
    ui_frame_begin("患者列表");
    Patient* p = g_sys.patients;
    int cnt = 0;
    if (!p) {
        printf("  暂无患者记录。\n");
    } else {
        printf("  %-8s %-16s %-6s %-4s %-6s\n", "编号", "姓名", "年龄", "性别", "记录数");
        ui_divider();
        while (p) {
            printf("  %-8d %-16s %-6d ", p->id, p->name, p->age);
            print_gender_str((int)p->gender);
            printf("  %-6d\n", p->recordCount);
            cnt++;
            p = p->next;
        }
    }
    printf("\n  共 %d 位患者。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_patient_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增患者  [/b 上一步, /q 取消]");
    ui_divider();
    char name[64] = {0};
    int  age = 0;
    int  gender = 0;
    int  fstep = 0;
    int  r;
    while (fstep < 3) {
        int back = 0;
        if (fstep == 0) {
            for (;;) {
                r = read_line_or_back("  姓名：", name, sizeof(name));
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (name[0] == '\0') { printf("  [提示] 姓名不能为空。\n"); continue; }
                break;
            }
        } else if (fstep == 1) {
            for (;;) {
                r = read_int_or_back("  年龄（整数）：", &age);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (!validate_age(age)) { printf("  [提示] 年龄无效，请输入 1~150。\n"); continue; }
                break;
            }
        } else {
            char gbuf[8] = {0};
            for (;;) {
                r = read_line_or_back("  性别 (0=男, 1=女)：", gbuf, sizeof(gbuf));
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (gbuf[0] != '0' && gbuf[0] != '1') {
                    printf("  [提示] 请输入 0（男）或 1（女）。\n");
                    continue;
                }
                gender = (gbuf[0] == '1') ? 1 : 0;
                break;
            }
        }
        if (back) { if (fstep > 0) fstep--; else { printf("  已取消。\n"); return; } }
        else fstep++;
    }
    int newId = next_patient_id++;
    if (!is_unique_patient_id(newId)) {
        printf("  [错误] 患者 ID 冲突，请重试。\n");
        next_patient_id--;
        ui_pause(); return;
    }
    Patient* p = create_patient(newId, name, age,
        gender == 0 ? GENDER_MALE : GENDER_FEMALE);
    if (!p) {
        printf("  [错误] 创建患者失败（内存不足）。\n");
        next_patient_id--;
        ui_pause(); return;
    }
    add_patient(&g_sys.patients, p);
    printf("  新患者已创建，编号：%d\n", newId);
    ui_pause();
}

static void find_patient_cli(void) {
    ui_clear();
    ui_frame_begin("查找患者  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入患者编号：", &id) <= 0) return;
    Patient* p = find_patient_by_id(g_sys.patients, id);
    if (!p) {
        printf("  未找到编号 %d 的患者。\n", id);
    } else {
        printf("  编号：%d  姓名：%s  年龄：%d  性别：", p->id, p->name, p->age);
        print_gender_str((int)p->gender);
        printf("  记录数：%d\n", p->recordCount);
    }
    ui_pause();
}

static void modify_patient_cli(void) {
    ui_clear();
    ui_frame_begin("修改患者信息  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入患者编号：", &id) <= 0) return;
    Patient* p = find_patient_by_id(g_sys.patients, id);
    if (!p) {
        printf("  未找到编号 %d 的患者。\n", id);
        ui_pause(); return;
    }
    printf("  当前姓名：%s  年龄：%d\n", p->name, p->age);
    ui_divider();
    char name[64] = {0};
    int age = 0, r;
    for (;;) {
        r = read_line_or_back("  新姓名（直接回车保留原值）：", name, sizeof(name));
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        break;
    }
    for (;;) {
        r = read_int_or_back("  新年龄（输入 0 保留原值）：", &age);
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (age != 0 && !validate_age(age)) { printf("  [提示] 年龄无效，请重新输入。\n"); continue; }
        break;
    }
    if (name[0] != '\0') { strncpy(p->name, name, MAX_NAME_LEN); p->name[MAX_NAME_LEN-1] = '\0'; }
    if (age != 0) p->age = age;
    printf("  患者信息已更新。\n");
    ui_pause();
}

static void delete_patient_cli(void) {
    ui_clear();
    ui_frame_begin("删除患者  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入患者编号：", &id) <= 0) return;
    Patient* p = find_patient_by_id(g_sys.patients, id);
    if (!p) {
        printf("  未找到编号 %d 的患者。\n", id);
        ui_pause(); return;
    }
    if (p->recordCount > 0) {
        printf("  [提示] 该患者有 %d 条诊疗记录，无法删除，请先撤销相关记录。\n", p->recordCount);
        ui_pause(); return;
    }
    Patient** pp = &g_sys.patients;
    while (*pp && (*pp)->id != id) pp = &(*pp)->next;
    if (*pp) {
        Patient* del = *pp;
        *pp = del->next;
        free(del);
        printf("  患者编号 %d 已删除。\n", id);
    }
    ui_pause();
}

static void menu_admin_patients(void) {
    const char* const items[] = {
        "患者列表",
        "新增患者",
        "按编号查找",
        "修改患者信息",
        "删除患者",
        "按患者查询诊疗记录",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [患者管理]", items, 7, 0);
        if (idx < 0 || idx == 6) return;
        ui_clear();
        switch (idx) {
        case 0: list_patients_cli();   break;
        case 1: add_patient_new_cli(); break;
        case 2: find_patient_cli();    break;
        case 3: modify_patient_cli();  break;
        case 4: delete_patient_cli();  break;
        case 5: handle_query_patient(); break;
        }
    }
}

// ================== 医生管理中心 ==================

static void list_doctors_cli(void) {
    ui_clear();
    ui_frame_begin("医生列表");
    Doctor* d = g_sys.doctors;
    int cnt = 0;
    if (!d) {
        printf("  暂无医生记录。\n");
    } else {
        printf("  %-8s %-16s %-14s\n", "工号", "姓名", "职称");
        ui_divider();
        while (d) {
            printf("  %-8d %-16s ", d->id, d->name);
            print_doctor_title_str((int)d->title);
            printf("\n");
            cnt++;
            d = d->next;
        }
    }
    printf("\n  共 %d 位医生。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_doctor_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增医生  [/b 上一步, /q 取消]");
    ui_divider();
    char name[64] = {0};
    int titleChoice = 0, deptId = 0, fstep = 0, r;
    while (fstep < 3) {
        int back = 0;
        if (fstep == 0) {
            for (;;) {
                r = read_line_or_back("  医生姓名：", name, sizeof(name));
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (name[0] == '\0') { printf("  [提示] 姓名不能为空。\n"); continue; }
                break;
            }
        } else if (fstep == 1) {
            printf("  职称：0.主任医师  1.副主任医师  2.主治医师  3.住院医师\n");
            for (;;) {
                r = read_int_or_back("  请选职称编号(0-3)：", &titleChoice);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (titleChoice < 0 || titleChoice > 3) { printf("  [提示] 请输入 0~3。\n"); continue; }
                break;
            }
        } else {
            for (;;) {
                r = read_int_or_back("  所属科室编号（-1 无）：", &deptId);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (deptId != -1 && !find_department_by_id(g_sys.departments, deptId)) {
                    printf("  [提示] 未找到科室 %d，请重新输入。\n", deptId);
                    continue;
                }
                break;
            }
        }
        if (back) { if (fstep > 0) fstep--; else { printf("  已取消。\n"); return; } }
        else fstep++;
    }
    int newId = next_doctor_id++;
    if (!is_unique_doctor_id(newId)) {
        printf("  [错误] 医生 ID 冲突，请重试。\n");
        next_doctor_id--;
        ui_pause(); return;
    }
    Department* dept = (deptId == -1) ? NULL : find_department_by_id(g_sys.departments, deptId);
    Doctor* d = create_doctor(newId, name, (DoctorTitle)titleChoice, dept);
    if (!d) {
        printf("  [错误] 创建医生失败（内存不足）。\n");
        next_doctor_id--;
        ui_pause(); return;
    }
    add_doctor(&g_sys.doctors, d);
    printf("  新医生已创建，工号：%d\n", newId);
    ui_pause();
}

static void find_doctor_cli(void) {
    ui_clear();
    ui_frame_begin("查找医生  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入医生工号：", &id) <= 0) return;
    Doctor* d = find_doctor_by_id(g_sys.doctors, id);
    if (!d) {
        printf("  未找到工号 %d 的医生。\n", id);
    } else {
        printf("  工号：%d  姓名：%s  职称：", d->id, d->name);
        print_doctor_title_str((int)d->title);
        printf("  科室：%s\n", d->dept ? d->dept->name : "无");
    }
    ui_pause();
}

static void delete_doctor_cli(void) {
    ui_clear();
    ui_frame_begin("删除医生  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入医生工号：", &id) <= 0) return;
    Doctor** pp = &g_sys.doctors;
    while (*pp && (*pp)->id != id) pp = &(*pp)->next;
    if (!*pp) {
        printf("  未找到工号 %d 的医生。\n", id);
        ui_pause(); return;
    }
    Doctor* del = *pp;
    *pp = del->next;
    free(del);
    printf("  医生工号 %d 已删除。\n", id);
    ui_pause();
}

static void modify_doctor_cli(void) {
    ui_clear();
    ui_frame_begin("修改医生信息  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入医生工号：", &id) <= 0) return;
    Doctor* d = find_doctor_by_id(g_sys.doctors, id);
    if (!d) {
        printf("  未找到工号 %d 的医生。\n", id);
        ui_pause(); return;
    }
    printf("  当前姓名：%s\n", d->name);
    ui_divider();
    char name[64] = {0};
    int r;
    for (;;) {
        r = read_line_or_back("  新姓名（直接回车保留）：", name, sizeof(name));
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        break;
    }
    if (name[0] != '\0') {
        strncpy(d->name, name, MAX_NAME_LEN);
        d->name[MAX_NAME_LEN-1] = '\0';
    }
    printf("  医生信息已更新。\n");
    ui_pause();
}

static void menu_admin_doctors(void) {
    const char* const items[] = {
        "医生列表",
        "新增医生",
        "按工号查找",
        "修改医生信息",
        "删除医生",
        "按医生查询诊疗记录",
        "医生工作量统计",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [医生管理]", items, 8, 0);
        if (idx < 0 || idx == 7) return;
        ui_clear();
        switch (idx) {
        case 0: list_doctors_cli();         break;
        case 1: add_doctor_new_cli();       break;
        case 2: find_doctor_cli();          break;
        case 3: modify_doctor_cli();        break;
        case 4: delete_doctor_cli();        break;
        case 5: handle_query_doctor();      break;
        case 6: stat_doctor_workload(); ui_pause(); break;
        }
    }
}

// ================== 科室管理中心 ==================

static void list_departments_cli(void) {
    ui_clear();
    ui_frame_begin("科室列表");
    Department* d = g_sys.departments;
    int cnt = 0;
    if (!d) {
        printf("  暂无科室记录。\n");
    } else {
        printf("  %-8s %-20s\n", "科室编号", "科室名称");
        ui_divider();
        while (d) {
            printf("  %-8d %-20s\n", d->id, d->name);
            cnt++;
            d = d->next;
        }
    }
    printf("\n  共 %d 个科室。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_department_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增科室  [/q 取消]");
    ui_divider();
    char name[64] = {0};
    for (;;) {
        int r = read_line_or_back("  科室名称：", name, sizeof(name));
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (name[0] == '\0') { printf("  [提示] 名称不能为空。\n"); continue; }
        break;
    }
    int newId = next_dept_id++;
    Department* d = create_department(newId, name);
    if (!d) {
        printf("  [错误] 创建科室失败（内存不足）。\n");
        next_dept_id--;
        ui_pause(); return;
    }
    add_department(&g_sys.departments, d);
    printf("  新科室已创建，编号：%d\n", newId);
    ui_pause();
}

static void find_department_cli_ui(void) {
    ui_clear();
    ui_frame_begin("查找科室  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入科室编号：", &id) <= 0) return;
    Department* d = find_department_by_id(g_sys.departments, id);
    if (!d) {
        printf("  未找到科室编号 %d。\n", id);
    } else {
        printf("  编号：%d  名称：%s\n", d->id, d->name);
    }
    ui_pause();
}

static void modify_department_cli(void) {
    ui_clear();
    ui_frame_begin("修改科室名称  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入科室编号：", &id) <= 0) return;
    Department* d = find_department_by_id(g_sys.departments, id);
    if (!d) {
        printf("  未找到科室编号 %d。\n", id);
        ui_pause(); return;
    }
    printf("  当前名称：%s\n", d->name);
    char name[64] = {0};
    for (;;) {
        int r = read_line_or_back("  新名称：", name, sizeof(name));
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (name[0] == '\0') { printf("  [提示] 名称不能为空。\n"); continue; }
        break;
    }
    strncpy(d->name, name, MAX_NAME_LEN);
    d->name[MAX_NAME_LEN-1] = '\0';
    printf("  科室名称已更新。\n");
    ui_pause();
}

static void delete_department_cli(void) {
    ui_clear();
    ui_frame_begin("删除科室  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入科室编号：", &id) <= 0) return;
    Department** pp = &g_sys.departments;
    while (*pp && (*pp)->id != id) pp = &(*pp)->next;
    if (!*pp) {
        printf("  未找到科室编号 %d。\n", id);
        ui_pause(); return;
    }
    Department* del = *pp;
    *pp = del->next;
    free(del);
    printf("  科室编号 %d 已删除。\n", id);
    ui_pause();
}

static void menu_admin_departments(void) {
    const char* const items[] = {
        "科室列表",
        "新增科室",
        "按编号查找",
        "修改科室名称",
        "删除科室",
        "按科室查询诊疗记录",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [科室管理]", items, 7, 0);
        if (idx < 0 || idx == 6) return;
        ui_clear();
        switch (idx) {
        case 0: list_departments_cli();    break;
        case 1: add_department_new_cli();  break;
        case 2: find_department_cli_ui();  break;
        case 3: modify_department_cli();   break;
        case 4: delete_department_cli();   break;
        case 5: handle_query_department(); break;
        }
    }
}

// ================== 病房管理中心 ==================

static int next_ward_id = 80001;

static Ward* find_ward_by_id_sys(int id) {
    Ward* w = g_sys.wards;
    while (w) { if (w->id == id) return w; w = w->next; }
    return NULL;
}

static void list_wards_cli(void) {
    ui_clear();
    ui_frame_begin("病房列表");
    Ward* w = g_sys.wards;
    int cnt = 0;
    if (!w) {
        printf("  暂无病房记录。\n");
    } else {
        printf("  %-8s %-10s %-8s\n", "病房编号", "房型", "科室ID");
        ui_divider();
        while (w) {
            printf("  %-8d ", w->id);
            print_room_type_str((int)w->type);
            printf("     %-8d\n", w->deptId);
            cnt++;
            w = w->next;
        }
    }
    printf("\n  共 %d 个病房。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_ward_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增病房  [/b 上一步, /q 取消]");
    ui_divider();
    int typeChoice = 0, deptId = 0, fstep = 0, r;
    while (fstep < 2) {
        int back = 0;
        if (fstep == 0) {
            printf("  房型：0.普通间  1.标准间  2.VIP间\n");
            for (;;) {
                r = read_int_or_back("  请选房型(0-2)：", &typeChoice);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (typeChoice < 0 || typeChoice > 2) { printf("  [提示] 请输入 0~2。\n"); continue; }
                break;
            }
        } else {
            for (;;) {
                r = read_int_or_back("  所属科室编号：", &deptId);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                break;
            }
        }
        if (back) { if (fstep > 0) fstep--; else { printf("  已取消。\n"); return; } }
        else fstep++;
    }
    int newId = next_ward_id++;
    Ward* w = create_ward(newId, (RoomType)typeChoice, deptId);
    if (!w) {
        printf("  [错误] 创建病房失败（内存不足）。\n");
        next_ward_id--;
        ui_pause(); return;
    }
    add_ward(&g_sys.wards, w);
    printf("  新病房已创建，编号：%d\n", newId);
    ui_pause();
}

static void list_beds_cli(void) {
    ui_clear();
    ui_frame_begin("床位列表");
    Bed* b = g_sys.beds;
    int cnt = 0;
    if (!b) {
        printf("  暂无床位记录。\n");
    } else {
        printf("  %-8s %-8s %-10s\n", "床位号", "病房ID", "状态");
        ui_divider();
        while (b) {
            printf("  %-8d %-8d ", b->id, b->ward ? b->ward->id : -1);
            print_bed_status_str((int)b->status);
            printf("\n");
            cnt++;
            b = b->next;
        }
    }
    printf("\n  共 %d 张床位。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_bed_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增床位  [/q 取消]");
    ui_divider();
    int wardId;
    for (;;) {
        int r = read_int_or_back("  所属病房编号：", &wardId);
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (!find_ward_by_id_sys(wardId)) {
            printf("  [提示] 未找到病房 %d，请重新输入。\n", wardId);
            continue;
        }
        break;
    }
    int newId = next_bed_id++;
    if (!is_unique_bed_id(newId)) {
        printf("  [错误] 床位 ID 冲突，请重试。\n");
        next_bed_id--;
        ui_pause(); return;
    }
    Ward* w = find_ward_by_id_sys(wardId);
    Bed* b = create_bed(newId, w, BED_FREE);
    if (!b) {
        printf("  [错误] 创建床位失败（内存不足）。\n");
        next_bed_id--;
        ui_pause(); return;
    }
    add_bed(&g_sys.beds, b);
    printf("  新床位已创建，床位号：%d\n", newId);
    ui_pause();
}

static void show_free_beds_cli(void) {
    ui_clear();
    ui_frame_begin("空闲床位");
    Bed* b = g_sys.beds;
    int cnt = 0;
    printf("  %-8s %-8s %-10s\n", "床位号", "病房ID", "房型");
    ui_divider();
    while (b) {
        if (b->status == BED_FREE) {
            printf("  %-8d %-8d ", b->id, b->ward ? b->ward->id : -1);
            if (b->ward) print_room_type_str((int)b->ward->type);
            printf("\n");
            cnt++;
        }
        b = b->next;
    }
    printf("\n  空闲床位共 %d 张。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void menu_admin_wards(void) {
    const char* const items[] = {
        "病房列表",
        "新增病房",
        "床位列表",
        "新增床位",
        "查看空闲床位",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [病房管理]", items, 6, 0);
        if (idx < 0 || idx == 5) return;
        ui_clear();
        switch (idx) {
        case 0: list_wards_cli();      break;
        case 1: add_ward_new_cli();    break;
        case 2: list_beds_cli();       break;
        case 3: add_bed_new_cli();     break;
        case 4: show_free_beds_cli();  break;
        }
    }
}

// ================== 药品管理中心 ==================

static void list_drugs_cli(void) {
    ui_clear();
    ui_frame_begin("药品列表");
    Drug* d = g_sys.drugs;
    int cnt = 0;
    if (!d) {
        printf("  暂无药品记录。\n");
    } else {
        printf("  %-8s %-20s %-10s\n", "药品编号", "名称", "单价");
        ui_divider();
        while (d) {
            printf("  %-8d %-20s %-10.2f\n", d->id, d->name, d->price);
            cnt++;
            d = d->next;
        }
    }
    printf("\n  共 %d 种药品。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void add_drug_new_cli(void) {
    ui_clear();
    ui_frame_begin("新增药品  [/b 上一步, /q 取消]");
    ui_divider();
    char name[64] = {0};
    double price = 0.0;
    int deptId = 0, fstep = 0, r;
    while (fstep < 3) {
        int back = 0;
        if (fstep == 0) {
            for (;;) {
                r = read_line_or_back("  药品名称：", name, sizeof(name));
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (name[0] == '\0') { printf("  [提示] 名称不能为空。\n"); continue; }
                break;
            }
        } else if (fstep == 1) {
            for (;;) {
                r = read_double_or_back("  单价（元）：", &price);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                if (!validate_money(price)) { printf("  [提示] 价格无效，请重新输入。\n"); continue; }
                break;
            }
        } else {
            for (;;) {
                r = read_int_or_back("  所属科室编号（-1 通用）：", &deptId);
                if (r == INPUT_QUIT) { printf("  已取消。\n"); return; }
                if (r == INPUT_BACK) { back = 1; break; }
                if (r == INPUT_EOF)  return;
                break;
            }
        }
        if (back) { if (fstep > 0) fstep--; else { printf("  已取消。\n"); return; } }
        else fstep++;
    }
    int newId = next_drug_id++;
    if (!is_unique_drug_id(newId)) {
        printf("  [错误] 药品 ID 冲突，请重试。\n");
        next_drug_id--;
        ui_pause(); return;
    }
    Drug* d = create_drug(newId, name, price, deptId, 0);
    if (!d) {
        printf("  [错误] 创建药品失败（内存不足）。\n");
        next_drug_id--;
        ui_pause(); return;
    }
    add_drug(&g_sys.drugs, d);
    printf("  新药品已创建，编号：%d\n", newId);
    ui_pause();
}

static void find_drug_cli(void) {
    ui_clear();
    ui_frame_begin("查找药品  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入药品编号：", &id) <= 0) return;
    Drug* d = find_drug_by_id(g_sys.drugs, id);
    if (!d) {
        printf("  未找到药品编号 %d。\n", id);
    } else {
        printf("  编号：%d  名称：%s  单价：%.2f\n", d->id, d->name, d->price);
    }
    ui_pause();
}

static void modify_drug_cli(void) {
    ui_clear();
    ui_frame_begin("修改药品信息  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入药品编号：", &id) <= 0) return;
    Drug* d = find_drug_by_id(g_sys.drugs, id);
    if (!d) {
        printf("  未找到药品编号 %d。\n", id);
        ui_pause(); return;
    }
    printf("  当前名称：%s  单价：%.2f\n", d->name, d->price);
    ui_divider();
    char name[64] = {0};
    double price = 0.0;
    int r;
    for (;;) {
        r = read_line_or_back("  新名称（直接回车保留）：", name, sizeof(name));
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        break;
    }
    for (;;) {
        r = read_double_or_back("  新单价（0 保留原值）：", &price);
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (price < 0) { printf("  [提示] 价格不能为负数。\n"); continue; }
        break;
    }
    if (name[0] != '\0') {
        strncpy(d->name, name, MAX_NAME_LEN);
        d->name[MAX_NAME_LEN-1] = '\0';
    }
    if (price > 0) d->price = price;
    printf("  药品信息已更新。\n");
    ui_pause();
}

static void delete_drug_cli(void) {
    ui_clear();
    ui_frame_begin("删除药品  [/q 取消]");
    ui_divider();
    int id;
    if (read_int_or_back("  请输入药品编号：", &id) <= 0) return;
    Drug** pp = &g_sys.drugs;
    while (*pp && (*pp)->id != id) pp = &(*pp)->next;
    if (!*pp) {
        printf("  未找到药品编号 %d。\n", id);
        ui_pause(); return;
    }
    Drug* del = *pp;
    *pp = del->next;
    free(del);
    printf("  药品编号 %d 已删除。\n", id);
    ui_pause();
}

static void add_drug_stock_cli(int isIn) {
    ui_clear();
    ui_frame_begin(isIn ? "药品入库  [/q 取消]" : "药品出库  [/q 取消]");
    ui_divider();
    int drugId, quantity;
    SimpleTime t;
    int r;
    for (;;) {
        r = read_int_or_back("  药品编号：", &drugId);
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (!find_drug_by_id(g_sys.drugs, drugId)) {
            printf("  [提示] 未找到药品 %d，请重新输入。\n", drugId);
            continue;
        }
        break;
    }
    for (;;) {
        r = read_int_or_back("  数量（正整数）：", &quantity);
        if (r == INPUT_QUIT || r == INPUT_EOF) { printf("  已取消。\n"); return; }
        if (quantity <= 0) { printf("  [提示] 数量必须大于 0。\n"); continue; }
        break;
    }
    r = prompt_and_read_time_or_back("  操作时间 (MM-DD HH:MM)：", &t);
    if (r <= 0) { printf("  已取消。\n"); return; }
    Drug* d2 = find_drug_by_id(g_sys.drugs, drugId);
    if (!isIn && d2 && d2->stock < quantity) {
        printf("  [错误] 药品 %s（ID %d）库存不足（当前库存 %d 盒），出库失败。\n",
            d2->name, d2->id, d2->stock);
        ui_pause(); return;
    }
    int newId = next_drug_stock_id++;
    int type = isIn ? 1 : 0;
    DrugStockRecord* rec = create_drug_stock_record(newId, drugId, quantity, type, t);
    if (!rec) {
        printf("  [错误] 创建库存记录失败（内存不足）。\n");
        next_drug_stock_id--;
        ui_pause(); return;
    }
    if (d2) {
        d2->stock += isIn ? quantity : -quantity;
    }
    add_drug_stock_record(&g_sys.drugStocks, rec);
    append_drug_stock_record_to_file(rec, "data/drug_stock_records.txt");
    printf("  药品%s记录已保存，记录ID：%d\n", isIn ? "入库" : "出库", newId);
    ui_pause();
}

static void list_drug_stocks_cli(void) {
    ui_clear();
    ui_frame_begin("药品出入库记录");
    DrugStockRecord* r = g_sys.drugStocks;
    int cnt = 0;
    if (!r) {
        printf("  暂无出入库记录。\n");
    } else {
        printf("  %-6s %-8s %-12s %-6s %-12s\n", "记录ID", "药品ID", "数量", "类型", "时间");
        ui_divider();
        while (r) {
            printf("  %-6d %-8d %-12d %-6s %02d-%02d %02d:%02d\n",
                r->id, r->drugId, r->quantity,
                r->type == 1 ? "入库" : "出库",
                r->time.month, r->time.day, r->time.hour, r->time.minute);
            cnt++;
            r = r->next;
        }
    }
    printf("\n  共 %d 条记录。\n", cnt);
    ui_footer("按任意键返回");
    ui_pause();
}

static void menu_admin_drugs(void) {
    const char* const items[] = {
        "药品列表",
        "新增药品",
        "按编号查找",
        "修改药品信息",
        "删除药品",
        "药品入库",
        "药品出库",
        "出入库记录",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [药品管理]", items, 9, 0);
        if (idx < 0 || idx == 8) return;
        ui_clear();
        switch (idx) {
        case 0: list_drugs_cli();        break;
        case 1: add_drug_new_cli();      break;
        case 2: find_drug_cli();         break;
        case 3: modify_drug_cli();       break;
        case 4: delete_drug_cli();       break;
        case 5: add_drug_stock_cli(1);   break;
        case 6: add_drug_stock_cli(0);   break;
        case 7: list_drug_stocks_cli();  break;
        }
    }
}

// ================== 统计与报告中心 ==================

static void menu_admin_stats_center(void) {
    const char* const items[] = {
        "时间段统计",
        "医生工作量统计",
        "营业额统计",
        "住院统计",
        "综合报表",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [统计与报告]", items, 6, 0);
        if (idx < 0 || idx == 5) return;
        ui_clear();
        switch (idx) {
        case 0: handle_time_range_stat();           break;
        case 1: stat_doctor_workload(); ui_pause();  break;
        case 2: stat_total_revenue();   ui_pause();  break;
        case 3: stat_inpatient_report(); ui_pause(); break;
        case 4: report_for_admin_view(); ui_pause(); break;
        }
    }
}

// ================== 保存与读取中心 ==================

static void menu_admin_storage(void) {
    const char* const items[] = {
        "保存所有数据",
        "读取所有数据",
        "返回"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [保存与读取]", items, 3, 0);
        if (idx < 0 || idx == 2) return;
        ui_clear();
        switch (idx) {
        case 0: save_all_to_files();   ui_pause(); break;
        case 1: load_all_from_files(); ui_pause(); break;
        }
    }
}

// ================== 退出程序确认 ==================

static void do_exit_program(void) {
    ui_clear();
    printf("\n");
    printf("  +----------------------------------------------------+\n");
    printf("  |                                                    |\n");
    printf("  |      欢 迎 使 用  医 院 医 疗 管 理 系 统         |\n");
    printf("  |    Hospital Medical Management System  v1.0        |\n");
    printf("  |                                                    |\n");
    printf("  +----------------------------------------------------+\n");
    printf("\n");
    printf("          [ Enter ]         确认退出程序\n");
    printf("          [ <- / Backspace ] 取消，返回菜单\n");
    printf("\n");
    for (;;) {
        int ch = _getch();
        if (ch == 13) { exit(0); }
        if (ch == 8)  return;
        if (ch == 0 || ch == 224) { int c2 = _getch(); if (c2 == 75) return; }
    }
}

// ================== 各角色的菜单循环 ==================

/* --- 患者菜单 --- */
static void menu_patient(void) {
    const char* const items[] = {
        "查询个人就诊历史记录",
        "时间段内个人费用统计",
        "患者角度报表",
        "退出登录"
    };

    for (;;) {
        int idx = menu_select("医院医疗管理系统  [患者端]", items, 4, 0);
        if (idx < 0) continue;
        ui_clear();
        switch (idx) {
        case 0: handle_query_patient_self();           break;
        case 1: handle_time_range_stat_patient();      break;
        case 2: report_for_patient_view();             break;
        case 3: return;
        }
        ui_pause();
    }
}

/* --- 医生菜单 --- */
static void menu_doctor(void) {
    const char* const items[] = {
        "新增诊疗记录",
        "修改本人诊治记录",
        "查询本科室记录",
        "按医生查询（本科室内）",
        "统计本科室医生工作量",
        "时间段统计",
        "医生角度报表",
        "保存数据",
        "加载数据",
        "退出登录"
    };

    for (;;) {
        int idx = menu_select("医院医疗管理系统  [医生/医护人员端]", items, 10, 0);
        if (idx < 0) continue;
        ui_clear();
        switch (idx) {
        case 0: add_record_manual();               break;
        case 1: modify_record();                   break;
        case 2: handle_query_dept_own();           break;
        case 3: handle_query_doctor_in_dept();     break;
        case 4: handle_doctor_workload_in_dept();  break;
        case 5: handle_time_range_stat();          break;
        case 6: report_for_doctor_view();          break;
        case 7: save_all_to_files();               break;
        case 8: load_all_from_files();             break;
        case 9: return;
        }
        ui_pause();
    }
}

/* --- 管理员菜单（全权限） --- */
static void menu_admin(void) {
    const char* const items[] = {
        "1. 记录管理",
        "2. 患者管理",
        "3. 医生管理",
        "4. 科室管理",
        "5. 病房管理",
        "6. 药品管理",
        "7. 统计与报告",
        "8. 保存与读取",
        "9. 注销",
        "10. 退出程序"
    };
    for (;;) {
        int idx = menu_select("医院医疗管理系统  [管理员]", items, 10, 0);
        if (idx < 0) continue;
        switch (idx) {
        case 0: {
            const char* const recItems[] = {
                "新增诊疗记录",
                "修改诊疗记录",
                "删除诊疗记录",
                "返回"
            };
            for (;;) {
                int ri = menu_select("医院医疗管理系统  [记录管理]", recItems, 4, 0);
                if (ri < 0 || ri == 3) break;
                ui_clear();
                switch (ri) {
                case 0: add_record_manual(); ui_pause(); break;
                case 1: modify_record();                break;
                case 2: delete_record_cli();            break;
                }
            }
            break;
        }
        case 1: menu_admin_patients();     break;
        case 2: menu_admin_doctors();      break;
        case 3: menu_admin_departments();  break;
        case 4: menu_admin_wards();        break;
        case 5: menu_admin_drugs();        break;
        case 6: menu_admin_stats_center(); break;
        case 7: menu_admin_storage();      break;
        case 8: return;  /* 注销：返回登录界面 */
        case 9: do_exit_program(); break;
        }
    }
}


// ================== 主菜单（登录循环） ==================

void system_main_menu(void) {
    show_welcome();  /* 显示欢迎页面，按 Enter 进入，Q 退出 */
    /* 登录循环：成功后进入对应角色菜单，用户选择退出时退出 */
    for (;;) {
        int r = do_login();
        if (r <= 0) return;   /* 0 = 用户退出; -1 = EOF */

        switch (g_session.role) {
        case ROLE_PATIENT: menu_patient(); break;
        case ROLE_DOCTOR:  menu_doctor();  break;
        case ROLE_ADMIN:   menu_admin();   break;
        default:           menu_admin();   break;
        }
        /* 角色菜单退出后，回到登录界面，可以重新登录 */
    }
}
