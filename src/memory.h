#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

#define CHUNK_SIZE (1024 * 1024)

typedef struct
{
    void *address;
    char *value;
    char *previous_value;
} ResultEntry;

typedef struct
{
    void *address;
    char *value;
    bool freeze;
} SelectionEntry;

typedef struct
{
    ResultEntry *results;
    size_t result_count;
    size_t result_capacity;
} ResultsTable;

typedef struct
{
    SelectionEntry *selection;
    size_t selection_count;
    size_t selection_capacity;
} SelectionTable;

typedef enum
{
    SCAN_EXACT_VALUE,
    SCAN_BIGGER_THAN,
    SCAN_SMALLER_THAN,
    SCAN_VALUE_BETWEEN,
    SCAN_UNKNOWN_INITIAL
} ScanType;

typedef enum
{
    VALUE_BYTE,
    VALUE_2BYTES,
    VALUE_4BYTES,
    VALUE_8BYTES
} ValueType;

extern DynamicArray memory_addresses;  // Dynamic array to store all memory addresses find with scan
extern ResultsTable results_table;     // Memory table to store memory addresses displayed
extern SelectionTable selection_table; // Memory table to store memory addresses selected by user

extern char previous_search_value[MAX_NAME_LEN]; // Previous value trageted by scan
extern char search_value[MAX_NAME_LEN];          // Value the scanner is looking for
extern int search_value_len;                     // Length of the value string
extern int selected_value_type;                  // Value type (0: Byte, 1: 2 Bytes, 2: 4 Bytes, 3: 8 Bytes)
extern int selected_scan_type;                   // Scan type (0: Exact, 1: Bigger, 2. Smaller, 3: Range, 4: Unknown)

bool get_value_size(int type, size_t *value_size);
bool parse_value(const char *input, int type, void *output);
bool refine_results(HANDLE process_handle, LPCVOID target_value, SIZE_T value_size);
bool scan_process_memory(HANDLE process_handle, LPCVOID target_value, SIZE_T value_size);
bool load_results(HANDLE process_handle, ResultsTable *table, const char *search_value_str, const char *previous_search_value_str);

void format_value(const void *value, size_t size, char *output, size_t output_size);
void refine_memory_scan(HANDLE process_handle, ResultsTable *table);
void start_memory_scan(HANDLE process_handle, ResultsTable *table);
void init_selection_table(SelectionTable *table);
void reload_results_table(ResultsTable *table);
void clear_results_table(ResultsTable *table);
void init_results_table(ResultsTable *table);

const char *get_error_string(DWORD error_id);

#endif