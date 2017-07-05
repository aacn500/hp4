#include "config.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "strutil.h"

/* Split arg string on spaces, except when between openblock,closeblock chars
 * Adapted from https://stackoverflow.com/a/26201820 */
int parse_argstring(struct argstruct *pa, const char *input) {
    if (pa == NULL) {
        return -1;
    }

    if (input == NULL) {
        return -1;
    }

    char *input_cpy = malloc((strlen(input) + 1) * sizeof(*input_cpy));
    if (input_cpy == NULL) {
        REPORT_ERROR(strerror(errno));
        return -1;
    }
    strcpy(input_cpy, input);
    char *token = input_cpy;
    char *current = input_cpy;
    char *block = NULL;

    /* NB. each block-closing character MUST be placed at the same index as
     *     its matching block-opening character to show the relationship.
     * NB. nested blocks will not be respected.
     * Separated openblock + closeblock as they could in future be extended
     * to allow e.g. parentheses as block characters. */
    const char openblock[] =  "\"'";
    const char closeblock[] = "\"'";
    const char delimiter = ' ';

    bool in_block = false;
    int block_idx = -1;

    int argc = 0;
    char **argv = NULL;

    while (*token != '\0') {
        if (!in_block && (block_idx >= 0)) {
            REPORT_ERROR("Not in a block but block_idx is set");
            free(argv);
            free(input_cpy);
            return -1;
        }
        else if (in_block && (*token == closeblock[block_idx])) {
            in_block = false;
            block_idx = -1;
            *token = delimiter;
            continue; /* avoid incrementing token */
        }
        else if (!in_block && ((block = strchr(openblock, *token)) != NULL)) {
            in_block = true;
            /* get index of character which is opening block */
            block_idx = block - openblock;
            /* don't add quote mark to final string */
            if (current == token)
                ++current;
        }
        else if (!in_block && *token == delimiter) {
            if (current == token) {
                /* skip adding empty strings to argv */
                ++current;
            }
            else {
                *token = '\0';
                char **nargv = realloc(argv, sizeof(*argv) * ++argc);
                if (nargv == NULL) {
                    REPORT_ERROR(strerror(errno));
                    free(argv);
                    free(input_cpy);
                    return -1;
                }
                argv = nargv;
                argv[argc-1] = current;
                current = token + 1;
            }
        }
        ++token;
    }

    if (in_block) {
        REPORT_ERROR("Invalid string; block was not terminated");
        free(argv);
        free(input_cpy);
        return -1;
    }
    else if (current == token) {
        /* do not add empty string at end of argv */
        char **nargv = realloc(argv, sizeof(*argv) * (argc + 1));
        if (nargv == NULL) {
            REPORT_ERROR(strerror(errno));
            free(argv);
            free(input_cpy);
            return -1;
        }
        argv = nargv;
        argv[argc] = NULL;
    }
    else {
        char **nargv = realloc(argv, sizeof(*argv) * (++argc + 1));
        if (nargv == NULL) {
            REPORT_ERROR(strerror(errno));
            free(argv);
            free(input_cpy);
            return -1;
        }
        argv = nargv;
        argv[argc-1] = current;
        argv[argc] = NULL;
    }

    if (pa == NULL) {
        REPORT_ERROR(strerror(errno));
        free(argv);
        free(input_cpy);
        return -1;
    }
    pa->argc = argc;
    pa->argv = argv;
    return 0;
}

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
