
#ifndef ACCESS_LOG_H
#define ACCESS_LOG_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#ifndef ACCESS_LOG_FILE
#define ACCESS_LOG_FILE  "../admin/access.log"
#endif

void log_access(const char *user,
                const char *src_ip, unsigned src_port,
                const char *dst,    unsigned dst_port,
                unsigned status);

void dump_access(void);

void search_access(const char *user);

void clean_logs(void);

#endif /* ACCESS_LOG_H */
