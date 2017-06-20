#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

/**
 * Returns a _new_ string which is the same as `original`, except that all
 * occurrences of `replace` are replaced with `with`.
 *
 * On error: returns NULL
 */
char *strrep(const char *original, const char *replace, const char *with) {
    if (original == NULL || replace == NULL || with == NULL) {
        REPORT_ERROR("NULL is not a valid argument");
        return NULL;
    }
    if (strlen(original) == 0 || strlen(replace) == 0) {
        REPORT_ERROR("0-length string is not a valid argument for original or replace");
        return NULL;
    }
    char *newmem;
    /* Pointer to track current location in `original`. */
    const char *strp = original;
    size_t result_len = 1;
    char *result = malloc(result_len);
    if (result == NULL) {
        REPORT_ERROR(strerror(errno));
        return NULL;
    }
    *result = 0;

    char *next = NULL;
    while ((next = strstr(strp, replace)) != NULL) {
        result_len += next - strp + strlen(with);
        newmem = realloc(result, result_len);
        if (newmem == NULL) {
            REPORT_ERROR(strerror(errno));
            free(result);
            return NULL;
        }
        result = newmem;
        strncat(result, strp, (next - strp));
        strcat(result, with);
        strp = next + strlen(replace);
    }

    result_len += strlen(strp);
    newmem = realloc(result, result_len);
    if (newmem == NULL) {
        REPORT_ERROR(strerror(errno));
        free(result);
        return NULL;
    }
    result = newmem;
    strcat(result, strp);

    return result;
}
