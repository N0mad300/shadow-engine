#ifndef PROCESS_H
#define PROCESS_H

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

#define MAX_PROCESSES 1024
#define MAX_NAME_LEN 256

typedef struct
{
    char name[MAX_NAME_LEN];
    int pid;
} ProcessInfo;

extern ProcessInfo processes[MAX_PROCESSES];

extern int process_count;
extern int selected_process;

void getRunningProcesses();

#endif