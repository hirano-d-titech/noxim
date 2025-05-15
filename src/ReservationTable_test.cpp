#include <gtest/gtest.h>
#include "ReservationTable.h"

TEST(ReservationTableTest, BasicAssertions) {
    auto rt = new ReservationTable();
    rt->setSize(4);
    TReservation r;
    r.input = 0;
    r.vc = 0;
    EXPECT_EQ(rt->checkReservation(r, 0), RT_AVAILABLE);
}
