// fingerprint_auth.h
#ifndef FINGERPRINT_AUTH_H
#define FINGERPRINT_AUTH_H

// #include <time.h>

// Cấu trúc để lưu kết quả xác thực vân tay
typedef struct {
    int fingerID;
    int confidence;
    // time_t timestamp;
} fingerprint_auth_result_t;

// Khai báo biến toàn cục sử dụng extern
extern fingerprint_auth_result_t auth_result;

#endif