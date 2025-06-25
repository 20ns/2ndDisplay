#pragma once

// Minimal stub of ViGEm Client header sufficient for TabDisplay to compile when
// the real SDK is not present.  If you have the official ViGEm SDK installed,
// simply make sure its include directory is on the compiler include path and
// delete this stub.

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _VIGEM_CLIENT_T* PVIGEM_CLIENT;
typedef struct _VIGEM_TARGET_T* PVIGEM_TARGET;

typedef enum {
    VIGEM_ERROR_NONE = 0,
    VIGEM_ERROR_GENERAL = 1
} VIGEM_ERROR;

static inline BOOL VIGEM_SUCCESS(VIGEM_ERROR e) { return e == VIGEM_ERROR_NONE; }

// Function stubs.  They do nothing at runtime unless you replace them with the
// real library via dynamic loading or remove this header.
static inline PVIGEM_CLIENT vigem_alloc(void) { return NULL; }
static inline void vigem_free(PVIGEM_CLIENT) {}
static inline VIGEM_ERROR vigem_connect(PVIGEM_CLIENT) { return VIGEM_ERROR_GENERAL; }
static inline void vigem_disconnect(PVIGEM_CLIENT) {}

static inline PVIGEM_TARGET vigem_target_x360_alloc(void) { return NULL; }
static inline void vigem_target_free(PVIGEM_TARGET) {}
static inline VIGEM_ERROR vigem_target_add(PVIGEM_CLIENT, PVIGEM_TARGET) { return VIGEM_ERROR_GENERAL; }
static inline void vigem_target_remove(PVIGEM_CLIENT, PVIGEM_TARGET) {}

#ifdef __cplusplus
}
#endif 