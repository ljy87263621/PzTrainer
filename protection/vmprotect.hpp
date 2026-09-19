#pragma once

#if defined(_MSC_VER)
#define PZ_VMP_NOINLINE __declspec(noinline)
#else
#define PZ_VMP_NOINLINE
#endif

#if defined(PZ_ENABLE_VMP_MARKERS)
#include <VMProtectSDK.h>

#define PZ_VMP_BEGIN_VIRTUALIZATION(name) VMProtectBeginVirtualization(name)
#define PZ_VMP_BEGIN_MUTATION(name) VMProtectBeginMutation(name)
#define PZ_VMP_BEGIN_ULTRA(name) VMProtectBeginUltra(name)
#define PZ_VMP_END() VMProtectEnd()
#define PZ_VMP_DECRYPT_STRING_A(value) VMProtectDecryptStringA(value)
#define PZ_VMP_DECRYPT_STRING_W(value) VMProtectDecryptStringW(value)
#else
#define PZ_VMP_BEGIN_VIRTUALIZATION(name) ((void)0)
#define PZ_VMP_BEGIN_MUTATION(name) ((void)0)
#define PZ_VMP_BEGIN_ULTRA(name) ((void)0)
#define PZ_VMP_END() ((void)0)
#define PZ_VMP_DECRYPT_STRING_A(value) (value)
#define PZ_VMP_DECRYPT_STRING_W(value) (value)
#endif
