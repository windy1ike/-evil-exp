#include <stdio.h>
#include <string.h>
#include "validate.h"
#include "constants.h"
#include "time_util.h"
#include "entities.h"
#include "drug.h"
#include "ward.h"
#include "record.h"
#include "system.h"

// ========== 基础合法性 ==========

int validate_age(int age) {
    if (age <= 0 || age > 120) {
        printf("  [提示] 年龄须在 1~120 岁之间，请重新输入。\n");
        return 0;
    }
    return 1;
}

int validate_money(double m) {
    if (m < 0.0) {
        printf("  [提示] 金额不能为负数，请重新输入。\n");
        return 0;
    }
    if (m > MAX_MONEY) {
        printf("  [提示] 金额超出上限（最大 %.2f 元），请重新输入。\n", MAX_MONEY);
        return 0;
    }
    long long cents = (long long)(m * 100 + 0.5);
    double back = cents / 100.0;
    if (back - m > 1e-6 || m - back > 1e-6) {
        printf("  [提示] 金额须精确到分（最多两位小数）。\n");
        return 0;
    }
    return 1;
}

int validate_string(const char* s) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0) {
        printf("  [提示] 名称/描述不能为空，请重新输入。\n");
        return 0;
    }
    if (len >= MAX_STR_LEN) {
        printf("  [提示] 输入内容过长（最多 %d 个字符），请缩短后重试。\n", MAX_STR_LEN - 1);
        return 0;
    }
    return 1;
}

int validate_quantity(int q) {
    if (q <= 0) {
        printf("  [提示] 数量必须为正整数，请重新输入。\n");
        return 0;
    }
    return 1;
}

int validate_deposit(double deposit,
    const SimpleTime* start,
    const SimpleTime* expectedEnd) {
    if (!validate_money(deposit)) return 0;

    if (deposit <= 0) {
        printf("  [提示] 住院押金必须大于零，请重新输入。\n");
        return 0;
    }

    long long cents = (long long)(deposit * 100 + 0.5);
    if (cents % (100 * 100) != 0) {
        printf("  [提示] 住院押金须为 100 元的整数倍，请重新输入。\n");
        return 0;
    }

    int days = days_between(start, expectedEnd);
    if (days <= 0) {
        printf("  [提示] 预计出院日期须晚于入院日期，请重新输入。\n");
        return 0;
    }

    double minDeposit = days * DAILY_HOSPITAL_FEE;
    if (deposit + 1e-6 < minDeposit) {
        printf("  [提示] 押金不足，按住院天数计算最少须缴纳 %.2f 元。\n", minDeposit);
        return 0;
    }

    return 1;
}

// ========== 唯一性检查 ==========

int is_unique_patient_id(int id) {
    Patient* p = g_sys.patients;
    while (p) {
        if (p->id == id) {
            printf("患者编号 %d 已存在。\n", id);
            return 0;
        }
        p = p->next;
    }
    return 1;
}

int is_unique_doctor_id(int id) {
    Doctor* d = g_sys.doctors;
    while (d) {
        if (d->id == id) {
            printf("医生工号 %d 已存在。\n", id);
            return 0;
        }
        d = d->next;
    }
    return 1;
}

int is_unique_drug_id(int id) {
    Drug* d = g_sys.drugs;
    while (d) {
        if (d->id == id) {
            printf("药品编号 %d 已存在。\n", id);
            return 0;
        }
        d = d->next;
    }
    return 1;
}

int is_unique_bed_id(int id) {
    Bed* b = g_sys.beds;
    while (b) {
        if (b->id == id) {
            printf("床位编号 %d 已存在。\n", id);
            return 0;
        }
        b = b->next;
    }
    return 1;
}

int is_unique_reg_id(long long regId) {
    MedicalRecord* r = g_sys.records;
    while (r) {
        if (r->regInfo.regId == regId) {
            printf("挂号单号 %lld 已存在。\n", regId);
            return 0;
        }
        r = r->next;
    }
    return 1;
}

// ========== 挂号规则 ==========

static int is_same_day_local(const SimpleTime* a, const SimpleTime* b) {
    return a->month == b->month && a->day == b->day;
}

int check_registration_rules(Patient* p,
    Doctor* d,
    Department* dept,
    const SimpleTime* regTime) {
    if (!p || !d || !dept || !regTime) return 0;

    int totalToday = 0;
    int doctorToday = 0;
    int patientToday = 0;
    int patientDeptToday = 0;

    MedicalRecord* r = g_sys.records;
    while (r) {
        if (!r->isCancelled && is_same_day_local(&r->regInfo.regTime, regTime)) {
            totalToday++;

            if (r->doctorInfo && r->doctorInfo->id == d->id) {
                doctorToday++;
            }
            if (r->patient && r->patient->id == p->id) {
                patientToday++;
                if (r->regInfo.dept && r->regInfo.dept->id == dept->id) {
                    patientDeptToday++;
                }
            }
        }
        r = r->next;
    }

    if (totalToday >= MAX_REG_PER_DAY) {
        printf("  [提示] 今日全院挂号已达上限（%d 个），暂停挂号。\n", MAX_REG_PER_DAY);
        return 0;
    }
    if (doctorToday >= 20) {
        printf("  [提示] 该医生今日号源已满（上限 20 个），请换诊或择日就诊。\n");
        return 0;
    }
    if (patientToday >= 5) {
        printf("  [提示] 您今日挂号次数已达上限（5 次），请择日就诊。\n");
        return 0;
    }
    if (patientDeptToday >= 1) {
        printf("  [提示] 您今日已在该科室挂号，不可重复，如需复诊请联系医护人员。\n");
        return 0;
    }
    return 1;
}

// ========== 药品数量约束 ==========

int validate_drug_count_per_visit(int kindCount) {
    if (kindCount < 0) {
        printf("  [提示] 药品数量字段不能为负数。\n");
        return 0;
    }
    if (kindCount > MAX_DRUG_PER_VISIT) {
        printf("  [提示] 单次就诊处方药品不得超过 %d 种。\n", MAX_DRUG_PER_VISIT);
        return 0;
    }
    return 1;
}

int validate_box_per_drug(int boxCount) {
    if (boxCount <= 0) {
        printf("  [提示] 单种药品数量须为正整数。\n");
        return 0;
    }
    if (boxCount > MAX_BOX_PER_DRUG) {
        printf("  [提示] 单种药品数量不得超过 %d 盒/件。\n", MAX_BOX_PER_DRUG);
        return 0;
    }
    return 1;
}