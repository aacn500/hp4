#ifndef HP4_DEBUG_H
#define HP4_DEBUG_H

#ifndef PRINT_DEBUG
#ifdef HP4_DEBUG
#include <stdio.h>
#define PRINT_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else /* HP4_DEBUG */
#define PRINT_DEBUG(...) while(0)
#endif /* HP4_DEBUG */
#endif /* PRINT_DEBUG */

#ifndef REPORT_ERROR
#include <stdio.h>
#define REPORT_ERROR(what) fprintf(stderr, " ERROR: %s:%s:%d\n   %s\n", \
                                   __FILE__, __func__, __LINE__,        \
                                   what);
#endif /* REPORT_ERROR */

#endif /* HP4_DEBUG_H */
