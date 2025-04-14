#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    void *data;          // Pointer to the actual data
    size_t size;         // Number of elements in use
    size_t capacity;     // Allocated capacity
    size_t element_size; // Size of each element in bytes
} DynamicArray;

void clear_array(DynamicArray *array, size_t new_initial_capacity, size_t new_element_size);
void create_array(DynamicArray *array, size_t initial_capacity, size_t element_size);
void transfer_array(DynamicArray *dest, DynamicArray *src);
void append(DynamicArray *array, const void *value);
void *get(DynamicArray *array, size_t index);
void resize_array(DynamicArray *array);
void free_array(DynamicArray *array);

#endif