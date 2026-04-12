#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "system.h"
#include "time_util.h"
#include "constants.h"
#include "entities.h"
#include "drug.h"
#include "ward.h"
#include "record.h"

// 与 system.c 中的全局 ID 计数器对应
extern int next_patient_id;
extern int next_doctor_id;
extern int next_dept_id;
extern int next_record_id;
extern int next_drug_id;
extern int next_bed_id;
extern int next_drug_stock_id;

#define FILE_PATIENTS    "data/patients.txt"
#define FILE_DOCTORS     "data/doctors.txt"
#define FILE_DEPARTMENTS "data/departments.txt"
#define FILE_DRUGS       "data/drugs.txt"
#define FILE_WARDS       "data/wards.txt"
#define FILE_BEDS        "data/beds.txt"
#define FILE_RECORDS     "data/records.txt"
#define FILE_DRUG_STOCKS "data/drug_stock_records.txt"

/* ========== 保存基础数据 ========== */

static void save_patients(void) {
    FILE* fp = fopen(FILE_PATIENTS, "w");
    if (!fp) { perror("patients.txt"); return; }
    Patient* p = g_sys.patients;
    while (p) {
        fprintf(fp, "%d %s %d %d\n",
            p->id, p->name, p->age, (int)p->gender);
        p = p->next;
    }
    fclose(fp);
}

static void save_departments(void) {
    FILE* fp = fopen(FILE_DEPARTMENTS, "w");
    if (!fp) { perror("departments.txt"); return; }
    Department* d = g_sys.departments;
    while (d) {
        fprintf(fp, "%d %s\n", d->id, d->name);
        d = d->next;
    }
    fclose(fp);
}

static void save_doctors(void) {
    FILE* fp = fopen(FILE_DOCTORS, "w");
    if (!fp) { perror("doctors.txt"); return; }
    Doctor* d = g_sys.doctors;
    while (d) {
        fprintf(fp, "%d %s %d %d\n",
            d->id, d->name, (int)d->title,
            d->dept ? d->dept->id : -1);
        d = d->next;
    }
    fclose(fp);
}

static void save_drugs(void) {
    FILE* fp = fopen(FILE_DRUGS, "w");
    if (!fp) { perror("drugs.txt"); return; }
    Drug* d = g_sys.drugs;
    while (d) {
        fprintf(fp, "%d %s %.2f %d %d\n",
            d->id, d->name, d->price, d->deptId, d->stock);
        d = d->next;
    }
    fclose(fp);
}

static void save_wards(void) {
    FILE* fp = fopen(FILE_WARDS, "w");
    if (!fp) { perror("wards.txt"); return; }
    Ward* w = g_sys.wards;
    while (w) {
        fprintf(fp, "%d %d %d\n", w->id, (int)w->type, w->deptId);
        w = w->next;
    }
    fclose(fp);
}

static void save_beds(void) {
    FILE* fp = fopen(FILE_BEDS, "w");
    if (!fp) { perror("beds.txt"); return; }
    Bed* b = g_sys.beds;
    while (b) {
        fprintf(fp, "%d %d %d\n",
            b->id, b->ward ? b->ward->id : -1, (int)b->status);
        b = b->next;
    }
    fclose(fp);
}

/* ========== 保存记录 ========== */

static void save_records(void) {
    FILE* fp = fopen(FILE_RECORDS, "w");
    if (!fp) { perror("records.txt"); return; }

    MedicalRecord* r = g_sys.records;
    while (r) {
        fprintf(fp, "RECORD_BEGIN\n");
        fprintf(fp, "ID %d\n", r->id);
        fprintf(fp, "PATIENT %d\n", r->patient ? r->patient->id : -1);
        fprintf(fp, "DOCTOR %d\n", r->doctorInfo ? r->doctorInfo->id : -1);
        fprintf(fp, "DEPT %d\n", r->regInfo.dept ? r->regInfo.dept->id : -1);
        fprintf(fp, "REGTIME %02d-%02d %02d:%02d\n",
            r->regInfo.regTime.month, r->regInfo.regTime.day,
            r->regInfo.regTime.hour, r->regInfo.regTime.minute);
        fprintf(fp, "REGSEQ %d\n", r->regInfo.seqNoOfDay);
        fprintf(fp, "REGID %lld\n", r->regInfo.regId);
        fprintf(fp, "SYMPTOM %s\n", r->diagInfo.symptom);
        fprintf(fp, "RESULT %s\n", r->diagInfo.result);
        fprintf(fp, "SUGGEST %s\n", r->diagInfo.suggestion);

        // 检查
        fprintf(fp, "EXAM_FLAG %d\n", r->dispInfo.hasExam);
        if (r->dispInfo.hasExam) {
            int cnt = 0;
            for (ExamItem* e = r->dispInfo.exam.items; e; e = e->next) cnt++;
            fprintf(fp, "EXAM_COUNT %d\n", cnt);
            for (ExamItem* e = r->dispInfo.exam.items; e; e = e->next) {
                fprintf(fp, "EXAM %s %.2f\n", e->name, e->fee);
            }
        }

        // 开药
        fprintf(fp, "DRUG_FLAG %d\n", r->dispInfo.hasDrug);
        if (r->dispInfo.hasDrug) {
            int cnt = 0;
            for (DrugItem* di = r->dispInfo.prescription.items; di; di = di->next) cnt++;
            fprintf(fp, "DRUG_COUNT %d\n", cnt);
            for (DrugItem* di = r->dispInfo.prescription.items; di; di = di->next) {
                fprintf(fp, "DRUG %d %d %.2f\n",
                    di->drugId, di->quantity, di->unitPrice);
            }
        }

        // 住院
        fprintf(fp, "HOSPITAL_FLAG %d\n", r->dispInfo.hasHospital);
        if (r->dispInfo.hasHospital) {
            HospitalizationInfo* h = &r->dispInfo.hospitalization;
            fprintf(fp, "HOSP_START %02d-%02d %02d:%02d\n",
                h->startTime.month, h->startTime.day,
                h->startTime.hour, h->startTime.minute);
            fprintf(fp, "HOSP_EXPECT %02d-%02d %02d:%02d\n",
                h->expectedLeaveTime.month, h->expectedLeaveTime.day,
                h->expectedLeaveTime.hour, h->expectedLeaveTime.minute);
            fprintf(fp, "HOSP_DEPOSIT %.2f\n", h->deposit);
            fprintf(fp, "HOSP_BED %d\n", h->bed ? h->bed->id : -1);
            fprintf(fp, "HOSP_IN %d\n", h->inHospital);
            fprintf(fp, "HOSP_TOTAL %.2f\n", h->totalFee);
            if (h->actualLeaveTime.month != 0) {
                fprintf(fp, "HOSP_ACTUAL %02d-%02d %02d:%02d\n",
                    h->actualLeaveTime.month, h->actualLeaveTime.day,
                    h->actualLeaveTime.hour, h->actualLeaveTime.minute);
            }
            else {
                fprintf(fp, "HOSP_ACTUAL NONE\n");
            }
        }

        fprintf(fp, "CANCELLED %d\n", r->isCancelled);
        fprintf(fp, "RECORD_END\n");
        r = r->next;
    }

    fclose(fp);
}

/* ========== 加载基础数据 ========== */

static void load_patients(void) {
    FILE* fp = fopen(FILE_PATIENTS, "r");
    if (!fp) return;

    int id, age, gender;
    char name[64];
    while (fscanf(fp, "%d %63s %d %d", &id, name, &age, &gender) == 4) {
        Patient* p = create_patient(id, name, age,
            gender == 0 ? GENDER_MALE : GENDER_FEMALE);
        if (!p) break;
        add_patient(&g_sys.patients, p);
        if (id >= next_patient_id) next_patient_id = id + 1;
    }

    fclose(fp);
}

static void load_departments(void) {
    FILE* fp = fopen(FILE_DEPARTMENTS, "r");
    if (!fp) return;

    int id;
    char name[64];
    while (fscanf(fp, "%d %63s", &id, name) == 2) {
        Department* d = create_department(id, name);
        if (!d) break;
        add_department(&g_sys.departments, d);
        if (id >= next_dept_id) next_dept_id = id + 1;
    }

    fclose(fp);
}

static void load_doctors(void) {
    FILE* fp = fopen(FILE_DOCTORS, "r");
    if (!fp) return;

    int id, title, deptId;
    char name[64];
    while (fscanf(fp, "%d %63s %d %d", &id, name, &title, &deptId) == 4) {
        Department* dept = find_department_by_id(g_sys.departments, deptId);
        Doctor* d = create_doctor(id, name, (DoctorTitle)title, dept);
        if (!d) break;
        add_doctor(&g_sys.doctors, d);
        if (id >= next_doctor_id) next_doctor_id = id + 1;
    }

    fclose(fp);
}

static void load_drugs(void) {
    FILE* fp = fopen(FILE_DRUGS, "r");
    if (!fp) return;

    int id, deptId, stock;
    char name[64];
    double price;
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        stock = 0;
        int n = sscanf(line, "%d %63s %lf %d %d", &id, name, &price, &deptId, &stock);
        if (n < 4) continue;
        Drug* d = create_drug(id, name, price, deptId, stock);
        if (!d) break;
        add_drug(&g_sys.drugs, d);
        if (id >= next_drug_id) next_drug_id = id + 1;
    }

    fclose(fp);
}

static void load_wards(void) {
    FILE* fp = fopen(FILE_WARDS, "r");
    if (!fp) return;

    int id, type, deptId;
    while (fscanf(fp, "%d %d %d", &id, &type, &deptId) == 3) {
        Ward* w = create_ward(id, (RoomType)type, deptId);
        if (!w) break;
        add_ward(&g_sys.wards, w);
    }

    fclose(fp);
}

static Ward* find_ward_by_id_local(int id) {
    Ward* w = g_sys.wards;
    while (w) {
        if (w->id == id) return w;
        w = w->next;
    }
    return NULL;
}

static void load_beds(void) {
    FILE* fp = fopen(FILE_BEDS, "r");
    if (!fp) return;

    int id, wardId, status;
    while (fscanf(fp, "%d %d %d", &id, &wardId, &status) == 3) {
        Ward* w = find_ward_by_id_local(wardId);
        Bed* b = create_bed(id, w, (BedStatus)status);
        if (!b) break;
        add_bed(&g_sys.beds, b);
        if (id >= next_bed_id) next_bed_id = id + 1;
    }

    fclose(fp);
}

/* ========== 加载记录 ========== */

static Bed* find_bed_by_id_local(int id) {
    Bed* b = g_sys.beds;
    while (b) {
        if (b->id == id) return b;
        b = b->next;
    }
    return NULL;
}

void load_records(void) {
    FILE* fp = fopen(FILE_RECORDS, "r");
    if (!fp) return;

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RECORD_BEGIN", 12) != 0) continue;

        MedicalRecord* r = create_record();
        if (!r) break;

        ExamItem* examHead = NULL;
        DrugItem* drugHead = NULL;

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "RECORD_END", 10) == 0) {
                break;
            }

            if (sscanf(line, "ID %d", &r->id) == 1) {
                if (r->id >= next_record_id) next_record_id = r->id + 1;
            }
            else if (strncmp(line, "PATIENT", 7) == 0) {
                int pid;
                if (sscanf(line, "PATIENT %d", &pid) == 1) {
                    r->patient = find_patient_by_id(g_sys.patients, pid);
                    if (r->patient) r->patient->recordCount++;
                }
            }
            else if (strncmp(line, "DOCTOR", 6) == 0) {
                int did;
                if (sscanf(line, "DOCTOR %d", &did) == 1) {
                    r->doctorInfo = find_doctor_by_id(g_sys.doctors, did);
                }
            }
            else if (strncmp(line, "DEPT", 4) == 0) {
                int deptId;
                if (sscanf(line, "DEPT %d", &deptId) == 1) {
                    r->regInfo.dept = find_department_by_id(g_sys.departments, deptId);
                }
            }
            else if (strncmp(line, "REGTIME", 7) == 0) {
                char tbuf[64];
                if (sscanf(line, "REGTIME %63[^\n]", tbuf) == 1) {
                    parse_time(tbuf, &r->regInfo.regTime);
                }
            }
            else if (sscanf(line, "REGSEQ %d", &r->regInfo.seqNoOfDay) == 1) {
                // ok
            }
            else if (sscanf(line, "REGID %lld", &r->regInfo.regId) == 1) {
                // ok
            }
            else if (strncmp(line, "SYMPTOM", 7) == 0) {
                char buf[128];
                if (sscanf(line, "SYMPTOM %127[^\n]", buf) == 1) {
                    strncpy(r->diagInfo.symptom, buf, MAX_STR_LEN);
                    r->diagInfo.symptom[MAX_STR_LEN - 1] = '\0';
                }
            }
            else if (strncmp(line, "RESULT", 6) == 0) {
                char buf[128];
                if (sscanf(line, "RESULT %127[^\n]", buf) == 1) {
                    strncpy(r->diagInfo.result, buf, MAX_STR_LEN);
                    r->diagInfo.result[MAX_STR_LEN - 1] = '\0';
                }
            }
            else if (strncmp(line, "SUGGEST", 7) == 0) {
                char buf[128];
                if (sscanf(line, "SUGGEST %127[^\n]", buf) == 1) {
                    strncpy(r->diagInfo.suggestion, buf, MAX_STR_LEN);
                    r->diagInfo.suggestion[MAX_STR_LEN - 1] = '\0';
                }
            }
            else if (sscanf(line, "EXAM_FLAG %d", &r->dispInfo.hasExam) == 1) {
                // ok
            }
            else if (strncmp(line, "EXAM ", 5) == 0) {
                char name[64];
                double fee;
                if (sscanf(line, "EXAM %63s %lf", name, &fee) == 2) {
                    ExamItem* e = (ExamItem*)malloc(sizeof(ExamItem));
                    if (e) {
                        strncpy(e->name, name, MAX_NAME_LEN);
                        e->name[MAX_NAME_LEN - 1] = '\0';
                        e->fee = fee;
                        e->next = examHead;
                        examHead = e;
                    }
                }
            }
            else if (sscanf(line, "DRUG_FLAG %d", &r->dispInfo.hasDrug) == 1) {
                // ok
            }
            else if (strncmp(line, "DRUG ", 5) == 0) {
                int did, qty;
                double price;
                if (sscanf(line, "DRUG %d %d %lf", &did, &qty, &price) == 3) {
                    DrugItem* di = (DrugItem*)malloc(sizeof(DrugItem));
                    if (di) {
                        di->drugId = did;
                        di->quantity = qty;
                        di->unitPrice = price;
                        di->next = drugHead;
                        drugHead = di;
                    }
                }
            }
            else if (sscanf(line, "HOSPITAL_FLAG %d", &r->dispInfo.hasHospital) == 1) {
                // ok
            }
            else if (strncmp(line, "HOSP_START", 10) == 0) {
                char tbuf[64];
                if (sscanf(line, "HOSP_START %63[^\n]", tbuf) == 1) {
                    parse_time(tbuf, &r->dispInfo.hospitalization.startTime);
                }
            }
            else if (strncmp(line, "HOSP_EXPECT", 11) == 0) {
                char tbuf[64];
                if (sscanf(line, "HOSP_EXPECT %63[^\n]", tbuf) == 1) {
                    parse_time(tbuf, &r->dispInfo.hospitalization.expectedLeaveTime);
                }
            }
            else if (sscanf(line, "HOSP_DEPOSIT %lf", &r->dispInfo.hospitalization.deposit) == 1) {
                // ok
            }
            else if (strncmp(line, "HOSP_BED", 8) == 0) {
                int bedId;
                if (sscanf(line, "HOSP_BED %d", &bedId) == 1) {
                    r->dispInfo.hospitalization.bed = find_bed_by_id_local(bedId);
                }
            }
            else if (sscanf(line, "HOSP_IN %d", &r->dispInfo.hospitalization.inHospital) == 1) {
                // ok
            }
            else if (sscanf(line, "HOSP_TOTAL %lf", &r->dispInfo.hospitalization.totalFee) == 1) {
                // ok
            }
            else if (strncmp(line, "HOSP_ACTUAL", 11) == 0) {
                char tbuf[64];
                if (strstr(line, "NONE")) {
                    r->dispInfo.hospitalization.actualLeaveTime.month = 0;
                }
                else if (sscanf(line, "HOSP_ACTUAL %63[^\n]", tbuf) == 1) {
                    parse_time(tbuf, &r->dispInfo.hospitalization.actualLeaveTime);
                }
            }
            else if (sscanf(line, "CANCELLED %d", &r->isCancelled) == 1) {
                // ok
            }
        }

        if (r->dispInfo.hasExam) {
            double total = 0.0;
            ExamItem* e = examHead;
            while (e) { total += e->fee; e = e->next; }
            r->dispInfo.exam.items = examHead;
            r->dispInfo.exam.totalFee = total;
        }
        if (r->dispInfo.hasDrug) {
            double total = 0.0;
            DrugItem* di = drugHead;
            while (di) { total += di->unitPrice * di->quantity; di = di->next; }
            r->dispInfo.prescription.items = drugHead;
            r->dispInfo.prescription.totalFee = total;
        }

        add_record(&g_sys.records, r);
    }

    fclose(fp);
}


/* ========== 药品出入库记录保存/加载/追加 ========== */

static void save_drug_stock_records(void) {
    FILE* fp = fopen(FILE_DRUG_STOCKS, "w");
    if (!fp) { perror("drug_stock_records.txt"); return; }
    DrugStockRecord* r = g_sys.drugStocks;
    while (r) {
        fprintf(fp, "%d %d %d %d %02d-%02d %02d:%02d\n",
            r->id, r->drugId, r->quantity, r->type,
            r->time.month, r->time.day,
            r->time.hour, r->time.minute);
        r = r->next;
    }
    fclose(fp);
}

static void load_drug_stock_records(void) {
    FILE* fp = fopen(FILE_DRUG_STOCKS, "r");
    if (!fp) return;

    int id, drugId, quantity, type;
    char datebuf[16], timebuf[16];
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d %d %d %d %15s %15s",
                   &id, &drugId, &quantity, &type, datebuf, timebuf) == 6) {
            char tbuf[32];
            snprintf(tbuf, sizeof(tbuf), "%s %s", datebuf, timebuf);
            SimpleTime t;
            parse_time(tbuf, &t);
            DrugStockRecord* rec = create_drug_stock_record(id, drugId, quantity, type, t);
            if (!rec) break;
            add_drug_stock_record(&g_sys.drugStocks, rec);
            if (id >= next_drug_stock_id) next_drug_stock_id = id + 1;
        }
    }
    fclose(fp);
}

void append_drug_stock_record_to_file(const DrugStockRecord* rec, const char* filename) {
    if (!rec || !filename) return;
    FILE* fp = fopen(filename, "a");
    if (!fp) {
        printf("[警告] 出入库记录未能写入文件，请检查磁盘。\n");
        return;
    }
    fprintf(fp, "%d %d %d %d %02d-%02d %02d:%02d\n",
        rec->id, rec->drugId, rec->quantity, rec->type,
        rec->time.month, rec->time.day,
        rec->time.hour, rec->time.minute);
    fclose(fp);
}

/* ========== 对外接口 ========== */

void save_all_to_files(void) {
    save_patients();
    save_departments();
    save_doctors();
    save_drugs();
    save_wards();
    save_beds();
    save_records();
    save_drug_stock_records();
    printf("所有数据已保存到 data/ 目录。\n");
}

void load_all_from_files(void) {
    load_patients();
    load_departments();
    load_doctors();
    load_drugs();
    load_wards();
    load_beds();
    load_records();
    free_drug_stock_records(g_sys.drugStocks);
    g_sys.drugStocks = NULL;
    load_drug_stock_records();
    printf("所有数据已从 data/ 目录加载（若存在）。\n");
}