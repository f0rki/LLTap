#ifndef RSA_GMP_H
#define RSA_GMP_H

#include <gmp.h>

#define PRIME_SIZE 256

typedef struct {
  mpz_t n;
  mpz_t e;
} RSAPubKey;

typedef struct {
  RSAPubKey pub;
  mpz_t d;
  mpz_t p;
  mpz_t q;
  struct {
    mpz_t dp;
    mpz_t dq;
    mpz_t qinv;
  } crt;
} RSAPrivKey;

#endif
