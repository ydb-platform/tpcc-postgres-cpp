#include "log_backend.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace NTPCC;

TEST(TLogCaptureTest, ShouldStartEmpty) {
    TLogCapture capture(10);
    auto lines = capture.GetLines();
    ASSERT_TRUE(lines.empty());
}

TEST(TLogCaptureTest, ShouldAddAndRetrieveLines) {
    TLogCapture capture(10);

    capture.AddLine("line 1");
    capture.AddLine("line 2");
    capture.AddLine("line 3");

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 3u);
    ASSERT_EQ(lines[0], "line 1");
    ASSERT_EQ(lines[1], "line 2");
    ASSERT_EQ(lines[2], "line 3");
}

TEST(TLogCaptureTest, ShouldRespectMaxLines) {
    TLogCapture capture(3);

    capture.AddLine("line 1");
    capture.AddLine("line 2");
    capture.AddLine("line 3");
    capture.AddLine("line 4");
    capture.AddLine("line 5");

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 3u);
    ASSERT_EQ(lines[0], "line 3");
    ASSERT_EQ(lines[1], "line 4");
    ASSERT_EQ(lines[2], "line 5");
}

TEST(TLogCaptureTest, ShouldWrapAroundCorrectly) {
    TLogCapture capture(4);

    for (int i = 1; i <= 6; ++i) {
        capture.AddLine("msg " + std::to_string(i));
    }

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 4u);
    ASSERT_EQ(lines[0], "msg 3");
    ASSERT_EQ(lines[1], "msg 4");
    ASSERT_EQ(lines[2], "msg 5");
    ASSERT_EQ(lines[3], "msg 6");
}

TEST(TLogCaptureTest, ShouldClear) {
    TLogCapture capture(10);

    capture.AddLine("line 1");
    capture.AddLine("line 2");

    capture.Clear();

    auto lines = capture.GetLines();
    ASSERT_TRUE(lines.empty());
}

TEST(TLogCaptureTest, ShouldWorkAfterClear) {
    TLogCapture capture(10);

    capture.AddLine("before clear");
    capture.Clear();
    capture.AddLine("after clear");

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], "after clear");
}

TEST(TLogCaptureTest, ShouldHandleSingleCapacity) {
    TLogCapture capture(1);

    capture.AddLine("first");
    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], "first");

    capture.AddLine("second");
    lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], "second");
}

TEST(TLogCaptureTest, ShouldPreserveFIFOOrder) {
    TLogCapture capture(100);

    for (int i = 0; i < 50; ++i) {
        capture.AddLine("msg " + std::to_string(i));
    }

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 50u);
    for (int i = 0; i < 50; ++i) {
        ASSERT_EQ(lines[i], "msg " + std::to_string(i));
    }
}

TEST(TLogCaptureTest, ShouldHandleExactCapacityFill) {
    TLogCapture capture(5);

    for (int i = 0; i < 5; ++i) {
        capture.AddLine("line " + std::to_string(i));
    }

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(lines[i], "line " + std::to_string(i));
    }
}

TEST(TLogCaptureTest, ShouldHandleMultipleWraps) {
    TLogCapture capture(3);

    for (int i = 0; i < 20; ++i) {
        capture.AddLine("item " + std::to_string(i));
    }

    auto lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 3u);
    ASSERT_EQ(lines[0], "item 17");
    ASSERT_EQ(lines[1], "item 18");
    ASSERT_EQ(lines[2], "item 19");
}

TEST(TLogCaptureTest, ShouldClearAfterWrap) {
    TLogCapture capture(3);

    for (int i = 0; i < 10; ++i) {
        capture.AddLine("x" + std::to_string(i));
    }

    capture.Clear();
    auto lines = capture.GetLines();
    ASSERT_TRUE(lines.empty());

    capture.AddLine("fresh");
    lines = capture.GetLines();
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], "fresh");
}
