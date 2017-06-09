#ifndef HP4_HP4_H
#define HP4_HP4_H

#ifndef PRINT_DEBUG
#ifdef HP4_DEBUG
#include <stdio.h>
#define PRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else /* HP4_DEBUG */
#define PRINT_DEBUG(...) while(0)
#endif /* HP4_DEBUG */
#endif /* PRINT_DEBUG */

struct hp4_args {
    char *stats_interval;

    char *graph_file;

    char help;
};

#endif /* HP4_HP4_H */
