#include "utils.h"

// Function to create a new dynamic array
void create_array(DynamicArray *array, size_t initial_capacity, size_t element_size)
{
    if (!array)
    {
        fprintf(stderr, "Error: NULL array passed to create_array\n");
        exit(EXIT_FAILURE);
    }

    array->data = malloc(initial_capacity * element_size);
    if (!array->data)
    {
        perror("Failed to allocate memory for data");
        exit(EXIT_FAILURE);
    }

    array->size = 0;
    array->capacity = initial_capacity;
    array->element_size = element_size;
}

// Function to resize the array with a growth factor of 1.5
void resize_array(DynamicArray *array)
{
    size_t new_capacity = (size_t)(array->capacity * 1.5);
    if (new_capacity == array->capacity)
    {
        new_capacity++; // Ensure the capacity grows
    }
    void *new_data = realloc(array->data, new_capacity * array->element_size);
    if (!new_data)
    {
        perror("Failed to reallocate memory");
        exit(EXIT_FAILURE);
    }
    array->data = new_data;
    array->capacity = new_capacity;
}

// Function to add an element to the array
void append(DynamicArray *array, const void *value)
{
    if (array->size == array->capacity)
    {
        resize_array(array); // Grow the array if full
    }
    // Copy the value into the next position in the array
    memcpy((char *)array->data + (array->size * array->element_size), value, array->element_size);
    array->size++;
}

// Function to get an element from the array
void *get(DynamicArray *array, size_t index)
{
    if (index >= array->size)
    {
        fprintf(stderr, "Index out of bounds\n");
        exit(EXIT_FAILURE);
    }
    return (char *)array->data + (index * array->element_size);
}

// Function to free the dynamic array
void free_array(DynamicArray *array)
{
    free(array->data);
    free(array);
}