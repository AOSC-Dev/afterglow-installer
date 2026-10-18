#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "guess.h"

#ifdef USE_BENCHMARK_TO_GUESS
#include <openssl/bn.h>
#endif

/**
 * Guess the SSH key generation time.
 * Returns the estimated time in seconds.
 */
static long guess_ssh_keygen_time_impl(void) {
#ifdef USE_BENCHMARK_TO_GUESS
  BIGNUM *bn = BN_new();
  if (bn == NULL)
    return -1;
  // quick benchmark
  struct timespec start, end;
  if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
    return -1;
  BN_rand(bn, 128, 0, 0);
  BN_mod_word(bn, 0x10a9fc70042UL);
  if (clock_gettime(CLOCK_MONOTONIC, &end) < 0)
    return -1;
  BN_free(bn);
  return ((end.tv_sec - start.tv_sec) * 1000000000L +
          (end.tv_nsec - start.tv_nsec)) /
         1000L * 28L * 5L / 1000000L;
#else
  char buffer[2048] = {0};
  const int fd = open("/proc/cpuinfo", O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  if (read(fd, buffer, sizeof(buffer)) < 0) {
    close(fd);
    return -1;
  }
  close(fd);
  const char *b = memmem(buffer, sizeof(buffer), "bogomips", 8);
  if (!b)
    return -1;
  char *s = (char *)b;
  for (; *s != '\n' && (*s < '0' || *s > '9') &&
         s < (buffer + sizeof(buffer) - 1);
       ++s)
    ;
  if (s < (buffer + sizeof(buffer) - 1)) {
    unsigned long bogomips = strtoul(s, &s, 10);
    return 11000L / bogomips; // approximation for i486/i586
  }
  return -1;
#endif
}

unsigned long guess_ssh_keygen_time(void) {
  const long r = guess_ssh_keygen_time_impl();
  if (r < 0) {
    return 2;  // fallback to 2 seconds if guessing fails
  }
  return (unsigned long)r;
}