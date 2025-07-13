#ifndef TPE_PROTOS_ADMIN_LOGS_H
#define TPE_PROTOS_ADMIN_LOGS_H


#define USER_FILE "./src/admin/admin_logs.txt"

#include <stdint.h>
#include <stddef.h>

void admin_log_record(const char *username,     /* "-" si no hay     */
                      const char *src_ip, uint16_t src_port,
                      const char *dst_host, uint16_t dst_port,
                      const char *stage,        /* "CONNECTED", "AUTH", … */
                      const char *result,       /* "OK" / "FAIL" / "-"    */
                      size_t bytes_up, size_t bytes_down,
                      uint32_t dur_ms);         /* 0 si no aplica        */

size_t admin_log_tail(char *out, size_t out_sz, size_t max_lines);

size_t admin_log_tail_user(char *out, size_t out_sz,
                           const char *user, size_t max_lines);

void  admin_log_clear(void);

#endif /* ADMIN_LOG_H */

