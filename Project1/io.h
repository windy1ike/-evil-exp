#ifndef IO_H
#define IO_H

#include "system.h"

// 基础数据读写
void load_basic_data();     // 加载科室/医生/药品/床位等
void save_basic_data();

// 诊疗记录读写
void import_records_from_file(const char* filename);
void save_all_records_to_file(const char* filename);

// 保存/载入系统全部信息
void save_all_to_files();
void load_all_from_files();

// 药品出入库记录追加写入
void append_drug_stock_record_to_file(const DrugStockRecord* rec, const char* filename);

#endif
