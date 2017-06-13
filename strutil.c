#include <stdlib.h>
#include <string.h>

/**
 * Returns a _new_ string which is the same as `original`, except that all
 * occurrences of `replace` are replaced with `with`.
 */
char *strrep(const char *original, const char *replace, const char *with) {
    if (original == NULL || replace == NULL || with == NULL) {
        return NULL;
    }
    if (strlen(original) == 0 || strlen(replace) == 0) {
        return NULL;
    }

    /* Pointer to track current location in `original`. */
    const char *strp = original;
    size_t result_len = 1;
    char *result = malloc(result_len);
    if (result == NULL) {
        return NULL;
    }
    *result = 0;

    char *next = NULL;
    while ((next = strstr(strp, replace)) != NULL) {
        result_len += next - strp + strlen(with);
        result = realloc(result, result_len);
        if (result == NULL) {
            return NULL;
        }
        strncat(result, strp, (next - strp));
        strcat(result, with);
        strp = next + strlen(replace);
    }

    result_len += strlen(strp);
    result = realloc(result, result_len);
    if (result == NULL) {
        return NULL;
    }
    strcat(result, strp);

    return result;
}
