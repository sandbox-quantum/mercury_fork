// mysql.hpp
//
// an implementation of protocol parsing for MySQL server greeting and client login

#ifndef MYSQL_HPP
#define MYSQL_HPP

#include "json_object.h"
#include "match.h"

// Reference : https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_PROTOCOL.html
// https://mariadb.com/kb/en/clientserver-protocol/

namespace mysql_consts{

    // Options and extended options are part of the server/client capabilities and extended capabilities ( supported in >= ver 4.1 )
    // Refer to https://dev.mysql.com/doc/dev/mysql-server/latest/mysql__com_8h_source.html

    static const char* const options[] = {
        "LONG_PASSWORD",
        "FOUND_ROWS",
        "LONG_FLAG",
        "CONNECT_WITH_DB",
        "NO_SCHEMA",
        "COMPRESS",
        "ODBC",
        "LOCAL_FILES",
        "IGNORE_SPACE",
        "PROTOCOL_41",
        "INTERACTIVE",
        "SSL",
        "IGNORE_SIGPIPE",
        "TRANSACTIONS",
        "RESERVED",
        "SECURE_CONNECTION"
    };

    static const char* const extended_options[] = {
        "MULTI_STATEMENTS",
        "MULTI_RESULTS",
        "PS_MULTI_RESULTS",
        "PLUGIN_AUTH",
        "CONNECT_ATTRS",
        "PLUGIN_AUTH_LENENC_CLIENT_DATA",
        "CAN_HANDLE_EXPIRED_PASSWORD",
        "SESSION_TRACK",
        "DEPRECATE_EOF",
        "OPTIONAL_RESULTSET_METADATA",
        "ZSTD_COMPRESSION_ALGORITHM",
        "QUERY_ATTRIBUTES",
        "MULTI_FACTOR_AUTHENTICATION",
        "CAPABILITY_EXTENSION",
        "SSL_VERIFY_SERVER_CERT",
        "REMEMBER_OPTIONS"
    };    

    struct capabilities {
        uint16_t val;

        capabilities (uint16_t cap) : val{cap} {}

        capabilities (datum &pkt) : val{encoded<uint16_t>{pkt,true}.value()} {}

        uint16_t operator () () { return val;}

        void write_json(struct json_object &record, bool output_metadata) {
            record.print_key_hex("capabilities_value", (uint8_t*)(&val), 2);

            if (output_metadata) {
                json_array cap_str{record, "capabilities_str"};
                for (size_t i = 0; i< 16; i++) {
                    if (val & (1UL << i)) {
                        cap_str.print_string(options[i]);
                    }
                }
                cap_str.close();
            }
        }
    };

    struct extended_capabilities {
        uint16_t ext_val;

        extended_capabilities (uint16_t ext_cap) : ext_val{ext_cap} {}

        extended_capabilities (datum &pkt) : ext_val{encoded<uint16_t>{pkt,true}.value()} {}

        void write_json(struct json_object &record, bool output_metadata) {
            record.print_key_uint16_hex("extended_capabilities_value", ext_val);

            if (output_metadata) {
                json_array ext_cap_str{record, "ext_capabilities_str"};
                for (size_t i = 0; i< 16; i++) {
                    if (ext_val & (1UL << i)) {
                        ext_cap_str.print_string(extended_options[i]);
                    }
                }
                ext_cap_str.close();
            }
        }
    };

    // MariaDB collation list ref: https://mariadb.com/kb/en/supported-character-sets-and-collations/
    // Collation list generated by running following query on an online MySQL server (8.0) - https://onecompiler.com/mysql or MariaDB server (>5.x)- https://onecompiler.com/mariadb
    // "SELECT CONCAT('"', COLLATION_NAME, '",') FROM information_schema.COLLATIONS ORDER BY ID"
    // The list has been taken from MariaDB as it is superset for MySQL collations

    static const char* const mysql_collations[] = {
        "big5_chinese_ci",
        "latin2_czech_cs",
        "dec8_swedish_ci",
        "cp850_general_ci",
        "latin1_german1_ci",
        "hp8_english_ci",
        "koi8r_general_ci",
        "latin1_swedish_ci",
        "latin2_general_ci",
        "swe7_swedish_ci",
        "ascii_general_ci",
        "ujis_japanese_ci",
        "sjis_japanese_ci",
        "cp1251_bulgarian_ci",
        "latin1_danish_ci",
        "hebrew_general_ci",
        "tis620_thai_ci",
        "euckr_korean_ci",
        "latin7_estonian_cs",
        "latin2_hungarian_ci",
        "koi8u_general_ci",
        "cp1251_ukrainian_ci",
        "gb2312_chinese_ci",
        "greek_general_ci",
        "cp1250_general_ci",
        "latin2_croatian_ci",
        "gbk_chinese_ci",
        "cp1257_lithuanian_ci",
        "latin5_turkish_ci",
        "latin1_german2_ci",
        "armscii8_general_ci",
        "utf8_general_ci",
        "cp1250_czech_cs",
        "ucs2_general_ci",
        "cp866_general_ci",
        "keybcs2_general_ci",
        "macce_general_ci",
        "macroman_general_ci",
        "cp852_general_ci",
        "latin7_general_ci",
        "latin7_general_cs",
        "macce_bin",
        "cp1250_croatian_ci",
        "utf8mb4_general_ci",
        "utf8mb4_bin",
        "latin1_bin",
        "latin1_general_ci",
        "latin1_general_cs",
        "cp1251_bin",
        "cp1251_general_ci",
        "cp1251_general_cs",
        "macroman_bin",
        "utf16_general_ci",
        "utf16_bin",
        "utf16le_general_ci",
        "cp1256_general_ci",
        "cp1257_bin",
        "cp1257_general_ci",
        "utf32_general_ci",
        "utf32_bin",
        "utf16le_bin",
        "binary",
        "armscii8_bin",
        "ascii_bin",
        "cp1250_bin",
        "cp1256_bin",
        "cp866_bin",
        "dec8_bin",
        "greek_bin",
        "hebrew_bin",
        "hp8_bin",
        "keybcs2_bin",
        "koi8r_bin",
        "koi8u_bin",
        "utf8_tolower_ci",
        "latin2_bin",
        "latin5_bin",
        "latin7_bin",
        "cp850_bin",
        "cp852_bin",
        "swe7_bin",
        "utf8_bin",
        "big5_bin",
        "euckr_bin",
        "gb2312_bin",
        "gbk_bin",
        "sjis_bin",
        "tis620_bin",
        "ucs2_bin",
        "ujis_bin",
        "geostd8_general_ci",
        "geostd8_bin",
        "latin1_spanish_ci",
        "cp932_japanese_ci",
        "cp932_bin",
        "eucjpms_japanese_ci",
        "eucjpms_bin",
        "cp1250_polish_ci",
        "utf16_unicode_ci",
        "utf16_icelandic_ci",
        "utf16_latvian_ci",
        "utf16_romanian_ci",
        "utf16_slovenian_ci",
        "utf16_polish_ci",
        "utf16_estonian_ci",
        "utf16_spanish_ci",
        "utf16_swedish_ci",
        "utf16_turkish_ci",
        "utf16_czech_ci",
        "utf16_danish_ci",
        "utf16_lithuanian_ci",
        "utf16_slovak_ci",
        "utf16_spanish2_ci",
        "utf16_roman_ci",
        "utf16_persian_ci",
        "utf16_esperanto_ci",
        "utf16_hungarian_ci",
        "utf16_sinhala_ci",
        "utf16_german2_ci",
        "utf16_croatian_ci",
        "utf16_unicode_520_ci",
        "utf16_vietnamese_ci",
        "ucs2_unicode_ci",
        "ucs2_icelandic_ci",
        "ucs2_latvian_ci",
        "ucs2_romanian_ci",
        "ucs2_slovenian_ci",
        "ucs2_polish_ci",
        "ucs2_estonian_ci",
        "ucs2_spanish_ci",
        "ucs2_swedish_ci",
        "ucs2_turkish_ci",
        "ucs2_czech_ci",
        "ucs2_danish_ci",
        "ucs2_lithuanian_ci",
        "ucs2_slovak_ci",
        "ucs2_spanish2_ci",
        "ucs2_roman_ci",
        "ucs2_persian_ci",
        "ucs2_esperanto_ci",
        "ucs2_hungarian_ci",
        "ucs2_sinhala_ci",
        "ucs2_german2_ci",
        "ucs2_croatian_ci",
        "ucs2_unicode_520_ci",
        "ucs2_vietnamese_ci",
        "ucs2_general_mysql500_ci",
        "utf32_unicode_ci",
        "utf32_icelandic_ci",
        "utf32_latvian_ci",
        "utf32_romanian_ci",
        "utf32_slovenian_ci",
        "utf32_polish_ci",
        "utf32_estonian_ci",
        "utf32_spanish_ci",
        "utf32_swedish_ci",
        "utf32_turkish_ci",
        "utf32_czech_ci",
        "utf32_danish_ci",
        "utf32_lithuanian_ci",
        "utf32_slovak_ci",
        "utf32_spanish2_ci",
        "utf32_roman_ci",
        "utf32_persian_ci",
        "utf32_esperanto_ci",
        "utf32_hungarian_ci",
        "utf32_sinhala_ci",
        "utf32_german2_ci",
        "utf32_croatian_ci",
        "utf32_unicode_520_ci",
        "utf32_vietnamese_ci",
        "utf8_unicode_ci",
        "utf8_icelandic_ci",
        "utf8_latvian_ci",
        "utf8_romanian_ci",
        "utf8_slovenian_ci",
        "utf8_polish_ci",
        "utf8_estonian_ci",
        "utf8_spanish_ci",
        "utf8_swedish_ci",
        "utf8_turkish_ci",
        "utf8_czech_ci",
        "utf8_danish_ci",
        "utf8_lithuanian_ci",
        "utf8_slovak_ci",
        "utf8_spanish2_ci",
        "utf8_roman_ci",
        "utf8_persian_ci",
        "utf8_esperanto_ci",
        "utf8_hungarian_ci",
        "utf8_sinhala_ci",
        "utf8_german2_ci",
        "utf8_croatian_ci",
        "utf8_unicode_520_ci",
        "utf8_vietnamese_ci",
        "utf8_general_mysql500_ci",
        "utf8mb4_unicode_ci",
        "utf8mb4_icelandic_ci",
        "utf8mb4_latvian_ci",
        "utf8mb4_romanian_ci",
        "utf8mb4_slovenian_ci",
        "utf8mb4_polish_ci",
        "utf8mb4_estonian_ci",
        "utf8mb4_spanish_ci",
        "utf8mb4_swedish_ci",
        "utf8mb4_turkish_ci",
        "utf8mb4_czech_ci",
        "utf8mb4_danish_ci",
        "utf8mb4_lithuanian_ci",
        "utf8mb4_slovak_ci",
        "utf8mb4_spanish2_ci",
        "utf8mb4_roman_ci",
        "utf8mb4_persian_ci",
        "utf8mb4_esperanto_ci",
        "utf8mb4_hungarian_ci",
        "utf8mb4_sinhala_ci",
        "utf8mb4_german2_ci",
        "utf8mb4_croatian_ci",
        "utf8mb4_unicode_520_ci",
        "utf8mb4_vietnamese_ci",
        "gb18030_chinese_ci",
        "gb18030_bin",
        "gb18030_unicode_520_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_de_pb_0900_ai_ci",
        "utf8mb4_is_0900_ai_ci",
        "utf8mb4_lv_0900_ai_ci",
        "utf8mb4_ro_0900_ai_ci",
        "utf8mb4_sl_0900_ai_ci",
        "utf8mb4_pl_0900_ai_ci",
        "utf8mb4_et_0900_ai_ci",
        "utf8mb4_es_0900_ai_ci",
        "utf8mb4_sv_0900_ai_ci",
        "utf8mb4_tr_0900_ai_ci",
        "utf8mb4_cs_0900_ai_ci",
        "utf8mb4_da_0900_ai_ci",
        "utf8mb4_lt_0900_ai_ci",
        "utf8mb4_sk_0900_ai_ci",
        "utf8mb4_es_trad_0900_ai_ci",
        "utf8mb4_la_0900_ai_ci",
        "utf8mb4_eo_0900_ai_ci",
        "utf8mb4_hu_0900_ai_ci",
        "utf8mb4_hr_0900_ai_ci",
        "utf8mb4_vi_0900_ai_ci",
        "utf8mb4_0900_as_cs",
        "utf8mb4_de_pb_0900_as_cs",
        "utf8mb4_is_0900_as_cs",
        "utf8mb4_lv_0900_as_cs",
        "utf8mb4_ro_0900_as_cs",
        "utf8mb4_sl_0900_as_cs",
        "utf8mb4_pl_0900_as_cs",
        "utf8mb4_et_0900_as_cs",
        "utf8mb4_es_0900_as_cs",
        "utf8mb4_sv_0900_as_cs",
        "utf8mb4_tr_0900_as_cs",
        "utf8mb4_cs_0900_as_cs",
        "utf8mb4_da_0900_as_cs",
        "utf8mb4_lt_0900_as_cs",
        "utf8mb4_sk_0900_as_cs",
        "utf8mb4_es_trad_0900_as_cs",
        "utf8mb4_la_0900_as_cs",
        "utf8mb4_eo_0900_as_cs",
        "utf8mb4_hu_0900_as_cs",
        "utf8mb4_hr_0900_as_cs",
        "utf8mb4_vi_0900_as_cs",
        "utf8mb4_ja_0900_as_cs",
        "utf8mb4_ja_0900_as_cs_ks",
        "utf8mb4_0900_as_ci",
        "utf8mb4_ru_0900_ai_ci",
        "utf8mb4_ru_0900_as_cs",
        "utf8mb4_zh_0900_as_cs",
        "utf8mb4_0900_bin"
    };

    // For server_status refer to https://dev.mysql.com/doc/dev/mysql-server/latest/mysql__com_8h_source.html
    static const char* const server_status_str[] = {
        "STATUS_IN_TRANS",
        "STATUS_AUTOCOMMIT",
        "MORE_RESULTS_EXISTS",
        "QUERY_NO_GOOD_INDEX_USED",
        "QUERY_NO_INDEX_USED",
        "STATUS_CURSOR_EXISTS",
        "STATUS_LAST_ROW_SENT",
        "STATUS_DB_DROPPED",
        "STATUS_NO_BACKSLASH_ESCAPES",
        "STATUS_METADATA_CHANGED",
        "QUERY_WAS_SLOW",
        "PS_OUT_PARAMS",
        "STATUS_IN_TRANS_READONLY",
        "SESSION_STATE_CHANGED",
        "RESERVED"
    };

    struct server_status {
        uint16_t status;

        server_status(datum &pkt) : status{encoded<uint16_t>{pkt,true}.value()} {}

        void write_json(struct json_object &record, bool output_metadata) {
            record.print_key_uint16_hex("server_status_value", status);

            if (output_metadata) {
                json_array status_str{record, "server_status_str"};
                for (size_t i = 0; i< 16; i++) {
                    if (status & (1UL << i)) {
                        status_str.print_string(server_status_str[i]);
                    }
                }
                status_str.close();
            }
        }        
    };

};

class mysql_server_greet : public base_protocol {
    uint32_t len;    // 3 bytes in little endian
    encoded<uint8_t> pkt_num;
    encoded<uint8_t> proto;    // fixed 0x0A
    datum version{};
    encoded<uint32_t> thread_id;
    datum salt_1;
    mysql_consts::capabilities cap;
    encoded<uint8_t> collation;
    mysql_consts::server_status srv_status;
    mysql_consts::extended_capabilities ext_cap;
    encoded<uint8_t> auth_plugin_len;
    bool if_auth_plugin;
    bool is_ver_less_41;
    bool is_mariadb;
    bool partial_salt;
    datum salt_2{};
    bool valid = true;
    uint32_t mariadb_ext_cap = 0;
    datum auth_plugin;

public:

    mysql_server_greet (datum &pkt) :
        len{ (encoded<uint8_t>{pkt}.value()) + (encoded<uint8_t>{pkt}.value() << 8) + (encoded<uint8_t>{pkt}.value() << 16) },
        pkt_num{pkt},
        proto{pkt},
        version{ [&]() -> datum {
                                    int off = pkt.find_delim(0x00);
                                    if (off < 0) {
                                        return datum{NULL,NULL};
                                    }
                                    const uint8_t* begin = pkt.data;
                                    const uint8_t* end  = pkt.data + off + 1;
                                    pkt.skip(off + 1);
                                    return datum{begin,end};
                                 } () },
        thread_id{pkt,true},
        salt_1{pkt, 9},
        cap{pkt},
        collation{pkt},
        srv_status{pkt},
        ext_cap{pkt},
        auth_plugin_len{pkt}
        {
            if (version.length() < 6 || *(version.data_end-1) != 0x00) {
                valid = false;
                return;
            }
            if_auth_plugin = (auth_plugin_len > 0);
            uint8_t maj_ver = *(version.data);
            uint8_t min_ver = *(version.data + 2);
            is_ver_less_41 =  (maj_ver < '4') || (maj_ver == '4' && min_ver < '1');
            if ( maj_ver < '5' && if_auth_plugin) {
                valid = false;
                return;
            }
            partial_salt = !is_ver_less_41;
            is_mariadb = ( version.length() > 9 || !(cap()&1) );
            if (is_mariadb) {
                pkt.skip(6);
                mariadb_ext_cap = encoded<uint32_t>{pkt}.value();
            }
            else {
                pkt.skip(10);
            }
            if (partial_salt) {
                salt_2.parse(pkt,13);
                if ( salt_2.length() !=13 || *(salt_2.data_end-1) != 0x00) {
                    valid = false;
                    return;
                }
            }
            if (if_auth_plugin) {
                auth_plugin.parse(pkt, auth_plugin_len+1);    // +1 for null terminated string
            }
            if (pkt.length()) {
                valid = false;
                return;
            }
        }

        bool is_not_empty() { return valid; }

        void write_json(struct json_object &record, bool output_metadata) {
            if (!valid) {
                return;
            }
            json_object mysql_json(record, "mysql_server");
            mysql_json.print_key_json_string("version",version);
            if (output_metadata) {
                mysql_json.print_key_int("pkt_num",pkt_num);
            }
            if (!partial_salt) {
                datum salt = salt_1;
                salt.trim(1);
                mysql_json.print_key_json_string("salt",salt);   
            }
            else {
                data_buffer<32> salt;
                salt.copy(salt_1.data,salt_1.length()-1);
                salt.copy(salt_2.data,salt_2.length()-1);
                mysql_json.print_key_json_string("salt",salt.contents());
            }
            cap.write_json(mysql_json,output_metadata);
            mysql_json.print_key_string("collation",mysql_consts::mysql_collations[collation.value()-1]);
            srv_status.write_json(mysql_json, output_metadata);
            ext_cap.write_json(mysql_json, output_metadata);
            if (auth_plugin_len) {
                mysql_json.print_key_int("auth_plugin_len",auth_plugin_len);
                mysql_json.print_key_json_string("auth_plugin",auth_plugin);
            }
            if (is_mariadb) {
                mysql_json.print_key_bool("mariadb",true);
                mysql_json.print_key_int("mariadb_extended",mariadb_ext_cap);
            }
            else {
                mysql_json.print_key_bool("mariadb",false);
            }

            mysql_json.close();
        }

        static constexpr mask_value_and_offset<8> matcher {
        {0xF8, 0xFF, 0xF0, 0xFF, 0xF0, 0xE0, 0xE0, 0x00},
        {0x00, 0x0A, 0x30, 0x2E, 0x30, 0x20, 0x20, 0x00},
        3,      // skip 3 bytes from start
    };

};




#endif  // MYSQL_HPP
