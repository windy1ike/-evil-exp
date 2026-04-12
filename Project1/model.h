// 模型层聚合头文件：包含所有实体/数据结构定义
// 使用方只需 #include "model.h" 即可获得所有数据类型

#ifndef MODEL_H
#define MODEL_H

#include "constants.h"   // 常量、枚举（Gender, DoctorTitle, BedStatus, RoomType, UserRole 等）
#include "time_util.h"   // SimpleTime 及时间工具函数
#include "entities.h"    // Department, Patient, Doctor
#include "drug.h"        // Drug, DrugStockRecord
#include "ward.h"        // Ward, Bed
#include "record.h"      // MedicalRecord 及所有子结构

#endif /* MODEL_H */
