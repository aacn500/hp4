#include <stdio.h>
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
    if (strlen(original) == 0u || strlen(replace) == 0u) {
        return NULL;
    }
    const char *strp = original;
    size_t result_len = 1;
    char *result = malloc(result_len);
    *result = 0;
    if (result == NULL) {
        return NULL;
    }

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

int main(int argc, char **argv) {
    char *changed = strrep(argv[1], argv[2], argv[3]);
    printf("%s\n", changed);
    free(changed);
    return 0;
}
