/*
 * string_builder.c: Lightweight way to create a string via a sequence of concatenations.
 *
 * Author: Ian Marshall
 * Date: 23/07/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "os_type.h"
#include "espmissingincludes.h"
#include "user_interface.h"

#include "string_builder.h"

LOCAL bool ICACHE_FLASH_ATTR resize_string_builder(string_builder *buf, unsigned int additional_required);

/*
 * Creates a string builder, with an initial size. The resulting builder must eventually be freed with 
 * free_string_builder.
 */
string_builder * ICACHE_FLASH_ATTR create_string_builder(int initial_len) {
    // Create the builder structure.
    string_builder *buf = (string_builder *)os_malloc(sizeof(string_builder));
    if (buf == NULL) {
        return NULL;
    }

    // Fill in the builder length fields.
    buf->allocated = (initial_len < 16) ? 16 : initial_len;
    buf->len = 0;

    // Allocate the requires space within the structure.
    buf->buf = (char *)os_malloc(buf->allocated * sizeof(char));
    if (buf->buf == NULL) {
        os_free(buf);
        return NULL;
    }

    // Ensure the string is empty.
    buf->buf[0] = '\0';

    // Buffer created, return it.
    return buf;
}

/*
 * De-allocates all memory for a string builder (including its contents).
 */
void ICACHE_FLASH_ATTR free_string_builder(string_builder *buf) {
    if (buf != NULL) {
        if (buf->buf != NULL) {
            os_free(buf->buf);
        }
        os_free(buf);
    }
}

/*
 * Appends a string to a pre-existing string builder. The builder is expanded to store the new string if required.
 */
bool ICACHE_FLASH_ATTR append_string_builder(string_builder *buf, const char *str) {
    // Ensure we have space to add the string to the builder.
    int len = os_strlen(str) + 1;
    int free = buf->allocated - buf->len - 1;
    if (free < len) {
        // We need to increase the size of the builder to fit the string in.
        if (!resize_string_builder(buf, len - free)) {
            // We were unable to resize the builder.
            os_printf("Unable to resize builder for string \"%s\".", str);
            return false;
        }
    }

    // Add the string.
    os_memmove(&buf->buf[buf->len], str, len - 1);
    buf->buf[buf->len + len] = '\0';
    buf->len += len - 1;
    return true;
}

/*
 * Appends a string builder to a pre-existing string builder. The builder is expanded to store the new string if 
 * required.
 */
bool ICACHE_FLASH_ATTR append_string_builder_to_string_builder(string_builder *buf, const string_builder *source) {
    // Ensure we have space to add the string to the builder.
    int free = buf->allocated - buf->len - 1;
    if (free < (source->len + 1)) {
        // We need to increase the size of the builder to fit the string in.
        if (!resize_string_builder(buf, source->len - free + 1)) {
            // We were unable to resize the builder.
            os_printf("Unable to resize builder for string builder appending\n.");
            return false;
        }
    }

    // Add the string builder's contents, including the trailing '\0'.
    os_memmove(&buf->buf[buf->len], source->buf, source->len + 1);
    buf->len += source->len;
    return true;
}

/*
 * Appends a 32-bit signed integer to a pre-existing string builder. The builder is expanded to store the new string if 
 * requried.
 */
bool ICACHE_FLASH_ATTR append_int32_string_builder(string_builder *buf, const int32_t val) {
    // Convert the integer to a string.
    char str[12];
    os_sprintf(str, "%d", val);

    // Store the string in the builder.
    return append_string_builder(buf, str);
}

/*
 * Dumps the contents of the builder via "os_printf". Designed for debugging purposes.
 */
void ICACHE_FLASH_ATTR printf_string_builder(string_builder *buf) {
    if (buf == NULL) {
        os_printf("NULL builder.\n");
    } else {
        for (int ii = 0; ii < buf->len; ii++) {
            os_printf("%c", buf->buf[ii]);
        }
    }
}

/*
 * Alters the size of a string builder by increasing the amount of storage available.
 */
LOCAL bool ICACHE_FLASH_ATTR resize_string_builder(string_builder *buf, 
                                                   unsigned int additional_required) {
    // Find the new size of the builder.
    int new_size;
    if ((buf->allocated + buf->allocated - buf->len) < additional_required) {
        // Merely doubling the builder won't help, create the additional requried.
        new_size = buf->len + additional_required;
    } else {
        new_size = buf->allocated + buf->allocated;
    }

    char *new_string;
    new_string = (char *)os_malloc(new_size * sizeof(char));
    if (new_string == NULL) {
        // We couldn't create the new builder to hold the expanded string.
        return false;
    }

    // Copy the old string to the new one.
    char *old_string = buf->buf;
    os_memmove(new_string, old_string, buf->len);
    os_free(old_string);

    // Update the builder structure.
    buf->buf = new_string;
    buf->allocated = new_size;
    return true;
}
