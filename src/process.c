#include "process.h"

ProcessInfo processes[MAX_PROCESSES];
int process_count = 0;
int selected_process = -1;

void get_running_processes()
{
    cleanup_process_handles();

    DWORD process_ids[MAX_PROCESSES], bytes_returned;
    process_count = 0;

    if (!EnumProcesses(process_ids, sizeof(process_ids), &bytes_returned))
    {
        fprintf(stderr, "Failed to enumerate processes.\n");
        return;
    }

    int count = bytes_returned / sizeof(DWORD);
    for (int i = 0; i < count && process_count < MAX_PROCESSES; ++i)
    {
        DWORD pid = process_ids[i];
        HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);

        if (hProcess)
        {
            char name[MAX_NAME_LEN] = "<unknown>";
            HMODULE hModule;
            DWORD bytes_needed;

            // Get process name
            if (EnumProcessModules(hProcess, &hModule, sizeof(hModule), &bytes_needed))
            {
                GetModuleBaseName(hProcess, hModule, name, sizeof(name) / sizeof(char));
            }

            strncpy_s(processes[process_count].name, sizeof(processes[process_count].name), name, MAX_NAME_LEN);
            processes[process_count].name[MAX_NAME_LEN - 1] = '\0';
            processes[process_count].handle = hProcess;
            process_count++;
        }
        else
        {
            CloseHandle(hProcess);
        }
    }
}

void cleanup_process_handles()
{
    for (int i = 0; i < process_count; i++)
    {
        if (processes[i].handle)
        {
            CloseHandle(processes[i].handle);
            processes[i].handle = NULL;
        }
    }
}