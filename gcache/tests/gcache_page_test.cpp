/*
 * Copyright (C) 2010-2019 Codership Oy <info@codership.com>
 *
 * $Id$
 */

#include "gcache_page_store.hpp"
#include "gcache_bh.hpp"
#include "gcache_page_test.hpp"

using namespace gcache;

static void ps_free (void* ptr)
{
    BufferHeader* const bh(ptr2BH(ptr));
    BH_release (bh);
    bh->seqno_g = SEQNO_ILL;
}

START_TEST(test1)
{
    log_info << "\n"
        "##########################\n"
        "##                      ##\n"
        "##        Test 1        ##\n"
        "##                      ##\n"
        "##########################\n";

    const char* const dir_name = "";
    ssize_t const bh_size = sizeof(gcache::BufferHeader);
    ssize_t const keep_size = 1;
    ssize_t const page_size = 2 + bh_size + gcache::Page::meta_size(BH_size(0));

    gcache::PageStore ps(dir_name, NULL, NULL, keep_size, page_size, page_size,
                         0, false);

    fail_if(ps.count()       != 0,"expected count 0, got %zu",ps.count());
    fail_if(ps.total_pages() != 0,"expected 0 pages, got %zu",ps.total_pages());
    fail_if(ps.total_size()  != 0,"expected size 0, got %zu", ps.total_size());

    void* ptx;
    size_t size(3 + bh_size);
    void* buf(ps.malloc(size, ptx));

    fail_if(NULL == buf);
    fail_if(NULL == ptx);
    fail_if(ps.count()       != 1,"expected count 1, got %zu",ps.count());
    fail_if(ps.total_pages() != 1,"expected 1 pages, got %zu",ps.total_pages());

    size -= 1;
    void* tmp = ps.realloc (buf, size);

    fail_if(buf != tmp);
    fail_if(ps.count()       != 1,"expected count 1, got %zu",ps.count());
    fail_if(ps.total_pages() != 1,"expected 1 pages, got %zu",ps.total_pages());

    size += gcache::Page::ALIGNMENT;
    // the following should fail as new page needs to be allocated
    tmp = ps.realloc (buf, size);

    fail_if(0   != tmp);
    fail_if(buf == tmp);
    fail_if(ps.count()       != 1,"expected count 1, got %zu",ps.count());
    fail_if(ps.total_pages() != 1,"expected 1 pages, got %zu",ps.total_pages());

    ps_free(buf);
    ps.discard (ptr2BH(buf));

    fail_if(ps.count()       != 1,"expected count 1, got %zu",ps.count());
    fail_if(ps.total_pages() != 0,"expected 0 pages, got %zu",ps.total_pages());
    fail_if(ps.total_size()  != 0,"expected size 0, got %zu", ps.total_size());
}
END_TEST

START_TEST(test2)
{
    log_info << "\n"
        "##########################\n"
        "##                      ##\n"
        "##        Test 2        ##\n"
        "##                      ##\n"
        "##########################\n";

    const char* const dir_name = "";
    ssize_t const bh_size = sizeof(gcache::BufferHeader);
    ssize_t const keep_size = 1;
    ssize_t page_size = (1 << 20) + bh_size;

    gcache::PageStore ps(dir_name, NULL, NULL, keep_size, page_size, page_size,
                         0, false);

    mark_point();

    void* ptx;
    uint8_t* buf = static_cast<uint8_t*>(ps.malloc(page_size, ptx));

    fail_if (0 == buf);
    fail_if (0 == ptx);

    while (--page_size)
    {
        buf[page_size] = page_size;
    }

    mark_point();

    ps_free(buf);
    ps.discard (ptr2BH(buf));
}
END_TEST

START_TEST(test3) // check that all page size is efficiently used
{
    log_info << "\n"
        "##########################\n"
        "##                      ##\n"
        "##        Test 3        ##\n"
        "##                      ##\n"
        "##########################\n";

    const char* const dir_name = "";
    ssize_t const keep_size = 1;
    ssize_t const page_overhead(gcache::Page::meta_size(BH_size(0)));
    ssize_t const page_size = 1024 + page_overhead;

    gcache::PageStore ps (dir_name, NULL, NULL, keep_size, page_size, page_size,
                          0, true);

    mark_point();

    ssize_t ptr_size = ((page_size - page_overhead) / 2);
    /* exactly half of the payload */
    assert(ptr_size == gcache::Page::aligned_size(ptr_size));

    void* ptx;
    void* ptr1 = ps.malloc(ptr_size, ptx);
    fail_if (0 == ptr1);

    void* ptr2 = ps.malloc(ptr_size, ptx);
    fail_if (0 == ptr2);

    fail_if (ps.count() != 1, "ps.count() = %zd, expected 1", ps.count());

    // check that ptr2 is adjacent to ptr1
    void* tmp = static_cast<uint8_t*>(ptr1) + ptr_size;

    fail_if (tmp != ptr2, "tmp = %p, ptr2 = %p", tmp, ptr2);

    ps_free(ptr1); ps.discard(ptr2BH(ptr1));
    ps_free(ptr2); ps.discard(ptr2BH(ptr2));
}
END_TEST

START_TEST(test4) // check that pages linger correctly and get deleted as they
{                 // should when keep_size is exceeded
    log_info << "\n"
        "##########################\n"
        "##                      ##\n"
        "##        Test 4        ##\n"
        "##                      ##\n"
        "##########################\n";

    const char* const dir_name = "";
    ssize_t const page_size = 1024;
    ssize_t const keep_pages = 3;
    ssize_t const keep_size = keep_pages * page_size;
    ssize_t const alloc_size = page_size - gcache::Page::meta_size(BH_size(0));
    size_t expect;

    gcache::PageStore ps(dir_name, NULL, NULL, keep_size, page_size, page_size,
                         0, false);

    fail_if(ps.count() != 0);
    fail_if(ps.total_pages() != 0);

    void* ptx;
    void* ptr1(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr1);
    expect = 1;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr2(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr2);
    expect = 2;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr3(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr3);
    expect = 3;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr4(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr4);
    expect = 4;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr1);
    ps.free(ptr2BH(ptr1));
    expect = keep_pages;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr2);
    ps.free(ptr2BH(ptr2));
    expect = keep_pages;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr5(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr5);
    expect = 3;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr5);
    ps.free(ptr2BH(ptr5));
    expect = 3;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr4);
    ps.discard(ptr2BH(ptr4));
    expect = 3;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr6(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr6);
    expect = 4; // page 3 is still locked
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr6);
    ps.free(ptr2BH(ptr6));
    expect = 4; // page 3 is still locked
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    void* ptr7(ps.malloc(alloc_size, ptx));
    fail_if(NULL == ptx);
    fail_if(NULL == ptr7);
    expect = 5;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr7);
    ps.free(ptr2BH(ptr7));
    expect = 5;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    ps_free(ptr3);
    ps.free(ptr2BH(ptr3));
    expect = keep_pages;
    fail_if(ps.total_pages() != expect, "Expected total_pages() = %d, got %d",
            expect, ps.total_pages());

    fail_if(ps.count() != 7);
}
END_TEST

Suite* gcache_page_suite()
{
    Suite* s = suite_create("gcache::PageStore");
    TCase* tc;

    tc = tcase_create("test");
    tcase_add_test(tc, test1);
    tcase_add_test(tc, test2);
    tcase_add_test(tc, test3);
    tcase_add_test(tc, test4);
    suite_add_tcase(s, tc);

    return s;
}
