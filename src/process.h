#ifndef PROCESS_H
#define PROCESS_H

#include <windows.h>
#include <psapi.h>
#include <string.h>

#include "dynamic_array.h"

#define MAX_RESULTS 1024
#define MAX_PROCESSES 1024
#define MAX_NAME_LEN 256

typedef struct
{
    char name[MAX_NAME_LEN];
    HANDLE handle;
} ProcessInfo;

extern ProcessInfo processes[MAX_PROCESSES];

extern int process_count;
extern int selected_process;

void get_running_processes();
void cleanup_process_handles();

#endif