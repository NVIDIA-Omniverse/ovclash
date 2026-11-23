// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

// The only definition of OVCLASH_API, shared by <ovclash/ovclash.h> and the Suite headers. The
// library build defines OVCLASH_EXPORTS, which selects dllexport on Windows. Consumers get
// dllimport on Windows and default symbol visibility with GCC and Clang. A consumer that defines
// OVCLASH_API before including this header keeps its own definition.
#if !defined(OVCLASH_API)
    #if defined(_WIN32)
        #if defined(OVCLASH_EXPORTS)
            #define OVCLASH_API __declspec(dllexport)
        #else
            #define OVCLASH_API __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) || defined(__clang__)
        #define OVCLASH_API __attribute__((visibility("default")))
    #else
        #define OVCLASH_API
    #endif
#endif
