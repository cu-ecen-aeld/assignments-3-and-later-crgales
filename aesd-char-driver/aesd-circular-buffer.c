/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    int index;
    int offset = char_offset;
    int entry_count = 0;

    if (buffer == NULL || entry_offset_byte_rtn == NULL)
    {
        return NULL;
    }

    index = buffer->out_offs;

    while (entry_count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED && buffer->entry[index].size <= offset)
    {
        offset -= buffer->entry[index].size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        entry_count++;
    }

    if (entry_count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        *entry_offset_byte_rtn = offset;
        return &buffer->entry[index];
    }
    
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    if(buffer == NULL || add_entry == NULL)
    {
        return;
    }

    buffer->entry[buffer->in_offs] = *add_entry;
    
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    if(buffer->full)
    {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    
    if(buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }
    else
    {
        buffer->full = false;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

/**
 * @param buffer the buffer to search for the next entry
 * @param entry the current entry, or NULL to start at the beginning of the buffer
 * @return the next entry in the buffer after @param entry, or the first entry if @param entry is NULL
 */
struct aesd_buffer_entry *aesd_circular_buffer_get_next_entry(struct aesd_circular_buffer *buffer, struct aesd_buffer_entry *entry)
{
    int index;
    if (buffer == NULL)
    {
        return NULL;
    }

    if (entry == NULL)
    {
        index = buffer->out_offs;
    }
    else
    {
        index = 0;
        while (index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED && &buffer->entry[index] != entry)
        {
            index++;
        }
        if (index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) return NULL;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    return &buffer->entry[index];
}
