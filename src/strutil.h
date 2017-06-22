#ifndef HP4_STRUTIL_H
#define HP4_STRUTIL_H

struct argstruct {
    int argc;
    char **argv;
};

int parse_argstring(struct argstruct *pa, const char *args);

char *strrep(const char *original, const char *replace, const char *with);

#endif /* HP4_STRUTIL_H */
