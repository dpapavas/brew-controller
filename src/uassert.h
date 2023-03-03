#ifndef _UASSERT_H_
#define _UASSERT_H_

void __attribute__((noreturn)) _uassert(
    const char *msg, int line, const char *func);

#define uassert(cond)                                           \
    if (!(cond)) {                                              \
        _uassert(__FILE__ ":%d: %s:: Assertion `" #cond         \
                "` failed.\n", __LINE__, __func__);             \
    }

#endif
