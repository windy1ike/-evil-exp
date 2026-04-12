//病房和床位管理的头文件

#ifndef WARD_H
#define WARD_H

#include "constants.h"

// 定义病房结构体
typedef struct Ward {
    int id;                     // 病房编号
    RoomType type;              // 病房类型（普通/标准/VIP）
    int deptId;                 // 对应科室编号
    struct Ward* next;          // 指向下一个病房的指针
} Ward;

// 定义床位结构体
typedef struct Bed {
    int id;                     // 床位编号（唯一）
    Ward* ward;                 // 所属病房的指针
    BedStatus status;           // 床位状态（空闲/已入住/维护）
    struct Bed* next;           // 指向下一个床位的指针
} Bed;

// 创建病房的函数声明
Ward* create_ward(int id, RoomType type, int deptId);
// 添加病房到链表的函数声明
void add_ward(Ward** head, Ward* w);

// 创建床位的函数声明
Bed* create_bed(int id, Ward* ward, BedStatus status);
// 添加床位到链表的函数声明
void add_bed(Bed** head, Bed* b);

// 自动分配空闲床位的函数声明
Bed* allocate_free_bed(Bed* beds, int deptId); 
// 出院释放床位的函数声明
void release_bed(Bed* bed);                    

// 释放病房链表的内存的函数声明
void free_wards(Ward* head);
// 释放床位链表的内存的函数声明
void free_beds(Bed* head);

#endif