/*
 * http.h
 */


#ifndef HTTP_H
#define HTTP_H

#include "extractor.h"

struct http_headers : public datum {
    bool complete;

    http_headers() : datum{}, complete{false} {}

    void parse(struct datum &p) {
        unsigned char crlf[2] = { '\r', '\n' };

        data = p.data;
        while (parser_get_data_length(&p) > 0) {
            if (parser_match(&p, crlf, sizeof(crlf), NULL) == status_ok) {
                complete = true;
                break;  /* at end of headers */
            }
            if (parser_skip_upto_delim(&p, crlf, sizeof(crlf)) == status_err) {
                break;
            }
        }
        data_end = p.data;
    }

    void print_host(struct json_object &o, const char *key) const;
    void print_matching_name(struct json_object &o, const char *key, struct datum &name) const;
    void print_matching_names(struct json_object &o, const char *key, std::list<struct datum> &name) const;
    void print_matching_names(struct json_object &o, std::list<std::pair<struct datum, std::string>> &name_list) const;
    void print_matching_names(struct json_object &o, std::unordered_map<std::basic_string<uint8_t>, std::string> &name_dict) const;

    void fingerprint(struct buffer_stream &buf, std::unordered_map<std::basic_string<uint8_t>, bool> &name_dict) const;

};

struct http_request {
    struct datum method;
    struct datum uri;
    struct datum protocol;
    struct http_headers headers;

    http_request() : method{NULL, NULL}, uri{NULL, NULL}, protocol{NULL, NULL}, headers{} {}

    void parse(struct datum &p);

    bool is_not_empty() const { return uri.is_not_empty(); }

    void write_json(struct json_object &record, bool output_metadata);

    void operator()(struct buffer_stream &b) const;

};

struct http_response {
    struct datum version;
    struct datum status_code;
    struct datum status_reason;
    struct http_headers headers;

    http_response() : version{NULL, NULL}, status_code{NULL, NULL}, status_reason{NULL, NULL}, headers{} {}

    void parse(struct datum &p);

    bool is_not_empty() const { return status_code.is_not_empty(); }

    void write_json(struct json_object &record);

    void operator()(struct buffer_stream &buf) const;

};

#endif /* HTTP_H */
