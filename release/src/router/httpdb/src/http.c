/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include "mongoose.h"
#include <sys/queue.h>
#include <getopt.h>
#include "dbapi.h"
#include "khash.h"

#define MAX_IDLE_CONNS 5
#define CONN_IDLE_TIMEOUT 30

#define PREFIX_API "/_api/"
#define PREFIX_API_LEN (6)
#define PREFIX_ROOT "/_root/"
#define PREFIX_ROOT_LEN (7)
#define PREFIX_TEMP "/_temp/"
#define PREFIX_TEMP_LEN (7)
#define PREFIX_RESP "/_resp/"
#define PREFIX_RESP_LEN (7)
#define PREFIX_RESULT "/_result/"
#define PREFIX_RESULT_LEN (9)

// 106.186.20.48
// gcc -o http ../../mongoose.c  http.c
// curl -X POST -d '{"id":3434343, "method":"echo", "params":[33,44],"fields":{"hello":"cc","hd":"oooo","hf":"nnn"}}' http://127.0.0.1:8000/_api/
// curl -X POST -d 'test' http://127.0.0.1:8000/_resp/3434343
// curl -X GET http://127.0.0.1:8000/_result/3434343
// gcc -DMG_ENABLE_SSL -lssl  -o http ../../mongoose.c  http.c
// ./load_balancer -p 8090 -l -  -k  -b / 192.168.6.1:80
// openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 1000 -nodes
// cat key.pem > ssl.pem; cat cert.pem >> ssl.pem

struct http_backend;

struct be_conn {
    struct http_backend *be;
    struct mg_connection *nc;
    time_t idle_deadline;

    STAILQ_ENTRY(be_conn) conns;
};

STAILQ_HEAD(be_conn_list_head, be_conn);
struct http_backend {
    const char *vhost;      /* NULL if any host */
    const char *uri_prefix; /* URI prefix, e.g. "/api/v1/", "/static/" */
    const char *uri_prefix_replacement; /* if not NULL, will replace uri_prefix in
                                           requests to backends */
    const char *host_port;              /* Backend address */
    int redirect;                       /* if true redirect instead of proxy */
    int usage_counter; /* Number of times this backend was chosen */

    struct be_conn_list_head conns;
    int num_conns;
};

struct peer {
    struct mg_connection *nc;
    int64_t body_len;  /* Size of the HTTP body to forward */
    int64_t body_sent; /* Number of bytes already forwarded */
    struct {
        /* Headers have been sent, no more headers. */
        unsigned int headers_sent : 1;
        unsigned int keep_alive : 1;
    } flags;
};

struct conn_data {
    struct be_conn *be_conn; /* Chosen backend */
    struct peer client;      /* Client peer */
    struct peer backend;     /* Backend peer */
    time_t last_activity;
    int https;
    int id;
};

#define DEFAULT_RESP_LEN (127)
struct json_req {
    STAILQ_ENTRY(json_req) reqs;

    struct mg_connection* nc;
    int id;
    char* p;
    int len;
    int cap;
    time_t deadline;
    char buf[DEFAULT_RESP_LEN+1];
};

typedef struct json_req* pjson_req;
STAILQ_HEAD(json_req_list_head, json_req);
KHASH_MAP_INIT_INT(32, pjson_req);

struct json_req_mgr {
    time_t last_check;
    struct json_req_list_head req_list;
    khash_t(32) *req_map;
    int total;
};

static struct json_req_mgr sreq_mgr = {0};

/* json list context */
struct dblist_ctx {
    struct mg_connection *nc;
    int index;
};

static const char *s_error_500 = "HTTP/1.1 500 Failed\r\n";
static const char *s_content_len_0 = "Content-Length: 0\r\n";
static const char *s_connection_close = "Connection: close\r\n";
static struct http_backend s_vhost_backends[100], s_default_backends[100];
static int s_num_vhost_backends = 0, s_num_default_backends = 0;
static int s_sig_num = 0;
static int s_backend_keepalive = 0;
static FILE *s_log_file = NULL;
static struct mg_serve_http_opts s_http_server_opts = {0};
static struct mg_serve_http_opts s_http_tmp_opts = {0};

static void init_req_mgr(struct json_req_mgr* mgr) {
    mgr->req_map = kh_init(32);
    STAILQ_INIT(&mgr->req_list);
    mgr->total = 0;
    mgr->last_check = 0;
}

static void check_timeout(struct json_req_mgr* mgr, time_t now) {
    khiter_t k;
    struct json_req* req;

    if((now - mgr->last_check) < 3) {
        return;
    }
    mgr->last_check = now;
    //printf("check total=%d\n", mgr->total);

    while (!STAILQ_EMPTY(&mgr->req_list)) {
        req = STAILQ_FIRST(&mgr->req_list);
        if (now > req->deadline) {
            //timeout
            STAILQ_REMOVE_HEAD(&mgr->req_list, reqs);

            k = kh_get(32, mgr->req_map, req->id);
            if(k != kh_end(mgr->req_map)) {
                //exists then delete it
                kh_del(32, mgr->req_map, k);
            }

            //free it
            if(req->p != NULL && req->p != req->buf) {
                free(req->p);
                req->p = NULL;
            }

            if(NULL != req->nc) {
                mg_printf_http_chunk(req->nc, "{ \"result\": %d }", -6);
                mg_send_http_chunk(req->nc, "", 0);
                req->nc->flags |= MG_F_SEND_AND_CLOSE;
                req->nc = NULL;
            }

            free(req);

            mgr->total--;
        } else {
            break;
        }
    }

}

static void delete_nc_by_id(struct json_req_mgr* mgr, int id) {
    khiter_t k;
    struct json_req* req;

    k = kh_get(32, mgr->req_map, id);
    if(k != kh_end(mgr->req_map)) {
        req = kh_value(mgr->req_map, k);
        req->nc = NULL;
    }
}

static int set_json_response(struct json_req_mgr* mgr, int id, const char* buf, int buf_len) {
    khiter_t k;
    pjson_req req;

    k = kh_get(32, mgr->req_map, id);
    if(k == kh_end(mgr->req_map)) {
        return -1;
    }
    req = kh_value(mgr->req_map, k);
    if(NULL == req->p) {
        req->p = req->buf;
        req->cap = DEFAULT_RESP_LEN;
    }

    if(req->cap < buf_len) {
        //now enough
        if(req->p != req->buf) {
            //free the alloc buf
            free(req->p);
        }

        req->p = (char*)malloc(buf_len+1);
        req->cap = buf_len;
        if (NULL == req->p) {
            req->cap = 0;
            return -2;
        }
    }

    req->len = buf_len;
    memcpy(req->p, buf, buf_len);
    req->p[buf_len] = '\0';

    return 0;
}

static int new_json_request(struct json_req_mgr* mgr, struct mg_connection *nc, int id, time_t now) {
    khiter_t k;
    int ret;
    pjson_req req = (pjson_req)malloc(sizeof(struct json_req));

    if(NULL == req) {
        return -2;
    }

    req->id = id;
    req->p = NULL;
    req->len = 0;
    req->cap = 0;
    req->deadline = now + CONN_IDLE_TIMEOUT - 5;
    req->nc = nc;

    k = kh_put(32, mgr->req_map, id, &ret);
    kh_value(mgr->req_map, k) = req;

    STAILQ_INSERT_TAIL(&mgr->req_list, req, reqs);
    mgr->total++;

    return 0;
}

static int show_shell_resp(struct json_req_mgr* mgr, int id, struct mg_connection *nc) {
    khiter_t k;
    pjson_req req;

    k = kh_get(32, mgr->req_map, id);
    if(k == kh_end(mgr->req_map)) {
        return -1;
    }
    req = kh_value(mgr->req_map, k);
    if(NULL != req->p) {
        if(nc != NULL) {
            mg_printf_http_chunk(nc, "{\"result\": \"%s\"}", req->p);
        } else if(req->nc != NULL) {
            mg_printf_http_chunk(req->nc, "{\"result\": \"%s\"}", req->p);
            mg_send_http_chunk(req->nc, "", 0);
            req->nc->flags |= MG_F_SEND_AND_CLOSE;
            req->nc = NULL;
        }
        return 0;
    }

    return -1;
}

static void ev_handler_http(struct mg_connection *nc, int ev, void *ev_data);
static void ev_handler_https(struct mg_connection *nc, int ev, void *ev_data);

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, int https);
static void write_log(const char *fmt, ...);

static void signal_handler(int sig_num) {
    signal(sig_num, signal_handler);
    s_sig_num = sig_num;
}

static void send_http_err(struct mg_connection *nc, const char *err_line) {
    mg_printf(nc, "%s%s%s\r\n", err_line, s_content_len_0, s_connection_close);
}

static void respond_with_error(struct conn_data *conn, const char *err_line) {
    struct mg_connection *nc = conn->client.nc;
    int headers_sent = conn->client.flags.headers_sent;
#ifdef DEBUG
    write_log("conn=%p nc=%p respond_with_error %d\n", conn, nc, headers_sent);
#endif
    if (nc == NULL) return;
    if (!headers_sent) {
        send_http_err(nc, err_line);
        conn->client.flags.headers_sent = 1;
    }
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

static int has_prefix(const struct mg_str *uri, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    return uri->len >= prefix_len && memcmp(uri->p, prefix, prefix_len) == 0;
}

static int matches_vhost(const struct mg_str *host, const char *vhost) {
    size_t vhost_len;
    if (vhost == NULL) {
        return 1;
    }
    vhost_len = strlen(vhost);
    return host->len == vhost_len && memcmp(host->p, vhost, vhost_len) == 0;
}

static void write_log(const char *fmt, ...) {
    va_list ap;
    if (s_log_file != NULL) {
        va_start(ap, fmt);
        vfprintf(s_log_file, fmt, ap);
        fflush(s_log_file);
        va_end(ap);
    }
}

static struct http_backend *choose_backend_from_list(
        struct http_message *hm, struct http_backend *backends, int num_backends) {
    int i;
    struct mg_str vhost = {"", 0};
    const struct mg_str *host = mg_get_http_header(hm, "host");
    if (host != NULL) vhost = *host;

    const char *vhost_end = vhost.p;

    while (vhost_end < vhost.p + vhost.len && *vhost_end != ':') {
        vhost_end++;
    }
    vhost.len = vhost_end - vhost.p;
    //printf("vhost.p=%s\n", vhost.p);

    struct http_backend *chosen = NULL;
    for (i = 0; i < num_backends; i++) {
        struct http_backend *be = &backends[i];
        //printf("be->vhost=%s hm->uri=%s prefix=%s\n", be->vhost, hm->uri.p, be->uri_prefix);
        if (has_prefix(&hm->uri, be->uri_prefix) &&
                matches_vhost(&vhost, be->vhost) &&
                (chosen == NULL ||
                 /* Prefer most specific URI prefixes */
                 strlen(be->uri_prefix) > strlen(chosen->uri_prefix) ||
                 /* Among prefixes of the same length chose the least used. */
                 (strlen(be->uri_prefix) == strlen(chosen->uri_prefix) &&
                  be->usage_counter < chosen->usage_counter))) {
            chosen = be;
        }
    }

    return chosen;
}

static struct http_backend *choose_backend(struct http_message *hm) {
    struct http_backend *chosen =
        choose_backend_from_list(hm, s_vhost_backends, s_num_vhost_backends);

    /* Nothing was chosen for this vhost, look for vhost == NULL backends. */
    if (chosen == NULL) {
        chosen = choose_backend_from_list(hm, s_default_backends,
                s_num_default_backends);
    }

    if (chosen != NULL) chosen->usage_counter++;

    return chosen;
}

static void forward_body(struct peer *src, struct peer *dst) {
    struct mbuf *src_io = &src->nc->recv_mbuf;
    if (src->body_sent < src->body_len) {
        size_t to_send = src->body_len - src->body_sent;
        if (src_io->len < to_send) {
            to_send = src_io->len;
        }
        mg_send(dst->nc, src_io->buf, to_send);
        src->body_sent += to_send;
        mbuf_remove(src_io, to_send);
    }
#ifdef DEBUG
    write_log("forward_body %p (ka=%d) -> %p sent %d of %d\n", src->nc,
            src->flags.keep_alive, dst->nc, src->body_sent, src->body_len);
#endif
}

static void forward(struct conn_data *conn, struct http_message *hm,
        struct peer *src_peer, struct peer *dst_peer) {
    struct mg_connection *src = src_peer->nc;
    struct mg_connection *dst = dst_peer->nc;
    struct mbuf *io = &src->recv_mbuf;
    int i;
    int is_request = (src_peer == &conn->client);
    src_peer->body_len = hm->body.len;
    struct http_backend *be = conn->be_conn->be;

    //printf("https=%d\n", conn->https);

    if (is_request) {
        /* Write rewritten request line. */
        size_t trim_len = strlen(be->uri_prefix);
        mg_printf(dst, "%.*s%s%.*s\r\n", (int) (hm->uri.p - io->buf), io->buf,
                be->uri_prefix_replacement,
                (int) (hm->proto.p + hm->proto.len - (hm->uri.p + trim_len)),
                hm->uri.p + trim_len);
    } else {
        /* Reply line goes without modification */
        mg_printf(dst, "%.*s %d %.*s\r\n", (int) hm->proto.len, hm->proto.p,
                (int) hm->resp_code, (int) hm->resp_status_msg.len,
                hm->resp_status_msg.p);
    }

    /* Headers. */
    for (i = 0; i < MG_MAX_HTTP_HEADERS && hm->header_names[i].len > 0; i++) {
        struct mg_str hn = hm->header_names[i];
        struct mg_str hv = hm->header_values[i];

        if (conn->https) {
#ifdef MG_ENABLE_SSL
            /*
             * If we terminate SSL and backend redirects to local HTTP port,
             * strip protocol to let client use HTTPS.
             * TODO(lsm): web page content may also contain local HTTP references,
             * they need to be rewritten too.
             */
            if (mg_vcasecmp(&hn, "Location") == 0) {
                size_t hlen = strlen(be->host_port);
                const char *hp = be->host_port, *p = memchr(hp, ':', hlen);

                if (p == NULL) {
                    p = hp + hlen;
                }

                if (mg_ncasecmp(hv.p, "http://", 7) == 0 &&
                        mg_ncasecmp(hv.p + 7, hp, (p - hp)) == 0) {
                    mg_printf(dst, "Location: %.*s\r\n", (int) (hv.len - (7 + (p - hp))),
                            hv.p + 7 + (p - hp));
                    continue;
                }
            }
#endif
        }

        /* We always rewrite the connection header depending on the settings. */
        if (mg_vcasecmp(&hn, "Connection") == 0) continue;

        /* Don't pass chunked transfer encoding to the client */
        if (mg_vcasecmp(&hn, "Transfer-encoding") == 0 &&
                mg_vcasecmp(&hv, "chunked") == 0) {
            continue;
        }

        mg_printf(dst, "%.*s: %.*s\r\n", (int) hn.len, hn.p, (int) hv.len, hv.p);
    }

    /* Emit the connection header. */
    const char *connection_mode = "close";
    if (dst_peer == &conn->backend) {
        if (s_backend_keepalive) connection_mode = "keep-alive";
    } else {
        if (conn->client.flags.keep_alive) connection_mode = "keep-alive";
    }
    mg_printf(dst, "Connection: %s\r\n", connection_mode);

    mg_printf(dst, "%s", "\r\n");

    mbuf_remove(io, hm->body.p - hm->message.p); /* We've forwarded headers */
    dst_peer->flags.headers_sent = 1;

    forward_body(src_peer, dst_peer);
}

struct be_conn *get_conn(struct http_backend *be) {
    if (STAILQ_EMPTY(&be->conns)) return NULL;
    struct be_conn *result = STAILQ_FIRST(&be->conns);
    STAILQ_REMOVE_HEAD(&be->conns, conns);
    be->num_conns--;
    return result;
}

/*
 * choose_backend parses incoming HTTP request and routes it to the appropriate
 * backend. It assumes that clients don't do HTTP pipelining, handling only
 * one request request for each connection. To give a hint to backend about
 * this it inserts "Connection: close" header into each forwarded request.
 */
static int connect_backend(struct conn_data *conn, struct http_message *hm) {
    struct mg_connection *nc = conn->client.nc;
    struct http_backend *be = choose_backend(hm);
    mg_event_handler_t http_handler = NULL;

    //write_log("%.*s %.*s backend=%s\n", (int) hm->method.len, hm->method.p,
    //        (int) hm->uri.len, hm->uri.p, be ? be->host_port : "not defined");

    if (be == NULL) return 0;
    if (be->redirect != 0) {
        mg_printf(nc, "HTTP/1.1 302 Found\r\nLocation: %s\r\n\r\n", be->host_port);
        return 1;
    }

    struct be_conn *bec = get_conn(be);
    if(conn->https) {
        http_handler = ev_handler_https;
    } else {
        http_handler = ev_handler_http;
    }

    if (bec != NULL) {
        bec->nc->handler = http_handler;

#ifdef DEBUG
        write_log("conn=%p to %p (%s) reusing bec=%p\n", conn, be, be->host_port,
                bec);
#endif
    } else {
        bec = malloc(sizeof(*conn->be_conn));
        memset(bec, 0, sizeof(*bec));

        bec->nc = mg_connect(nc->mgr, be->host_port, http_handler);
#ifdef DEBUG
        write_log("conn=%p new conn to %p (%s) bec=%p\n", conn, be, be->host_port,
                bec);
#endif
        if (bec->nc == NULL) {
            free(bec);
            write_log("Connection to [%s] failed\n", be->host_port);
            return 0;
        }
    }
    bec->be = be;
    conn->be_conn = bec;
    conn->backend.nc = bec->nc;
    conn->backend.nc->user_data = conn;
    mg_set_protocol_http_websocket(conn->backend.nc);
    return 1;
}

static int is_keep_alive(struct http_message *hm) {
    const struct mg_str *connection_header = mg_get_http_header(hm, "Connection");
    if (connection_header == NULL) {
        /* HTTP/1.1 connections are keep-alive by default. */
        if (mg_vcasecmp(&hm->proto, "HTTP/1.1") != 0) return 0;
    } else if (mg_vcasecmp(connection_header, "keep-alive") != 0) {
        return 0;
    }
    // We must also have Content-Length.
    return mg_get_http_header(hm, "Content-Length") != NULL;
}

static void idle_backend_handler(struct mg_connection *nc, int ev,
        void *ev_data) {
    (void) ev_data; /* Unused. */
    struct be_conn *bec = nc->user_data;
    const time_t now = time(NULL);
#ifdef DEBUG
    write_log("%d idle bec=%p nc=%p ev=%d deadline=%d\n", now, bec, nc, ev,
            bec->idle_deadline);
#endif
    switch (ev) {
        case MG_EV_POLL: {
                             if (bec->idle_deadline > 0 && now > bec->idle_deadline) {
#ifdef DEBUG
                                 write_log("bec=%p nc=%p closing due to idleness\n", bec, bec->nc);
#endif
                                 bec->nc->flags |= MG_F_CLOSE_IMMEDIATELY;
                             }
                             break;
                         }

        case MG_EV_CLOSE: {
#ifdef DEBUG
                              write_log("bec=%p closed\n", bec);
#endif
                              if (bec->idle_deadline > 0) {
                                  STAILQ_REMOVE(&bec->be->conns, bec, be_conn, conns);
                              }
                              free(bec);
                              break;
                          }
    }
}

void release_backend(struct conn_data *conn) {
    /* Disassociate the backend, put back on the pool. */
    struct be_conn *bec = conn->be_conn;
    conn->be_conn = NULL;
    if (bec->nc == NULL) {
        free(bec);
        memset(&conn->backend, 0, sizeof(conn->backend));
        return;
    }
    struct http_backend *be = bec->be;
    bec->nc->user_data = bec;
    bec->nc->handler = idle_backend_handler;
    if (conn->backend.flags.keep_alive) {
        bec->idle_deadline = time(NULL) + CONN_IDLE_TIMEOUT;
        STAILQ_INSERT_TAIL(&be->conns, bec, conns);
#ifdef DEBUG
        write_log("bec=%p becoming idle\n", bec);
#endif
        be->num_conns++;
        while (be->num_conns > MAX_IDLE_CONNS) {
            bec = STAILQ_FIRST(&be->conns);
            STAILQ_REMOVE_HEAD(&be->conns, conns);
            be->num_conns--;
            bec->idle_deadline = 0;
            bec->nc->flags = MG_F_CLOSE_IMMEDIATELY;
#ifdef DEBUG
            write_log("bec=%p evicted\n", bec);
#endif
        }
    } else {
        bec->idle_deadline = 0;
        bec->nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
    memset(&conn->backend, 0, sizeof(conn->backend));
}

static void ev_handler_http(struct mg_connection *nc, int ev, void *ev_data) {
    ev_handler(nc, ev, ev_data, 0);
}

static void ev_handler_https(struct mg_connection *nc, int ev, void *ev_data) {
    ev_handler(nc, ev, ev_data, 1);
}

static int print_json(dbclient* client, void* o, char* prefix, char* key, char* value) {
    struct dblist_ctx* ctx = (struct dblist_ctx*)o;

    if(ctx->index == 0) {
        mg_printf_http_chunk(ctx->nc, "\"%s\":\"%s\"\n", key, value);
    } else {
        mg_printf_http_chunk(ctx->nc, ",\"%s\":\"%s\"\n", key, value);
    }

    ctx->index++;
    return 0;
}

static int dblist_prefix(struct mg_connection *nc, dbclient* client, char* prefix) {
    struct dblist_ctx db_ctx;
    char *p = strtok(prefix, ",");
    int i = 0;

    db_ctx.nc = nc;

    mg_printf_http_chunk(nc, "{\"result\":[");

    while(p != NULL) {
        db_ctx.index = 0;
        if(i == 0) {
            mg_printf_http_chunk(nc, "{");
        } else {
            mg_printf_http_chunk(nc, ",{");
        }
        dbclient_list(client, prefix, &db_ctx, print_json);
        mg_printf_http_chunk(nc, "}\n");

        p = strtok(NULL, ",");
        i++;
    }

    mg_printf_http_chunk(nc, "]}");
    return 0;
}

static int process_json(struct conn_data* conn, struct http_message *hm) {
#define DST_LEN (510)
    int i, n, ok, dst_len = DST_LEN-9;
    struct json_token tokens[200] = {{0}};
    char *buf, dst[DST_LEN+50];
    struct json_token *method, *params, *fields;
    struct mg_connection *nc = conn->client.nc;
    dbclient client;

    /* Send headers keep-alive */
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n"
            "Content-Type: application/json\r\n\r\n");

    if(0 == mg_vcasecmp(&hm->method, "POST")) {
        n = parse_json(hm->body.p, hm->body.len, tokens, sizeof(tokens) / sizeof(tokens[0]));
        if (n <= 0) {
            return -1;
        }

        fields = find_json_token(tokens, "fields");
        if (fields != NULL && JSON_TYPE_OBJECT == fields[0].type && fields[0].num_desc > 0) {
            n = fields[0].num_desc/2;
            dbclient_start(&client);
            //printf("n=%d origin=%d\n", n, fields[0].num_desc);

            for(i = 0; i <= n; i++) {
                if(fields[i*2+1].len > 0 && fields[i*2+2].len > 0) {
                    //printf("o1=%d o2=%d\n", fields[i*2+1].len, fields[i*2+2].len);
                    dbclient_bulk(&client, "set", fields[i*2+1].ptr, fields[i*2+1].len, fields[i*2+2].ptr, fields[i*2+2].len);
                }
            }

            dbclient_end(&client);
        }

        dst[0] = '\0';
        buf = dst;
        ok = 1;
        if(method != NULL && JSON_TYPE_STRING == method[0].type && dst_len > method[0].len) {
            n = sprintf(buf, "%s/%s", "/jffs/softcenter/scripts", method[0].ptr);
            buf[n] = '\0';
            if(-1 == access(buf, X_OK)) {
                ok = 0;
            }

            buf[0] = '\0';
        } else {
            ok = 0;
        }

        fields = find_json_token(tokens, "id");
        method = find_json_token(tokens, "method");
        if(ok && fields != NULL && JSON_TYPE_NUMBER == fields[0].type && fields[0].len < 9) {
            n = method[0].len;
            memcpy(buf, method[0].ptr, n);
            buf[n] = ' ';
            buf[n+1] = '\0';
            buf += n+1;
            dst_len -= n+1;

            //for id
            n = fields[0].len;
            memcpy(buf, fields[0].ptr, n);
            buf[n] = ' ';
            buf[n+1] = '\0';
            buf += n+1;
            dst_len -= n+1;

            params = find_json_token(tokens, "params");
            if(params != NULL && JSON_TYPE_ARRAY == params[0].type && params[0].num_desc > 0) {
                for(i = 1; i <= params[0].num_desc; i++) {
                    if(dst_len < params[i].len) {
                        break;
                    }
                    n = params[i].len;

                    if(JSON_TYPE_NUMBER == params[i].type) {
                        memcpy(buf, params[i].ptr, n);
                        buf[n] = ' ';
                        buf[n+1] = '\0';
                        buf += n+1;
                        dst_len -= n+1;
                    }
                }
            }

            //TODO assess for check if xxx.sh is exists
            n = sprintf(buf, "%s", " > /dev/null 2>&1 &");
            buf[n] = '\0';

            n = strtol(fields[0].ptr, NULL, 10);
            //printf("set id=%d flen=%d dst=%s\n", n, fields[0].len, dst);
            system(dst);

            new_json_request(&sreq_mgr, nc, n, time(NULL));
            conn->id = n;
        } else {
            mg_printf_http_chunk(nc, "{ \"result\": %d }", 0);
            mg_send_http_chunk(nc, "", 0);
        }

        return 0;
    } else if(0 == mg_vcasecmp(&hm->method, "GET")) {
        dst[0] = '\0';
        if(dst_len < hm->uri.len || hm->uri.len <= PREFIX_API_LEN) {
            mg_printf_http_chunk(nc, "{ \"result\": %d }", -3);
            mg_send_http_chunk(nc, "", 0);
            return -3;
        }
        n = hm->uri.len - PREFIX_API_LEN;
        memcpy(dst, hm->uri.p + PREFIX_API_LEN, n);
        dst[n] = '\0';
        //printf("get dst=%s\n", dst);

        dbclient_start(&client);
        dblist_prefix(nc, &client, dst);
        dbclient_end(&client);

        mg_send_http_chunk(nc, "", 0);

        nc->flags |= MG_F_SEND_AND_CLOSE;
        conn->client.nc = NULL;

        return 0;
    }

    return -2;
}

static int process_resp(struct mg_connection *nc, struct http_message *hm, int* id) {
    char dst[64];
    int n;

    if(hm->uri.len > (8+PREFIX_RESP_LEN)) {
        return -5;
    }

    n = hm->uri.len - PREFIX_RESP_LEN;
    memcpy(dst, hm->uri.p + PREFIX_RESP_LEN, n);
    dst[n] = 0;

    n = strtol(dst, NULL, 10);

    *id = n;
    return set_json_response(&sreq_mgr, n, hm->body.p, hm->body.len);
}

static int process_result(struct mg_connection *nc, struct http_message *hm) {
    char dst[64];
    int n;

    if(hm->uri.len > (8+PREFIX_RESULT_LEN)) {
        return -5;
    }

    n = hm->uri.len - PREFIX_RESULT_LEN;
    memcpy(dst, hm->uri.p + PREFIX_RESULT_LEN, n);
    dst[n] = 0;

    n = strtol(dst, NULL, 10);

    mg_printf_http_chunk(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: application/json\r\n\r\n");
    if(0 != show_shell_resp(&sreq_mgr, n, nc)) {
        mg_printf_http_chunk(nc, "%d", -1);
    }
    mg_send_http_chunk(nc, "", 0);

    return 0;
}

static int check_path_exists(const char *url, int len) {
    int i;
    char path[256];
    static int root_len = 0;
    struct stat st = {0};

    if(0 == root_len) {
        root_len = strlen(s_http_server_opts.document_root);
        if('/' == s_http_server_opts.document_root[root_len]) {
            root_len -= 1;
        }
    }

    if((len + root_len) > 255 || len <= 0) {
        return 0;
    }

    for(i = len-1; i >= 0; i--) {
        if(url[i] == '.') {
            break;
        }
    }
    if (i < 0) {
        return 0;
    }

    memcpy(path, s_http_server_opts.document_root, root_len);
    memcpy(path+root_len, url, len);
    path[root_len+len] = '\0';

    if(0 == stat(path, &st) && S_ISREG(st.st_mode)) {
        return 1;
    }

    return 0;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, int https) {
    struct conn_data *conn = (struct conn_data *) nc->user_data;
    const time_t now = time(NULL);
    int result, id;

#ifdef DEBUG
    write_log("%d conn=%p nc=%p ev=%d ev_data=%p bec=%p bec_nc=%p\n", now, conn,
            nc, ev, ev_data, conn != NULL ? conn->be_conn : NULL,
            conn != NULL && conn->be_conn != NULL ? conn->be_conn->nc : NULL);
#endif

    if (conn == NULL) {
        if (ev == MG_EV_ACCEPT) {
            conn = calloc(1, sizeof(*conn));
            if (conn == NULL) {
                send_http_err(nc, s_error_500);
            } else {
                memset(conn, 0, sizeof(*conn));
                nc->user_data = conn;
                conn->client.nc = nc;
                conn->client.body_len = -1;
                conn->backend.body_len = -1;
                conn->last_activity = now;
                conn->https = https;
                conn->id = 0;
            }
            return;
        } else {
            if (ev != MG_EV_POLL) {
                nc->flags |= MG_F_CLOSE_IMMEDIATELY;
            }
            return;
        }
    }

    conn->https = https;

    if (ev != MG_EV_POLL) conn->last_activity = now;

    switch (ev) {
        case MG_EV_HTTP_REQUEST:
            { /* From client */
                assert(conn != NULL);
                assert(conn->be_conn == NULL);
                struct http_message *hm = (struct http_message *) ev_data;

                check_timeout(&sreq_mgr, time(NULL));

                if(hm != NULL && has_prefix(&hm->uri, PREFIX_API)) {
                    //printf("json connected\n");
                    //mg_printf(nc, "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n"
                    //          "Content-Type: application/json\r\n\r\n{}");

                    result = process_json(conn, hm);
                    //printf("result=%d\n", result);
                    if(result != 0) {
                        nc->flags |= MG_F_SEND_AND_CLOSE;
                        conn->client.nc = NULL;
                    }

                    break;
                } else if(hm != NULL && has_prefix(&hm->uri, PREFIX_ROOT)) {
                    //rewrite uri
                    hm->uri.p += PREFIX_ROOT_LEN-1;
                    hm->uri.len -= PREFIX_ROOT_LEN-1;
                    mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */

                    nc->flags |= MG_F_SEND_AND_CLOSE;
                    conn->client.nc = NULL;
                    break;
                } else if(hm != NULL && has_prefix(&hm->uri, PREFIX_TEMP)) {
                    //rewrite uri
                    hm->uri.p += PREFIX_TEMP_LEN-1;
                    hm->uri.len -= PREFIX_TEMP_LEN-1;
                    mg_serve_http(nc, hm, s_http_tmp_opts); /* Serve static content */

                    nc->flags |= MG_F_SEND_AND_CLOSE;
                    conn->client.nc = NULL;
                    break;
                } else if(hm != NULL && has_prefix(&hm->uri, PREFIX_RESP)) {

                    id = 0;
                    result = process_resp(nc, hm, &id);
                    mg_printf_http_chunk(nc, "%s{\"result\":%d}\n", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                            "Content-Type: application/json\r\n\r\n", result);
                    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
                    nc->flags |= MG_F_SEND_AND_CLOSE;
                    conn->client.nc = NULL;

                    if(id != 0) {
                        show_shell_resp(&sreq_mgr, id, NULL);
                    }

                    break;
                } else if(hm != NULL && has_prefix(&hm->uri, PREFIX_RESULT)) {

                    process_result(nc, hm);

                    nc->flags |= MG_F_SEND_AND_CLOSE;
                    conn->client.nc = NULL;
                    break;
                } else {
                    if(check_path_exists(hm->uri.p, hm->uri.len)) {
                        mg_serve_http(nc, hm, s_http_server_opts);

                        nc->flags |= MG_F_SEND_AND_CLOSE;
                        conn->client.nc = NULL;
                    } else {
                        conn->client.flags.keep_alive = is_keep_alive(hm);
                        if (!connect_backend(conn, hm)) {
                            respond_with_error(conn, s_error_500);
                            break;
                        }

                        if (conn->backend.nc == NULL) {
                            /* This is a redirect, we're done. */
                            conn->client.nc->flags |= MG_F_SEND_AND_CLOSE;
                            break;
                        }

                        forward(conn, hm, &conn->client, &conn->backend);
                    }
                    break;
                }
            }

        case MG_EV_CONNECT:
            { /* To backend */
                assert(conn != NULL);
                assert(conn->be_conn != NULL);
                int status = *(int *) ev_data;
                if (status != 0) {
                    write_log("Error connecting to %s: %d (%s)\n",
                            conn->be_conn->be->host_port, status, strerror(status));
                    /* TODO(lsm): mark backend as defunct, try it later on */
                    respond_with_error(conn, s_error_500);
                    conn->be_conn->nc = NULL;
                    release_backend(conn);
                    break;
                }
                break;
            }

        case MG_EV_HTTP_REPLY:
            { /* From backend */
                assert(conn != NULL);
                struct http_message *hm = (struct http_message *) ev_data;
                conn->backend.flags.keep_alive = s_backend_keepalive && is_keep_alive(hm);
                forward(conn, hm, &conn->backend, &conn->client);
                release_backend(conn);
                if (!conn->client.flags.keep_alive) {
                    conn->client.nc->flags |= MG_F_SEND_AND_CLOSE;
                } else {
#ifdef DEBUG
                    write_log("conn=%p remains open\n", conn);
#endif
                }
                break;
            }

        case MG_EV_POLL:
            {
                assert(conn != NULL);
                if (now - conn->last_activity > CONN_IDLE_TIMEOUT &&
                        conn->backend.nc == NULL /* not waiting for backend */) {
#ifdef DEBUG
                    write_log("conn=%p has been idle for too long\n", conn);
                    conn->client.nc->flags |= MG_F_SEND_AND_CLOSE;
#endif
                }
                break;
            }

        case MG_EV_CLOSE:
            {
                assert(conn != NULL);
                if(conn->client.nc == NULL) {
                    //json closed
                    //printf("json closed\n");
                } else if (nc == conn->client.nc) {
#ifdef DEBUG
                    write_log("conn=%p nc=%p client closed, body_sent=%d\n", conn, nc,
                            conn->backend.body_sent);
#endif
                    conn->client.nc = NULL;
                    if (conn->backend.nc != NULL) {
                        conn->backend.nc->flags |= MG_F_CLOSE_IMMEDIATELY;
                    }
                } else if (nc == conn->backend.nc) {
#ifdef DEBUG
                    write_log("conn=%p nc=%p backend closed\n", conn, nc);
#endif
                    conn->backend.nc = NULL;
                    if (conn->client.nc != NULL &&
                            (conn->backend.body_len < 0 ||
                             conn->backend.body_sent < conn->backend.body_len)) {
                        write_log("Backend %s disconnected.\n", conn->be_conn->be->host_port);
                        respond_with_error(conn, s_error_500);
                    }
                }

                if(conn->id != 0) {
                    delete_nc_by_id(&sreq_mgr, conn->id);
                }

                if (conn->client.nc == NULL && conn->backend.nc == NULL) {
                    //printf("free conn conn=0x%02x\n", conn);
                    free(conn);
                }

                break;
            }
    }
}

static void print_usage_and_exit(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s [-p http_port] [-s https_port] [-l log] [-r reverse_host]"
#if MG_ENABLE_SSL
            "[-c cert] "
#endif
            "[-w www_root]\nExample: ./bin/httpdb -p 8000 -s 8443 -r 10.1.1.1:80 -c ../tests/ssl.pem -w ../tests/web_root\n",
            prog_name);
    exit(EXIT_FAILURE);
}

const struct option long_options[] = {
    {"http_port", required_argument, 0, 'p'},
    {"https_port", required_argument, 0, 's'},
    {"cert", required_argument, 0, 'c'},
    {"reverse", required_argument, 0, 'r'},
    {"www", required_argument, 0, 'w'},
    {"log", required_argument, 0, 'l'},
    {0, 0, 0, 0}
};

int main(int argc, char *argv[]) {
    struct mg_mgr mgr;
    struct mg_connection *nc_http = NULL, *nc_https = NULL;
    struct http_backend *be;
    char http_port[64], https_port[64], www[128], reverse[128];
    char *vhost = NULL, *cert = NULL, *log = NULL;
    int c = 0;//IMPORTANT use int

    mg_mgr_init(&mgr, NULL);

    s_backend_keepalive = 1;
    s_log_file = stdout;
    vhost = NULL;

    //cert = "../tests/ssl.pem";
    //s_http_server_opts.document_root = "../tests/web_root";
    //s_http_server_opts.enable_directory_listing = "no";
    //s_http_server_opts.url_rewrites = "/_root=/web_root";

    s_http_tmp_opts.document_root = "/tmp/info";
    s_http_tmp_opts.enable_directory_listing = "no";

    http_port[0] = '\0';
    https_port[0] = '\0';
    www[0] = '\0';
    reverse[0] = '\0';

    while (c >= 0) {
        c = getopt_long(argc, argv, "p:s:c:r:w:l:h", long_options, NULL);
        switch(c) {
            case 'p':
                strncpy(http_port, optarg, 63);
                break;
            case 's':
                strncpy(https_port, optarg, 63);
                break;
            case 'c':
                cert = optarg;
                break;
            case 'r':
                strncpy(reverse, optarg, 127);
            case 'w':
                strncpy(www, optarg, 127);
                break;
            case 'l':
                log = optarg;
                break;
            case 'h':
                print_usage_and_exit(argv[0]);
                break;
            default:
                // Bug in netgear. c === 0xFF
                printf("got c=%x\n", c);
                c = -1;
                break;
        }
    }

    s_http_server_opts.document_root = www;

    be =
        vhost != NULL ? &s_vhost_backends[s_num_vhost_backends++]
        : &s_default_backends[s_num_default_backends++];
    STAILQ_INIT(&be->conns);

    be->vhost = vhost;
    be->uri_prefix = "/";
    be->host_port = reverse;
    be->redirect = 0;
    be->uri_prefix_replacement = be->uri_prefix;

    /* if ((r = strchr(be->uri_prefix, '=')) != NULL) {
     *r = '\0';
     be->uri_prefix_replacement = r + 1;
     } */

    printf(
            "Adding backend for %s%s : %s "
            "[redirect=%d,prefix_replacement=%s]\n",
            be->vhost == NULL ? "" : be->vhost, be->uri_prefix, be->host_port,
            be->redirect, be->uri_prefix_replacement);

    init_req_mgr(&sreq_mgr);

    if (strlen(http_port) > 0) {
        if ((nc_http = mg_bind(&mgr, http_port, ev_handler_http)) == NULL) {
            fprintf(stderr, "mg_bind(%s) failed\n", http_port);
            exit(EXIT_FAILURE);
        }
    }

    if (strlen(https_port) > 0) {
        if ((nc_https = mg_bind(&mgr, https_port, ev_handler_https)) == NULL) {
            fprintf(stderr, "mg_bind(%s) failed\n", https_port);
            exit(EXIT_FAILURE);
        }
    }

#if MG_ENABLE_SSL
    if (cert != NULL && nc_https != NULL) {
        const char *err_str = mg_set_ssl(nc_https, cert, NULL);
        if (err_str != NULL) {
            fprintf(stderr, "Error loading SSL cert: %s\n", err_str);
            exit(1);
        }
    }
#endif

    if (s_num_vhost_backends + s_num_default_backends == 0) {
        print_usage_and_exit(argv[0]);
    }

    if(NULL == nc_http && NULL == nc_https) {
        fprintf(stderr,  "not http or https found\n");
        exit(1);
    }

    if(nc_http != NULL) {
        mg_set_protocol_http_websocket(nc_http);
    }

    if(nc_https != NULL) {
        mg_set_protocol_http_websocket(nc_https);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Run event loop until signal is received */
    printf("Starting http on port %s\nhttps on port %s\n", http_port, https_port);
    while (s_sig_num == 0) {
        mg_mgr_poll(&mgr, 1000);
    }

    /* Cleanup */
    mg_mgr_free(&mgr);

    printf("Exiting on signal %d\n", s_sig_num);

    return EXIT_SUCCESS;
}

