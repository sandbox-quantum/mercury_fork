// tofsee.hpp
//
// initial message de-obfuscation and parsing

#ifndef TOFSEE_HPP
#define TOFSEE_HPP

#include "datum.h"
#include "json_object.h"
#include "utils.h"

template <size_t bits, typename T>
inline T rotl(T &x) {
    return (x << bits )|(x >> (sizeof(T)*8 - bits));
}

// Tofsee is a malware family that utilizes custom encryption and
// obfuscation in order to evade detection.  This implementation
// follows the description provided by CERT Polska
// https://cert.pl/en/posts/2016/09/tofsee-en/, and correctly parsed
// packets generated by malware samples in May 2023.
//
// The tofsee initial message is 200 bytes in length, and is encrypted
// with a fixed key.  This implementation decrypts the ciphertext of
// that message into a tofsee_initial_message::plaintext, using the
// function tofsee_initial_message::decrypt(), and then parses the
// initial message from that buffer.  The plaintext has this
// structure:
//
//    struct greeting{
//        uint8_t key[128];
//        uint8_t unk1[16];
//        uint32_t bot_IP;
//        uint32_t srv_time;
//        uint8_t unk2[48];
//    };
//
// This class does not define a matcher for the initial bytes of a
// message.  Instead, it is run on initial TCP messages that are
// exactly 200 bytes in length, and are in the server-to-client
// direction.  In the is_not_empty() function, actual Tofsee messages
// are distinguished by the low Hamming weight of the unknown_1 field
// after decryption.  If it were generated uniformly at random, the
// expected weight of that field would be 64, but in traffic
// observations, the weight was no greater than seven.
//
class tofsee_initial_message {

    static void decrypt(const uint8_t *ciphertext, uint8_t *plaintext, size_t data_len) {
        uint8_t res = 198;
        for (size_t i=0; i<data_len; i++) {
            uint8_t c = *ciphertext++;
            *plaintext++ = res ^ rotl<5>(c);
            res = c ^ 0xc6;
        }
    }

    class plaintext : public datum {
        uint8_t buffer[200];
    public:
        plaintext(datum &d) {
            if (d.length() != sizeof(buffer)) {
                d.set_null(); // error: ciphertext has wrong length
                return;
            }
            decrypt(d.data, buffer, sizeof(buffer));
            data = buffer;
            data_end = buffer + sizeof(buffer);
        }
    };

    plaintext pt;
    datum key;
    datum unknown_1;
    datum ipv4;
    datum srv_time;
    datum unknown_2;

    static constexpr size_t weight_threshold = 16;

public:

    tofsee_initial_message(datum &ct) :
        pt{ct},
        key{pt, 128},
        unknown_1{pt, 16},
        ipv4{pt, 4},
        srv_time{pt, 4},
        unknown_2{pt, 48} { }

    void write_json(json_object &o, bool=true) const {
        if (!is_not_empty()) {
            return;
        }
        json_object tofsee{o, "tofsee_initial_message"};
        tofsee.print_key_hex("key", key.data, key.length());
        tofsee.print_key_hex("unknown_1", unknown_1.data, unknown_1.length());
        tofsee.print_key_ipv4_addr("bot_ip", ipv4.data);
        tofsee.print_key_hex("srv_time", srv_time.data, srv_time.length());
        tofsee.print_key_hex("unknown_2", unknown_2.data, unknown_2.length());
        tofsee.close();
    }

    bool is_not_empty() const {
        if (!unknown_2.is_not_null()) {
            return false;  // bad message, probably wrong size
        }
        size_t weight = 0;
        for (const auto & x : unknown_1) {
            weight += __builtin_popcount(x);
        }
        if (weight < weight_threshold) {
            return true;
        }
        return false;
    }

#ifndef NDEBUG
    static bool unit_test() {

        // true positive test: verify the correct parsing of a valid
        // tofsee initial message ciphertext
        //
        uint8_t tim_ciphertext[200] = {
            0xff, 0xd4, 0x33, 0xb9, 0x69, 0x1a, 0x79, 0x7b,
            0xe1, 0x9b, 0x32, 0xa5, 0x26, 0xd0, 0x03, 0x23,
            0xa2, 0x0f, 0x26, 0xe5, 0x81, 0xb3, 0x0d, 0xe9,
            0xb6, 0xd4, 0x5b, 0xa6, 0xed, 0x4e, 0x8d, 0xe2,
            0x15, 0xf3, 0x67, 0xcb, 0xa4, 0x75, 0xd8, 0x28,
            0x76, 0x9b, 0x30, 0xf1, 0x54, 0x02, 0x6d, 0x2e,
            0xfd, 0x6a, 0x33, 0xfc, 0x94, 0x66, 0x06, 0x0b,
            0x2a, 0xa9, 0x2c, 0x64, 0xc8, 0x69, 0x96, 0x88,
            0xf4, 0x23, 0xe7, 0x5a, 0xfd, 0xd7, 0xa4, 0x09,
            0x5a, 0xe3, 0x71, 0xb7, 0x1e, 0x65, 0x98, 0xba,
            0xbc, 0x00, 0xad, 0xc7, 0xc0, 0xae, 0xe2, 0x2c,
            0x32, 0x57, 0xb4, 0xd0, 0xa2, 0x07, 0x43, 0xbc,
            0x0d, 0x40, 0xd7, 0x7f, 0xe9, 0x71, 0xb7, 0xc3,
            0x3f, 0xa5, 0x49, 0xd8, 0xfe, 0x16, 0x72, 0xc0,
            0x9b, 0x62, 0xdc, 0xa4, 0x3c, 0x4c, 0x2d, 0xd3,
            0x3c, 0x6e, 0x8a, 0xc4, 0xcd, 0x45, 0x2b, 0xdb,
            0xe0, 0x31, 0xbf, 0xcb, 0x60, 0x35, 0x9f, 0xca,
            0x60, 0x34, 0x8f, 0x4a, 0x7c, 0xd5, 0x98, 0xf2,
            0x8b, 0xd3, 0x80, 0xfd, 0xfb, 0xb2, 0xab, 0xdd,
            0xcd, 0x8f, 0x1f, 0x24, 0xfb, 0x6d, 0xfa, 0xf9,
            0x66, 0x41, 0x4b, 0xae, 0xb1, 0xb4, 0x67, 0x01,
            0xc6, 0xcb, 0x5b, 0x2e, 0xd0, 0x0f, 0x66, 0xee,
            0x7f, 0xc7, 0x6f, 0x15, 0xfb, 0x86, 0x0d, 0x2c,
            0x10, 0xea, 0x3c, 0xfb, 0x09, 0x82, 0x6e, 0x3d,
            0x9e, 0x79, 0xc6, 0x34, 0x55, 0xac, 0x13, 0x6d
        };
        datum tim{tim_ciphertext, tim_ciphertext + sizeof(tim_ciphertext)};
        tofsee_initial_message tofsee{tim};
        if (!tofsee.is_not_empty()) {
            return false;
        }
        return true;

        // false positive test: verify that an invalid, 200 byte
        // garbage packet will not be accepted as a valid tofsee
        // initial message ciphertext
        //
        for (auto & c : tim_ciphertext) {
            c = 0xff; // overwrite the valid message with garbage
        }
        tofsee_initial_message invalid_tofsee{tim};
        if (invalid_tofsee.is_not_empty()) {
            return false;
        }
        return true;
    }

    static inline bool unit_test_passed = tofsee_initial_message::unit_test();
#endif // NDEBUG

};

#endif // TOFSEE_HPP
