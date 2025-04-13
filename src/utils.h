#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    void *data;          // Pointer to the actual data
    size_t size;         // Number of elements in use
    size_t capacity;     // Allocated capacity
    size_t value_size;   // Bytes per value (1/2/4/8)
    size_t element_size; // Size of each element in bytes
} DynamicArray;

void create_array(DynamicArray *array, size_t initial_capacity, size_t element_size);
void *get(DynamicArray *array, size_t index);
void append(DynamicArray *array, const void *value);
void resize_array(DynamicArray *array);
void free_array(DynamicArray *array);

#endif