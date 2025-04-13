#include "memory.h"

DynamicArray memory_addresses;
MemoryTable memory_table;

char previous_search_value[MAX_NAME_LEN] = "N/A";
char search_value[MAX_NAME_LEN] = {0};
int search_value_len = 0;
int selected_value_type = VALUE_4BYTES;
int selected_scan_type = SCAN_EXACT_VALUE;

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

bool scan_process_memory(
    HANDLE process_handle,
    LPCVOID target_value,
    SIZE_T value_size)
{
    if (process_handle == NULL || process_handle == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Invalid process handle\n");
        return false;
    }

    if (target_value == NULL || value_size == 0)
    {
        fprintf(stderr, "Invalid target value parameters\n");
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi;
    LPVOID current_address = 0;
    SIZE_T bytes_read;
    SIZE_T bytes_returned;
    BOOL success = TRUE;

    while (success)
    {
        bytes_returned = VirtualQueryEx(process_handle, current_address, &mbi, sizeof(mbi));

        // Error handling
        if (bytes_returned == 0)
        {
            DWORD error = GetLastError();

            // Expected termination when we reach the end of memory space
            if (error == ERROR_INVALID_PARAMETER)
            {
                break; // Normal termination condition
            }

            // Handle other errors
            fprintf(stderr, "VirtualQueryEx failed at address 0x%p, error: %lu\n",
                    (void *)current_address, error);
            success = FALSE;
            break;
        }

        // Skip uncommitted or reserved memory regions
        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        {
            current_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
            continue;
        }

        // Process the region in manageable chunks
        for (SIZE_T offset = 0; offset < mbi.RegionSize; offset += CHUNK_SIZE)
        {
            SIZE_T chunk_size = min(CHUNK_SIZE, mbi.RegionSize - offset);
            LPVOID chunk_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + offset);
            BYTE *buffer = malloc(chunk_size);

            if (ReadProcessMemory(process_handle, chunk_address, buffer, chunk_size, &bytes_read))
            {
                // Scan the chunk content
                for (SIZE_T i = 0; i <= bytes_read - value_size; i++)
                {
                    if (memcmp(buffer + i, target_value, value_size) == 0)
                    {
                        LPVOID found_addr = (LPVOID)((ULONG_PTR)chunk_address + i);
                        append(&memory_addresses, &found_addr);
                    }
                }
            }
            free(buffer);
        }
        current_address = (LPVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
    }
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

void start_memory_scan(HANDLE process_handle, int type, int scan_type, MemoryTable *table)
{
    uint64_t parsed_value;
    size_t value_size;

    // Parse input value
    if (!parse_value(search_value, type, &parsed_value))
    {
        fprintf(stderr, "Invalid input value!\n");
        return;
    }

    if (!get_value_size(type, &value_size))
    {
        fprintf(stderr, "Invalid value type!\n");
        return;
    }

    // Clear previous results
    table->result_count = 0;

    // Start the scan
    if (!scan_process_memory(process_handle, &parsed_value, value_size))
    {
        fprintf(stderr, "No matching values found!\n");
        return;
    }

    if (!load_results(process_handle, table, search_value, previous_search_value))
    {
        fprintf(stderr, "Failed to load results addresses!\n");
        return;
    }
}

bool load_results(HANDLE process_handle, MemoryTable *table, const char *search_value_str, const char *previous_search_value_str)
{
    if (process_handle == NULL || table == NULL || table->results == NULL)
    {
        fprintf(stderr, "Invalid parameters!\n");
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

        MemoryEntry entry = {
            .address = addr,
            .value = _strdup(search_value_str),
            .previous_value = _strdup(previous_search_value_str)};

        table->results[table->result_count] = entry;
        printf("Added entry %zu/%zu (address: %p, value: %s)\n", table->result_count, table->result_capacity - 1, addr, table->results[table->result_count].value);
        table->result_count++;
    }
    return true;
}