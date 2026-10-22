/* This file is part of Natron <https://natrongithub.github.io/>.
 * Copyright (C) 2026 The Natron developers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NATRON_ENGINE_VIEWERPLAYBACKCACHE_H
#define NATRON_ENGINE_VIEWERPLAYBACKCACHE_H

// Python must precede standard headers in Natron translation units.
#include <Python.h>
#include "Global/Macros.h"

#include <cstddef>
#ifdef __NATRON_WIN32__
#include <windows.h>
#else
#include <unistd.h>
#endif

NATRON_NAMESPACE_ENTER

inline std::size_t viewerPlaybackCachePageSize()
{
    static const std::size_t pageSize = []() -> std::size_t {
#ifdef __NATRON_WIN32__
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        return size > 0 ? static_cast<std::size_t>(size) : 4096;
#endif
    }();
    return pageSize;
}

/**
 * Prepare a disk-cached Viewer tile in a render worker before queuing display.
 * Reading one byte per page moves demand-page faults away from the GUI/PBO
 * upload without allocating or copying another image. The caller must retain
 * the cache entry until upload, just as for an ordinary cache hit.
 * This does not pin pages: the OS may still reclaim them under memory pressure.
 */
inline void prepareViewerPlaybackCache(const unsigned char* source, std::size_t size)
{
    if (!source || size == 0) {
        return;
    }
    const volatile unsigned char* data = source;
    const std::size_t pageSize = viewerPlaybackCachePageSize();
    std::size_t offset = 0;
    for (;;) {
        const unsigned char value = data[offset];
        (void)value;
        // Subtraction prevents overflow at the end of a very large buffer.
        if (size - offset <= pageSize) {
            break;
        }
        offset += pageSize;
    }
    // An unaligned tile can end on a page not visited by the loop above.
    const unsigned char last = data[size - 1];
    (void)last;
}

NATRON_NAMESPACE_EXIT

#endif // NATRON_ENGINE_VIEWERPLAYBACKCACHE_H
