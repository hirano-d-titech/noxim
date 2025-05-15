#include <gtest/gtest.h>
#include "ReservationTable.h"

TEST(ReservationTableTest, reserve) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    {
        TReservation r;
        r.input = 0;
        r.vc = 0;
        rt->reserve(r, 0); // EXPECT_NO_DEATH
        EXPECT_DEATH(rt->reserve(r, 0), ".*");
        EXPECT_DEATH(rt->reserve(r, 1), ".*");
    }

    {
        TReservation r;
        r.input = 1;
        r.vc = 0;
        EXPECT_DEATH(rt->reserve(r, 0), ".*");
        rt->reserve(r, 1); // EXPECT_NO_DEATH
    }

    {
        TReservation r;
        r.input = 2;
        r.vc = 1;
        rt->reserve(r, 2);
        r.input = 3;
        rt->reserve(r, 3);
    }
}

TEST(ReservationTableTest, checkReservation) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    {
        TReservation r;
        r.input = 0;
        r.vc = 0;
        EXPECT_EQ(rt->checkReservation(r, 0), RT_AVAILABLE);
        rt->reserve(r, 0);
        EXPECT_EQ(rt->checkReservation(r, 0), RT_ALREADY_SAME);
        EXPECT_EQ(rt->checkReservation(r, 1), RT_ALREADY_OTHER_OUT);
    }

    {
        TReservation r;
        r.input = 1;
        r.vc = 0;
        EXPECT_EQ(rt->checkReservation(r, 0), RT_OUTVC_BUSY);
        EXPECT_EQ(rt->checkReservation(r, 1), RT_AVAILABLE);
    }

    {
        TReservation r;
        r.input = 0;
        r.vc = 1;
        EXPECT_EQ(rt->checkReservation(r, 2), RT_AVAILABLE);
        EXPECT_EQ(rt->checkReservation(r, 3), RT_AVAILABLE);
    }
}

TEST(ReservationTableTest, release) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    {
        TReservation r;
        r.input = 0;
        r.vc = 0;
        rt->reserve(r, 0);
        EXPECT_DEATH(rt->release(r, 1), ".*");
        rt->release(r, 0);
        EXPECT_DEATH(rt->release(r, 0), ".*");
    }
}

TEST(ReservationTableTest, getReservations) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    {
        TReservation r;
        r.input = 0;
        r.vc = 0;
        rt->reserve(r, 0);
        EXPECT_EQ(rt->getReservations(0).size(), 1);
        EXPECT_EQ(rt->getReservations(1).size(), 0);
        rt->release(r, 0);
        EXPECT_EQ(rt->getReservations(0).size(), 0);
    }

    {
        TReservation r;
        r.input = 1;
        r.vc = 0;
        rt->reserve(r, 1);
        EXPECT_EQ(rt->getReservations(1).size(), 1);
    }
}

TEST(ReservationTableTest, isNotReserved) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    {
        TReservation r;
        r.input = 0;
        r.vc = 0;
        EXPECT_TRUE(rt->isNotReserved(0));
        rt->reserve(r, 0);
        EXPECT_FALSE(rt->isNotReserved(0));
        rt->release(r, 0);
        EXPECT_TRUE(rt->isNotReserved(0));
    }
}

TEST(ReservationTableTest, updateIndex) {
    auto rt = new ReservationTable();
    rt->setSize(DIRECTIONS + 2);

    TReservation r;
    r.input = 0;
    r.vc = 0;
    rt->reserve(r, 0);
    TReservation r2;
    r2.input = 1;
    r2.vc = 1;
    rt->reserve(r2, 0);

    auto get_first = rt->getReservations(0);
    rt->updateIndex();
    auto get_second = rt->getReservations(0);
    EXPECT_NE(get_first.size(), get_second.size());
}
