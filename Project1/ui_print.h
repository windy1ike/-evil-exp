// UI 打印辅助函数头文件
#ifndef UI_PRINT_H
#define UI_PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

/* 清屏 */
void ui_clear(void);

/* 打印带标题的页头（上下分隔线） */
void ui_header(const char* title);

/* 打印分隔线 */
void ui_divider(void);

/* 打印带提示文字的页脚 */
void ui_footer(const char* hint);

/* 按任意键继续 */
void ui_pause(void);

/* 带边框的页面框架（与欢迎界面同款宽度）
 * ui_frame_begin : 顶部 +===+ 边框 + 标题行 + 分隔线
 * ui_frame_end   : 底部分隔线 + 提示行
 */
void ui_frame_begin(const char* title);
void ui_frame_end(const char* hint);

#ifdef __cplusplus
}
#endif

#endif /* UI_PRINT_H */
