#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>
#include "app.h"

int main() {
  
    SetConsoleOutputCP(936);
    SetConsoleCP(936);
    
    init_system();

    // Pure console application
    system_main_menu();

    destroy_system();
    return 0;
}