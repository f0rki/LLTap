#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "rsa_gmp.h"


void rsa_clear(RSAPrivKey* k)
{
  mpz_clears(k->pub.n, k->pub.e,
             k->d, k->p, k->q,
             k->crt.dp, k->crt.dq, k->crt.qinv,
             NULL);
}


void rsa_keygen(RSAPrivKey* k)
{
  gmp_randstate_t randstate;
  mpz_t one, phin, t1;
  mpz_inits(one, phin, t1, 0);

  mpz_init2(k->pub.n, PRIME_SIZE  * 2);
  mpz_init(k->pub.e);
  mpz_init(k->d);
  mpz_init2(k->p, PRIME_SIZE);
  mpz_init2(k->q, PRIME_SIZE);
  mpz_init(k->crt.dp);
  mpz_init(k->crt.dq);
  mpz_init(k->crt.qinv);

  mpz_set_ui(one, 1);

  // e = 65537
  mpz_set_ui(k->pub.e, 65537);

  // p = random prime
  gmp_randinit_default(randstate);
  mpz_urandomb(k->p, randstate, PRIME_SIZE);
  mpz_nextprime(k->p, k->p);

  // q = random prime s.t. phi(pq) = 1

  do {
    mpz_set_ui(k->pub.n, 0);
    mpz_set_ui(k->q, 0);
    mpz_set_ui(t1, 0);
    mpz_urandomb(t1, randstate, PRIME_SIZE);
    mpz_nextprime(k->q, t1);
    mpz_mul(k->pub.n, k->p, k->q);
    mpz_set_ui(phin, 0);
    mpz_set_ui(t1, 0);
    mpz_sub(phin, k->p, one);
    mpz_sub(t1, k->q, one);
    mpz_mul(phin, phin, t1);
    mpz_set_ui(t1, 0);
    mpz_gcd(t1, phin, k->pub.e);
  } while(mpz_cmp(t1, one) != 0);

  mpz_invert(k->d, k->pub.e, phin);

  // dp = d mod (p-1)
  mpz_set_ui(t1, 0);
  mpz_sub(t1, k->p, one);
  mpz_mod(k->crt.dp, k->d, t1);
  // dq = d mod (q-1)
  mpz_set_ui(t1, 0);
  mpz_sub(t1, k->q, one);
  mpz_mod(k->crt.dq, k->d, t1);
  // qinv = q^-1 mod p
  mpz_invert(k->crt.qinv, k->q, k->p);

  fputs("Generated the following RSA key:\nn = ", stdout);
  mpz_out_str(stdout, 16, k->pub.n);
  fputs("\ne = ", stdout);
  mpz_out_str(stdout, 16, k->pub.e);
  fputs("\np = ", stdout);
  mpz_out_str(stdout, 16, k->p);
  fputs("\nq = ", stdout);
  mpz_out_str(stdout, 16, k->q);

  puts("");

  mpz_clears(one, phin, t1, 0);
  gmp_randclear(randstate);
}


void rsa_sign(mpz_t* sig, mpz_t m, RSAPrivKey* k)
{
  mpz_t sp, sq;
  mpz_inits(sp, sq, NULL);

  mpz_powm(sp, m, k->crt.dp, k->p);
  mpz_powm(sq, m, k->crt.dq, k->q);

  mpz_sub(*sig, sp, sq);
  mpz_mod(*sig, *sig, k->p);
  mpz_mul(*sig, *sig, k->crt.qinv);
  mpz_mod(*sig, *sig, k->p);
  mpz_mul(*sig, *sig, k->q);
  mpz_add(*sig, *sig, sq);
  mpz_mod(*sig, *sig, k->pub.n);

  mpz_clears(sp, sq, NULL);
}


bool rsa_verify(mpz_t sig, mpz_t m, RSAPubKey* k)
{
  mpz_t sigm;
  mpz_init(sigm);
  mpz_powm(sigm, sig, k->e, k->n);
  bool r = (mpz_cmp(m, sigm) == 0);
  mpz_clear(sigm);
  return r;
}

int main(void)
{
  mpz_t sig;
  mpz_t m;
  RSAPrivKey k;

  rsa_keygen(&k);

  mpz_init(m);
  mpz_init(sig);

  mpz_set_ui(m, 1234);
  rsa_sign(&sig, m, &k);
  bool ret = rsa_verify(sig, m, &k.pub);
  if (ret) {
    puts("RSA verify success");
  } else {
    puts("RSA verify fail!");
  }
  mpz_clears(sig, m, NULL);
  rsa_clear(&k);
  return ret;
}
