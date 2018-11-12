#include <util/bitmap.h>
#include "gtest/gtest.h"

namespace {

// bitmap.
TEST(bitmap, assign) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i = 0;

    bitmap * src = new (0) bitmap(bitmap_size, 0);
    bitmap * dst = new (0) bitmap(bitmap_size, 0);
    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Assign to the destination */
    *dst = *src;

    for (i = 0; i < bitmap_size; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    EXPECT_FALSE(*dst != *src);

    delete (src);
    delete (dst);

}

TEST(bitmap, copy) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i = 0;
    int16_t start = rand() % bitmap_size;
    int16_t len = rand() % bitmap_size;

    bitmap * src = new (0) bitmap(bitmap_size, 0);
    bitmap * dst = new (0) bitmap(bitmap_size, 0);

    EXPECT_GE(src->Size(), bitmap_size);
    EXPECT_GE(dst->Size(), bitmap_size);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Order the start and end */
    if (len > bitmap_size - start) {
        len = -1;
    }

    /* Assign to the destination */
    src->CopyRange(start, len, *dst);

    for (i = start; i < start + len; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    delete (src);
    delete (dst);

}

TEST(bitmap, assign_different_size) {
    int16_t bitmap_size_1 = rand() & 0x7FFF;
    int16_t bitmap_size_2 = rand() & 0x7FFF;
    int16_t i = 0;

    bitmap * src = new (0) bitmap(bitmap_size_1, 0);
    bitmap * dst = new (0) bitmap(bitmap_size_2, 0);

    EXPECT_GE(src->Size(), bitmap_size_1);
    EXPECT_GE(dst->Size(), bitmap_size_2);

    /* Fill it randomly */
    for (i = 0; i < bitmap_size_1; i++) {
        if (rand() % 2) src->SetIndex(i);
    }

    /* Assign to the destination */
    *dst = *src;

    for (i = 0; i < bitmap_size_1; i++) {
        EXPECT_EQ(src->Value(i), dst->Value(i));
    }

    EXPECT_EQ(src->Count(), dst->Count());
    EXPECT_EQ(src->Size(), dst->Size());
    EXPECT_GE(dst->Size(), bitmap_size_1);

    delete (src);
    delete (dst);

}

TEST(bitmap, resize) {
    int16_t bitmap_size_start = rand() & 0x7FFF;
    int16_t bitmap_size_end = rand() & 0x7FFF;
    int16_t i = 0;

    bitmap * bm = new (0) bitmap(bitmap_size_start, 0);

    EXPECT_GE(bm->Size(), bitmap_size_start);

    /* Fill it with ones */
    for (i = 0; i < bitmap_size_start; i++) {
        bm->SetIndex(i);
    }

    EXPECT_EQ(bm->Count(), bitmap_size_start);

    bm->Resize(bitmap_size_end);

    if (bitmap_size_end < bitmap_size_start) {
        EXPECT_EQ(bm->Count(), bitmap_size_end);
    } else {
        EXPECT_EQ(bm->Count(), bitmap_size_start);
    }

    delete (bm);

}

TEST(bitmap, set_range) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start = rand() % bitmap_size;
    int16_t len = rand() % bitmap_size;

    bitmap * bm = new (0) bitmap(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);

    /* Clean then bitmap */
    bm->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);

    /* Order the start and end */
    if (len > bitmap_size - start) {
        len = -1;
    }

    /* Set the range */
    bm->SetRange(start, len);

    /* Verify the set bits */
    if (len >= 0) {
        EXPECT_EQ(bm->Count(), len);
    } else {
        EXPECT_EQ(bm->Count(), bitmap_size - start);
    }

    delete (bm);

}

TEST(bitmap, set_range_and_copy_till_end) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t start = rand() % bitmap_size;
    int16_t len = -1;

    bitmap * bm = new (0) bitmap(bitmap_size, 0);
    bitmap * bm_cpy = new (0) bitmap(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);
    EXPECT_GE(bm_cpy->Size(), bitmap_size);

    /* Clean the bitmap */
    bm->FreeRange(0, bitmap_size);
    bm_cpy->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);
    EXPECT_EQ(bm_cpy->Count(), 0);

    /* Set the range */
    bm->SetRange(start, len);
    bm->CopyRange(start, len, *bm_cpy);

    /* Verify the set bits */
    EXPECT_EQ(bm->Count(), bitmap_size - start);
    EXPECT_EQ(bm_cpy->Count(), bitmap_size - start);

    delete (bm);
    delete (bm_cpy);

}

TEST(bitmap, purge_delete) {
    int16_t bitmap_size = rand() & 0x7FFF;

    bitmap * bm = new (0) bitmap(bitmap_size, 0);

    EXPECT_GE(bm->Size(), bitmap_size);

    bm->purge();

    delete (bm);
}

TEST(bitmap, get_free_index) {
    int16_t bitmap_size = rand() & 0x7FFF;
    int16_t i = 0;
    int16_t current_index = 0;

    bitmap * bm = new (0) bitmap(bitmap_size, 0);
    
    EXPECT_GE(bm->Size(), bitmap_size);
    bm->FreeRange(0, bitmap_size);
    EXPECT_EQ(bm->Count(), 0);

    for (i = 0; i < bitmap_size; i++) {
        current_index = bm->GetFreeIndex();
        EXPECT_EQ(current_index, i);
    }

    delete (bm);
}

}  // namespace