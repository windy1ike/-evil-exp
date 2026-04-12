// 常量定义头文件
#ifndef CONSTANTS_H
#define CONSTANTS_H

// 字符串最大长度
#define MAX_NAME_LEN 30
#define MAX_STR_LEN  30

// 容量上限
#define MAX_OUTPATIENTS      200  // 门诊病人最大数量
#define MAX_INPATIENTS       60   // 住院病人最大数量
#define MAX_RECORDS_PER_PAT  30   // 每位病人最大记录数
#define MAX_DOCTORS          40   // 医生最大数量
#define MAX_DRUG_TYPES       20   // 药品种类最大数量
#define MAX_REG_PER_DAY      500  // 每天最大注册数量
#define MAX_DRUG_PER_VISIT   10   // 每次就诊最大开药数量
#define MAX_BOX_PER_DRUG     100  // 每种药品最大盒数
#define CURRENT_YEAR         2026 // 当前年份

// 医院费用相关
#define DAILY_HOSPITAL_FEE   200.0 // 每日住院费用
#define MAX_MONEY            100000.0 // 最大金额限制

// 性别枚举
typedef enum {
    GENDER_MALE = 0,  // 男性
    GENDER_FEMALE     // 女性
} Gender;

// 医生职称枚举
typedef enum {
    DOC_TITLE_CHIEF,         // 主任医师
    DOC_TITLE_DEPUTY_CHIEF,  // 副主任医师
    DOC_TITLE_ATTENDING,         // 主治医师
    DOC_TITLE_RESIDENT           // 住院医师
} DoctorTitle;

// 处置类型枚举
typedef enum {
    DISP_EXAM = 0,    // 检查
    DISP_DRUG,        // 开药
    DISP_HOSPITAL     // 住院
} DisposalType;

// 床位状态枚举
typedef enum {
    BED_FREE = 0,     // 床位空闲
    BED_OCCUPIED,     // 床位已占用
    BED_MAINTENANCE   // 床位维护中
} BedStatus;

// 房间类型枚举
typedef enum {
    ROOM_COMMON = 0,  // 普通房
    ROOM_STANDARD,    // 标准房
    ROOM_VIP          // VIP房
} RoomType;

// 用户角色枚举
typedef enum {
    ROLE_PATIENT = 0, // 病人
    ROLE_DOCTOR,      // 医生
    ROLE_ADMIN        // 管理员
} UserRole;

#endif