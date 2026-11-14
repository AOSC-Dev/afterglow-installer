#!/usr/bin/python3
TARGET_POINTER_SIZE = 4

with open("reserved_users.txt") as f:
    reserved_users = [line.strip() for line in f]

reserved_users.sort()
# MIN_CHAR_LEN = min(len(user) for user in reserved_users)
# MAX_CHAR_LEN = max(len(user) for user in reserved_users)
bins = {}
for user in reserved_users:
    bins.setdefault(len(user), []).append(user)

clauses = []
for length, users in bins.items():
    clause = f"        case {length}:"
    for user in users:
        clause += f"\n            if (__builtin_memcmp(user, \"{user}\", {length}) == 0) return true;"
    clause += "\n            break;"
    clauses.append(clause)

TEMPLATE = """// Automatically generated file
#include <stdbool.h>
#include <string.h>

const bool is_reserved_user(const char* restrict user) {
    const size_t len = strnlen(user, 32);
    switch (len) {
        default:
           return false;
@CLAUSES@
    }
    return false;
}
"""

print(TEMPLATE.replace("@CLAUSES@", "\n".join(clauses)))
