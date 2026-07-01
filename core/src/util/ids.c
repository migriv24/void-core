/* ids.c — real-ID minting (SPEC §3.1). Prefers the OS CSPRNG
 * (BCryptGenRandom on Windows, /dev/urandom elsewhere); falls back to a seeded
 * splitmix64 only if that fails, so an id is always produced. */
#include "vc_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#  include <windows.h>
#  include <bcrypt.h>
#  include <process.h>
#else
#  include <stdio.h>
#endif

static int csprng_bytes(unsigned char *buf, size_t n) {
#if defined(_WIN32)
  return BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, (ULONG)n,
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#else
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f) return 0;
  size_t r = fread(buf, 1, n, f);
  fclose(f);
  return r == n;
#endif
}

char *vc_strdup(const char *s) {
  if (!s) s = "";
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}

static uint64_t g_state;
static int g_seeded;

static uint64_t splitmix64(uint64_t *x) {
  uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

static void ensure_seed(void) {
  if (g_seeded) return;
  uint64_t seed = (uint64_t)time(NULL);
  seed ^= (uint64_t)(uintptr_t)&seed << 16;
#if defined(_WIN32)
  seed ^= (uint64_t)_getpid() * 0x100000001B3ULL;
#endif
  g_state = seed ? seed : 0xDEADBEEFCAFEBABEULL;
  g_seeded = 1;
}

void vc_mint_id(const char *prefix, char *out, size_t out_sz) {
  unsigned char b[5];
  unsigned long long h;
  if (csprng_bytes(b, sizeof b)) {
    h = ((unsigned long long)b[0] << 32) | ((unsigned long long)b[1] << 24) |
        ((unsigned long long)b[2] << 16) | ((unsigned long long)b[3] << 8) | b[4];
  } else { /* fallback PRNG (still 40 bits) */
    ensure_seed();
    h = (unsigned long long)(splitmix64(&g_state) & 0xFFFFFFFFFFULL);
  }
  /* 10 hex digits (40 bits), matching the SPEC examples (rune_9fa3c1b7e2). */
  snprintf(out, out_sz, "%s_%010llx", prefix ? prefix : "id", h);
}
