/* The former DNS answer code of components/dns_server/dns_server.c (the
 * ESP-IDF captive portal example, verbatim from d8d32926: definitions,
 * parse_dns_name, parse_dns_request) on host shims. */
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ESP_LOGD(tag, ...) ((void) (tag))
#define ESP_LOGE(tag, ...) ((void) (tag))
#define printf(...) ((void) 0)
#define IPADDR_ANY 0
typedef void *TaskHandle_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip; } esp_netif_ip_info_t;
typedef struct dns_entry_pair { const char *name; const char *if_key; esp_ip4_addr_t ip; } dns_entry_pair_t;
typedef struct dns_server_handle *dns_server_handle_t;
static uint32_t g_ip;
static void *esp_netif_get_handle_from_ifkey(const char *k) { return (void *) k; }
static int esp_netif_get_ip_info(void *n, esp_netif_ip_info_t *i) { (void) n; i->ip.addr = g_ip; return 0; }

#define DNS_PORT (53)
#define DNS_MAX_LEN (256)

#define OPCODE_MASK (0x7800)
#define QR_FLAG (1 << 7)
#define QD_TYPE_A (0x0001)
#define ANS_TTL_SEC (300)

static const char *TAG = "example_dns_redirect_server";

// DNS Header Packet
typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

// DNS Question Packet
typedef struct
{
    uint16_t type;
    uint16_t class;
} dns_question_t;

// DNS Answer Packet
typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;

// DNS server handle
struct dns_server_handle
{
    bool started;
    TaskHandle_t task;
    int num_of_entries;
    dns_entry_pair_t entry[];
};

/*
    Parse the name from the packet from the DNS name format to a regular .-seperated name
    returns the pointer to the next part of the packet
*/
static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{

    char *label = raw_name;
    char *name_itr = parsed_name;
    int name_len = 0;

    do {
        int sub_name_len = *label;
        // (len + 1) since we are adding  a '.'
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len) {
            return NULL;
        }

        // Copy the sub name that follows the the label
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    // Terminate the final string, replacing the last '.'
    parsed_name[name_len - 1] = '\0';
    // Return pointer to first char after the name
    return label + 1;
}

// Parses the DNS request and prepares a DNS response with the IP of the softAP
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len, dns_server_handle_t h)
{
    ESP_LOGD(TAG, "TEST");
    if (req_len > dns_reply_max_len) {
        return -1;
    }

    // Prepare the reply
    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    // Endianess of NW packet different from chip
    dns_header_t *header = (dns_header_t *) dns_reply;
    ESP_LOGD(TAG, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d", ntohs(header->id), ntohs(header->flags),
             ntohs(header->qd_count));

    // Not a standard query
    if ((header->flags & OPCODE_MASK) != 0) {
        return 0;
    }

    // Set question response flag
    header->flags |= QR_FLAG;

    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len) {
        return -1;
    }

    // Pointer to current answer and question
    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    // Respond to all questions based on configured rules
    for (int qd_i = 0; qd_i < qd_count; qd_i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        dns_question_t *question = (dns_question_t *) (name_end_ptr);
        uint16_t qd_type = ntohs(question->type);
        uint16_t qd_class = ntohs(question->class);

        ESP_LOGD(TAG, "Received type: %d | Class: %d | Question for: %s", qd_type, qd_class, name);

        if (qd_type == QD_TYPE_A) {
            esp_ip4_addr_t ip = {.addr = IPADDR_ANY};
            // Check the configured rules to decide whether to answer this question or not
            for (int i = 0; i < h->num_of_entries; ++i) {
                // check if the name either corresponds to the entry, or if we should answer to all queries ("*")
                if (strcmp(h->entry[i].name, "*") == 0 || strcmp(h->entry[i].name, name) == 0) {
                    if (h->entry[i].if_key) {

                        esp_netif_ip_info_t ip_info;
                        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey(h->entry[i].if_key), &ip_info);
                        ip.addr = ip_info.ip.addr;
                        ESP_LOGD(TAG, "TEST %s", h->entry[i].if_key);
                        break;
                    } else if (h->entry->ip.addr != IPADDR_ANY) {
                        ip.addr = h->entry[i].ip.addr;
                        break;
                    }
                }
            }
            uint8_t octet1 = (ip.addr >> 24) & 0xFF;
            uint8_t octet2 = (ip.addr >> 16) & 0xFF;
            uint8_t octet3 = (ip.addr >> 8) & 0xFF;
            uint8_t octet4 = ip.addr & 0xFF;

            printf("IP address: %u.%u.%u.%u\n", octet1, octet2, octet3, octet4);

            if (ip.addr == IPADDR_ANY) { // no rule applies, continue with another question
                continue;
            }
            dns_answer_t *answer = (dns_answer_t *) cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            ESP_LOGD(TAG, "Answer with PTR offset: 0x%" PRIX16 " and IP 0x%" PRIX32, ntohs(answer->ptr_offset), ip.addr);

            answer->addr_len = htons(sizeof(ip.addr));
            answer->ip_addr = ip.addr;
        }
    }
    return reply_len;
}


/* the reply with the former rule of the firmware: "*" -> the AP address */
size_t ref_dns_reply(const uint8_t *req, size_t len, const uint8_t *ip, uint8_t *out, size_t out_len)
{
    static struct {
        struct dns_server_handle h;
        dns_entry_pair_t e[1];
    } s;
    s.h.num_of_entries = 1;
    s.h.entry[0].name = "*";
    s.h.entry[0].if_key = "WIFI_AP_DEF";
    memcpy(&g_ip, ip, 4);
    /* the task's receive buffer: 127 bytes plus a terminator */
    char rx[128];
    if (len > sizeof(rx) - 1) {
        return 0;
    }
    memcpy(rx, req, len);
    rx[len] = 0;
    int n = parse_dns_request(rx, len, (char *) out, out_len, &s.h);
    return n > 0 ? (size_t) n : 0;
}
