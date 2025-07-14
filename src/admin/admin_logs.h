
#ifndef ADMIN_LOGS_H
#define ADMIN_LOGS_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#ifndef ACCESS_LOG_FILE
#define ACCESS_LOG_FILE  "./src/admin/access.txt"
#endif

void log_access(const char *user,
                const char *src_ip, unsigned src_port,
                const char *dst,    unsigned dst_port,
                unsigned status);

char* dump_access(int n_logs);

char * search_access(const char *user);

void clean_logs(void);

#endif /* ADMIN_LOGS_H */
