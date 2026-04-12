#ifndef DRUG_H
#define DRUG_H

#include "constants.h"
#include "time_util.h"

typedef struct Drug {
    int id;                         // 药品编号（唯一）
    char name[MAX_NAME_LEN];
    double price;                   // 单价
    int deptId;                     // 所属科室编号
    int stock;                      // 当前库存数量
    struct Drug* next;
} Drug;

typedef struct DrugStockRecord {
    int id;                         // 出入库编号
    int drugId;
    int quantity;                   // 变动数量（正数）
    int type;                       // 0=出库 1=入库
    SimpleTime time;
    struct DrugStockRecord* next;
} DrugStockRecord;

Drug* create_drug(int id, const char* name, double price, int deptId, int stock);
void add_drug(Drug** head, Drug* d);
Drug* find_drug_by_id(Drug* head, int id);
void free_drugs(Drug* head);

DrugStockRecord* create_drug_stock_record(int id, int drugId, int quantity, int type, SimpleTime time);
void add_drug_stock_record(DrugStockRecord** head, DrugStockRecord* rec);
void free_drug_stock_records(DrugStockRecord* head);

#endif
