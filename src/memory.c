#include "memory.h"

DynamicArray memory_addresses;
ResultsTable results_table;
SelectionTable selection_table;

char previous_search_value[MAX_NAME_LEN] = "N/A";
char search_value[MAX_NAME_LEN] = {0};
int search_value_len = 0;
int selected_value_type = VALUE_4BYTES;
int selected_scan_type = SCAN_EXACT_VALUE;

void init_results_table(ResultsTable *table)
{
    clear_results_table(table);

    // Clear all entries in the results and selected arrays
    memset(table, 0, sizeof(ResultsTable));

    table->result_count = 0;
    table->result_capacity = MAX_RESULTS;
    table->results = malloc(table->result_capacity * sizeof(ResultEntry));
}

void clear_results_table(ResultsTable *table)
{
    // Free existing entries
    for (size_t i = 0; i < table->result_count; i++)
    {
        free(table->results[i].value);
        free(table->results[i].previous_value);
        table->results[i].value = NULL;
        table->results[i].previous_value = NULL;
    }

    table->result_count = 0;
}

void init_selection_table(SelectionTable *table)
{
    clear_selection_table(table);

    // Clear all entries in the results and selected arrays
    memset(table, 0, sizeof(SelectionTable));

    // Initialize counts to zero
    table->selection_count = 0;
    table->selection_capacity = MAX_RESULTS;
    table->selection = malloc(table->selection_capacity * sizeof(SelectionEntry));
}

void clear_selection_table(SelectionTable *table)
{
    for (size_t i = 0; i < table->selection_count; i++)
    {
        free(table->selection[i].value);
        free(table->selection[i].previous_value);
        table->selection[i].value = NULL;
        table->selection[i].previous_value = NULL;
        table->selection[i].length = 0;
        table->selection[i].previous_length = 0;
    }
    table->selection_count = 0;
}

void format_value(const void *value, size_t size, char *output, size_t output_size)
{
    const unsigned char *bytes = (const unsigned char *)value;

    switch (size)
    {
    case 1:
        snprintf(output, output_size, "%u", *(uint8_t *)bytes);
        break;
    case 2:
        snprintf(output, output_size, "%u", *(uint16_t *)bytes);
        break;
    case 4:
        snprintf(output, output_size, "%u", *(uint32_t *)bytes);
        break;
    case 8:
        snprintf(output, output_size, "%llu", *(uint64_t *)bytes);
        break;
    default:
        strncpy_s(output, output_size, "???", 4);
    }
}

bool scan_process_memory(HANDLE process_handle, LPCVOID target_value, SIZE_T value_size)
{
    printf("[DEBUG] Starting memory scan for value size: %zu bytes\n", value_size);

    // Parameter validation
    if (process_handle == NULL || process_handle == INVALID_HANDLE_VALUE)
    {
        const char *handle_state = process_handle == NULL ? "NULL" : "INVALID_HANDLE_VALUE";
        fprintf(stderr, "[ERROR] Invalid process handle (%s)\n", handle_state);
        return false;
    }

    if (target_value == NULL)
    {
        fprintf(stderr, "[ERROR] Target value pointer is NULL\n");
        return false;
    }

    if (value_size == 0 || value_size > 8)
    {
        fprintf(stderr, "[ERROR] Invalid value size: %zu (must be 1-8 bytes)\n", value_size);
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi;
    LPVOID current_address = 0;
    SIZE_T total_regions = 0;
    SIZE_T scanned_chunks = 0;
    SIZE_T read_errors = 0;
    SIZE_T matches_found = 0;
    SIZE_T skipped_regions = 0;
    SIZE_T partial_reads = 0;

    printf("[DEBUG] Beginning memory enumeration...\n");

    while (1)
    {
        SIZE_T bytes_returned = VirtualQueryEx(process_handle, current_address, &mbi, sizeof(mbi));

        // Region enumeration error handling
        if (bytes_returned == 0)
        {
            DWORD error = GetLastError();

            if (error == ERROR_INVALID_PARAMETER)
            {
                printf("[DEBUG] Reached end of process memory space\n");
                break;
            }

            fprintf(stderr, "[ERROR] VirtualQueryEx failed at 0x%p (Error 0x%lx: %s)\n",
                    current_address, error, get_error_string(error));
            return false;
        }

        total_regions++;
        printf("[DEBUG] Region %zu: 0x%p-0x%p (%zu bytes) State: 0x%lx Protect: 0x%lx\n",
               total_regions, mbi.BaseAddress,
               (LPVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize),
               mbi.RegionSize, mbi.State, mbi.Protect);

        // Skip uncommitted or reserved memory regions
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        {
            skipped_regions++;
            printf("[DEBUG] Skipping region (State: 0x%lx, Protect: 0x%lx)\n", mbi.State, mbi.Protect);
            current_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
            continue;
        }

        // Process the region in manageable chunks
        printf("[DEBUG] Scanning committed region of %zu bytes\n", mbi.RegionSize);
        for (SIZE_T offset = 0; offset < mbi.RegionSize; offset += CHUNK_SIZE)
        {
            SIZE_T chunk_size = min(CHUNK_SIZE, mbi.RegionSize - offset);
            LPVOID chunk_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + offset);
            BYTE *buffer = malloc(chunk_size);

            if (!buffer)
            {
                fprintf(stderr, "[ERROR] Failed to allocate %zu bytes for chunk buffer\n", chunk_size);
                continue;
            }

            SIZE_T bytes_read;
            if (!ReadProcessMemory(process_handle, chunk_address, buffer, chunk_size, &bytes_read))
            {
                DWORD error = GetLastError();
                fprintf(stderr, "[ERROR] ReadProcessMemory failed at 0x%p (Error 0x%lx: %s)\n", chunk_address, error, get_error_string(error));
                read_errors++;
                free(buffer);
                continue;
            }

            if (bytes_read != chunk_size)
            {
                partial_reads++;
                printf("[WARNING] Partial read at 0x%p (%zu/%zu bytes)\n", chunk_address, bytes_read, chunk_size);
            }

            // Scan the chunk content
            for (SIZE_T i = 0; i <= bytes_read - value_size; i++)
            {
                if (memcmp(buffer + i, target_value, value_size) == 0)
                {
                    LPVOID found_addr = (LPVOID)((ULONG_PTR)chunk_address + i);
                    append(&memory_addresses, &found_addr);
                    matches_found++;
                }
            }

            scanned_chunks++;
            free(buffer);
        }
        current_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
    }
    printf("[DEBUG] Memory scan complete\n"
           "  Total regions processed: %zu\n"
           "  Skipped regions: %zu\n"
           "  Scanned chunks: %zu\n"
           "  Read errors: %zu\n"
           "  Total matches found: %zu\n",
           total_regions, skipped_regions, scanned_chunks,
           read_errors, matches_found);

    printf("Number of addresses found: %zu\n", memory_addresses.size);
    return memory_addresses.size > 0;
}

bool parse_value(const char *input, int type, void *output)
{
    char *endptr;
    unsigned long long tmp;

    // Handle hex format
    if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X'))
        tmp = strtoull(input, &endptr, 16);
    else
        tmp = strtoull(input, &endptr, 10);

    if (endptr == input || *endptr != '\0')
        return false;

    // Validate type ranges
    switch (type)
    {
    case VALUE_BYTE:
        if (tmp > UINT8_MAX)
            return false;
        *(uint8_t *)output = (uint8_t)tmp;
        break;
    case VALUE_2BYTES:
        if (tmp > UINT16_MAX)
            return false;
        *(uint16_t *)output = (uint16_t)tmp;
        break;
    case VALUE_4BYTES:
        if (tmp > UINT32_MAX)
            return false;
        *(uint32_t *)output = (uint32_t)tmp;
        break;
    case VALUE_8BYTES:
        *(uint64_t *)output = (uint64_t)tmp;
        break;
    default:
        return false;
    }
    return true;
}

bool get_value_size(int type, size_t *value_size)
{
    switch (type)
    {
    case VALUE_BYTE:
        *value_size = 1;
        return true;
    case VALUE_2BYTES:
        *value_size = 2;
        return true;
    case VALUE_4BYTES:
        *value_size = 4;
        return true;
    case VALUE_8BYTES:
        *value_size = 8;
        return true;
    default:
        return false;
    }
}

void start_memory_scan(HANDLE process_handle, ResultsTable *table)
{
    uint64_t parsed_value;
    size_t value_size;

    // Parse input value
    if (!parse_value(search_value, selected_value_type, &parsed_value))
    {
        fprintf(stderr, "Invalid input value!\n");
        return;
    }

    if (!get_value_size(selected_value_type, &value_size))
    {
        fprintf(stderr, "Invalid value type!\n");
        return;
    }

    // Clear previous results
    table->result_count = 0;
    memset(table->results, 0, table->result_capacity * sizeof(ResultEntry));
    clear_array(&memory_addresses, 100000, sizeof(LPVOID));
    char previous_search_value[MAX_NAME_LEN] = "N/A";

    // Start the scan
    if (!scan_process_memory(process_handle, &parsed_value, value_size))
    {
        fprintf(stderr, "No matching values found!\n");
        return;
    }

    clear_results_table(table);

    if (!load_results(process_handle, table, search_value, previous_search_value))
    {
        fprintf(stderr, "Failed to load results addresses!\n");
        return;
    }

    strncpy_s(previous_search_value, sizeof(previous_search_value), search_value, _TRUNCATE);
}

void refine_memory_scan(HANDLE process_handle, ResultsTable *table)
{
    uint64_t parsed_value;
    size_t value_size;

    // Parse input value
    if (!parse_value(search_value, selected_value_type, &parsed_value))
    {
        fprintf(stderr, "Invalid input value!\n");
        return;
    }

    if (!get_value_size(selected_value_type, &value_size))
    {
        fprintf(stderr, "Invalid value type!\n");
        return;
    }

    // Refine the scan results
    if (!refine_results(process_handle, &parsed_value, value_size))
    {
        fprintf(stderr, "No matching values found!\n");
        return;
    }

    clear_results_table(table);

    if (!load_results(process_handle, table, search_value, previous_search_value))
    {
        fprintf(stderr, "Failed to load results addresses!\n");
        return;
    }

    strncpy_s(previous_search_value, sizeof(previous_search_value), search_value, _TRUNCATE);
}

bool refine_results(HANDLE process_handle, LPCVOID target_value, SIZE_T value_size)
{
    printf("[DEBUG] Starting refine_results...\n");

    // Parameter validation
    if (process_handle == NULL)
    {
        fprintf(stderr, "Error: Invalid process handle (NULL)\n");
        return false;
    }
    if (target_value == NULL)
    {
        fprintf(stderr, "Error: Target value pointer is NULL\n");
        return false;
    }
    if (value_size == 0 || value_size > 8)
    {
        fprintf(stderr, "Error: Invalid value_size (%zu)\n", value_size);
        return false;
    }

    printf("[DEBUG] Creating temporary array (capacity: %zu)\n", memory_addresses.capacity);
    DynamicArray temp_array;
    create_array(&temp_array, memory_addresses.capacity, sizeof(LPVOID));

    size_t total_matches = 0;
    size_t read_errors = 0;
    size_t partial_reads = 0;

    printf("[DEBUG] Scanning %zu addresses...\n", memory_addresses.size);
    for (size_t i = 0; i < memory_addresses.size; i++)
    {
        LPVOID addr = *(LPVOID *)get(&memory_addresses, i);
        uint8_t buffer[8] = {0};
        SIZE_T bytes_read;

        // printf("[DEBUG] Checking address: 0x%p\n", addr);

        if (!ReadProcessMemory(process_handle, addr, buffer, value_size, &bytes_read))
        {
            DWORD error = GetLastError();
            fprintf(stderr, "ReadProcessMemory failed at 0x%p (Error: 0x%lx : %s)\n", addr, error, get_error_string(error));
            read_errors++;
            continue;
        }

        if (bytes_read != value_size)
        {
            fprintf(stderr, "Partial read at 0x%p (%zu/%zu bytes)\n",
                    addr, bytes_read, value_size);
            partial_reads++;
            continue;
        }

        if (memcmp(buffer, target_value, value_size) == 0)
        {
            printf("[MATCH] Found matching value at 0x%p\n", addr);
            append(&temp_array, &addr);
            total_matches++;
        }
    }

    printf("[DEBUG] Scan complete. Results:\n"
           "  Total addresses processed: %zu\n"
           "  Successful matches: %zu\n"
           "  Read errors: %zu\n"
           "  Partial reads: %zu\n",
           memory_addresses.size, total_matches, read_errors, partial_reads);

    printf("[DEBUG] Transferring results to main array\n");
    transfer_array(&memory_addresses, &temp_array);
    printf("[DEBUG] New address count: %zu\n", memory_addresses.size);

    return memory_addresses.size > 0;
}

bool load_results(HANDLE process_handle, ResultsTable *table, const char *search_value_str, const char *previous_search_value_str)
{
    if (!process_handle || !table || !table->results)
    {
        fprintf(stderr, "Invalid parameters in load_results!\n");
        return false;
    }

    size_t entries_to_show = min(MAX_RESULTS, memory_addresses.size);

    for (size_t i = 0; i < entries_to_show; i++)
    {
        if (table->result_count >= table->result_capacity)
        {
            break;
        }

        LPVOID addr = *(LPVOID *)get(&memory_addresses, i);

        ResultEntry entry = {
            .address = addr,
            .value = _strdup(search_value_str),
            .previous_value = _strdup(previous_search_value_str)};

        // Check for allocation errors
        if (!entry.value || !entry.previous_value)
        {
            fprintf(stderr, "Memory allocation failed for entry %zu\n", i);
            free(entry.value);
            free(entry.previous_value);
            return false;
        }

        table->results[table->result_count] = entry;
        table->result_count++;
    }
    return true;
}

const char *get_error_string(DWORD error_id)
{
    static char buffer[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error_id, 0, buffer, sizeof(buffer), NULL);
    return buffer;
}