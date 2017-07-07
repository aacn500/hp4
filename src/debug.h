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
#define REPORT_ERROR(what) fprintf(stderr, " ERROR: %s:%s:%d\n  " what "\n", \
                                   __FILE__, __func__, __LINE__);
#endif /* REPORT_ERROR */
#ifndef REPORT_ERRORF
#include <stdio.h>
#define REPORT_ERRORF(what, ...) fprintf(stderr, " ERROR: %s:%s:%d\n  " what "\n", \
                                         __FILE__, __func__, __LINE__, __VA_ARGS__);
#endif /* REPORT_ERRORF */

#endif /* HP4_DEBUG_H */
