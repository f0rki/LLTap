#include <stdio.h>
#include <string.h>
#include <err.h>
#include "liblltap.h"
#include "rsa_gmp.h"


void fault_powm(mpz_t rop, mpz_t base, mpz_t exp, mpz_t mod, size_t* ret) {
  (void)ret;
  puts("Target just computed mpz_powm");
  puts("base:");
  mpz_out_str(stdout, 16, base);
  puts("\nexp:");
  mpz_out_str(stdout, 16, exp);
  puts("\nmod:");
  mpz_out_str(stdout, 16, mod);
  puts("\nresult:");
  mpz_out_str(stdout, 16, rop);
  puts("\nSimulating fault by flipping bit 42");
  mpz_combit(rop, 42);
  mpz_out_str(stdout, 16, rop);
  puts("\nDone. Deregistering hook");
  lltap_deregister_hook("__gmpz_powm", LLTAP_POST_HOOK);
}


void rsa_sign_hook_pre(mpz_t** sig, mpz_t** m, RSAPrivKey** k) {
  (void)sig;(void)k;
  puts("RSA sign function called:\nmessage: ");
  mpz_out_str(stdout, 16, **m);
  puts("\nRegistering hook for mpz_powm");
  /*lltap_register_hook("__gmpz_powm", (LLTapHook) fault_powm, LLTAP_POST_HOOK);*/
}

/*LLTAP_REGISTER_HOOK("puts", puts_hook, LLTAP_REPLACE_HOOK)*/
LLTAP_HOOKSV my_hooks[] = {
    {"rsa_sign", (LLTapHook) rsa_sign_hook_pre, LLTAP_PRE_HOOK},
    {"__gmpz_powm", (LLTapHook) fault_powm, LLTAP_POST_HOOK},
    LLTAP_HOOKSV_END,
    };
LLTAP_REGISTER_HOOKS(my_hooks)
