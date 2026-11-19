/*
 * URL parts of HTTP API requests (implemented in Rust,
 * components/rust_core/src/http_query.rs): history query parameters, the
 * allTime flag, CAN node IDs and actions. Numbers are read like newlib did
 * on the device (32-bit strtoul/atoi).
 */

#ifndef HTTP_QUERY_H
#define HTTP_QUERY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t start;    /* ts: start (client time, ms), 0: none */
    uint64_t current;  /* cur: the client's current time (ms) */
    uint64_t span_ms;  /* historySpan (v2) / history_span (v1): 1 h default, at most 3 h */
    uint32_t limit;    /* limit: at most 1000 samples, 0: no limit */
    bool requested;    /* ts given and not 0 */
    bool has_current;  /* cur given */
} history_query_t;

/* query: the URL query (httpd_req_get_url_query_str), NULL if none. */
void http_history_query(const char *query, bool v2, history_query_t *out);
/* allTime=1 or allTime=true */
bool http_query_all_time(const char *query);

/* /api/v2/can/nodes/{id}: the ID (1..31), -1 missing, -2 invalid. */
int http_can_node_id(const char *uri);
/* /api/v2/can/nodes/{id}/{action}: the ID or -2 (invalid);
 * *action: 0 restart, 1 shutdown, 2 identify, -1 unknown. */
int http_can_node_action(const char *uri, int *action);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_QUERY_H */
