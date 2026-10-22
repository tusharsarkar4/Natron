/* This file is part of Natron <https://natrongithub.github.io/>.
 * Copyright (C) 2026 The Natron developers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <Python.h>
#include "Global/Macros.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include "Engine/ViewerPlaybackCache.h"
#ifndef __NATRON_WIN32__
#include <sys/mman.h>
#endif

NATRON_NAMESPACE_USING

TEST(ViewerPlaybackCache, EmptyBufferIsSafe)
{
    prepareViewerPlaybackCache(0, 0);
    prepareViewerPlaybackCache(0, 100);
    unsigned char byte = 42;
    prepareViewerPlaybackCache(&byte, 0);
    EXPECT_EQ(42, byte);
}

TEST(ViewerPlaybackCache, PreparationDoesNotModifyPixels)
{
    const std::size_t page = viewerPlaybackCachePageSize();
    ASSERT_GT(page, 0U);
    std::vector<unsigned char> pixels(page * 3 + 17);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<unsigned char>(i);
    }
    const std::vector<unsigned char> original = pixels;
    std::thread worker([&]() {
        prepareViewerPlaybackCache(pixels.data() + 7, pixels.size() - 7);
    });
    worker.join();
    EXPECT_EQ(original, pixels);
}

#ifdef __NATRON_WIN32__
namespace {
std::atomic<unsigned> guardedPagesRead(0);
std::atomic<DWORD> faultThread(0);
unsigned char* guardedStart = 0;
std::size_t guardedSize = 0;

LONG CALLBACK observeCachePageRead(EXCEPTION_POINTERS* exception)
{
    const EXCEPTION_RECORD* record = exception->ExceptionRecord;
    if (record->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION && record->NumberParameters >= 2) {
        const unsigned char* address = reinterpret_cast<const unsigned char*>(record->ExceptionInformation[1]);
        if (address >= guardedStart && address < guardedStart + guardedSize) {
            ++guardedPagesRead;
            faultThread.store(GetCurrentThreadId());
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
}

TEST(ViewerPlaybackCache, EveryMappedPageIsReadByWorkerWithoutOverrun)
{
    const std::size_t page = viewerPlaybackCachePageSize();
    unsigned char* pages = static_cast<unsigned char*>(VirtualAlloc(0, page * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    ASSERT_TRUE(pages != 0);
    std::memset(pages, 42, page * 4);
    DWORD oldProtection;
    ASSERT_TRUE(VirtualProtect(pages + page * 3, page, PAGE_NOACCESS, &oldProtection) != 0);
    PVOID handler = AddVectoredExceptionHandler(1, observeCachePageRead);
    ASSERT_TRUE(handler != 0);
    guardedStart = pages;
    guardedSize = page * 3;

    const std::size_t offsets[] = {0, 1, page - 1};
    for (std::size_t offset : offsets) {
        const std::size_t sizes[] = {1, page - offset, page + 1, page * 3 - offset};
        for (std::size_t size : sizes) {
            guardedPagesRead.store(0);
            faultThread.store(0);
            ASSERT_TRUE(VirtualProtect(pages, page * 3, PAGE_READONLY | PAGE_GUARD, &oldProtection) != 0);
            DWORD workerID = 0;
            std::thread worker([&]() {
                workerID = GetCurrentThreadId();
                prepareViewerPlaybackCache(pages + offset, size);
            });
            worker.join();
            const unsigned expectedPages = static_cast<unsigned>((offset + size - 1) / page + 1);
            EXPECT_EQ(expectedPages, guardedPagesRead.load());
            EXPECT_EQ(workerID, faultThread.load());
            EXPECT_NE(GetCurrentThreadId(), faultThread.load());
        }
    }
    RemoveVectoredExceptionHandler(handler);
    VirtualFree(pages, 0, MEM_RELEASE);
    guardedStart = 0;
    guardedSize = 0;
}
#else
TEST(ViewerPlaybackCache, DoesNotReadPastEndOfMapping)
{
    // Keep the next page inaccessible so an overrun fails the native test.
    const std::size_t page = viewerPlaybackCachePageSize();
    unsigned char* pages = static_cast<unsigned char*>(mmap(0, page * 4,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    ASSERT_TRUE(pages != MAP_FAILED);
    std::memset(pages, 42, page * 3);
    ASSERT_EQ(0, mprotect(pages + page * 3, page, PROT_NONE));
    const std::size_t offsets[] = {0, 1, page - 1};
    for (std::size_t offset : offsets) {
        prepareViewerPlaybackCache(pages + offset, page * 3 - offset);
    }
    EXPECT_EQ(42, pages[page * 3 - 1]);
    munmap(pages, page * 4);
}
#endif
