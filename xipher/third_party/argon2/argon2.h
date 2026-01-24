#ifndef XIPHER_THIRD_PARTY_ARGON2_H
#define XIPHER_THIRD_PARTY_ARGON2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum argon2_type {
    Argon2_d = 0,
    Argon2_i = 1,
    Argon2_id = 2
} argon2_type;

int argon2id_hash_encoded(uint32_t t_cost,
                          uint32_t m_cost,
                          uint32_t parallelism,
                          const void *pwd,
                          size_t pwdlen,
                          const void *salt,
                          size_t saltlen,
                          size_t hashlen,
                          char *encoded,
                          size_t encodedlen);

int argon2id_verify(const char *encoded, const void *pwd, size_t pwdlen);

size_t argon2_encodedlen(uint32_t t_cost,
                         uint32_t m_cost,
                         uint32_t parallelism,
                         uint32_t saltlen,
                         uint32_t hashlen,
                         argon2_type type);

const char *argon2_error_message(int error_code);

#ifndef ARGON2_OK
#define ARGON2_OK 0
#endif

#ifdef __cplusplus
}
#endif

#endif // XIPHER_THIRD_PARTY_ARGON2_H
