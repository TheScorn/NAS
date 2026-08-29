#include "../NAS.h"
#include <gtest/gtest.h>
#include <cstring>
#include <stdlib.h>

class HandlerVerifyIntTest: public testing::Test {
    protected:
};

class DirSizeTest: public testing::Test {
    protected:
};

TEST(HandlerVerifyIntTest, IntString) {
    char mes[] = "1234";
    int verify_int_status = verify_int(mes);
    EXPECT_EQ(0, verify_int_status);
}

TEST(HandlerVerifyIntTest, NoIntString) {
    char mes[] = "asmda123sa";
    int verify_status = verify_int(mes);
    EXPECT_EQ(-1, verify_status);
}

TEST(HandlerVerifyIntTest, FloatString) {
    char mes[] = "13.421";
    int verify_status = verify_int(mes);
    EXPECT_EQ(-1, verify_status);
}

//Ten test jest do wyjebania XD. Generalnie testy trzeba będzie się nauczyc pisać.
TEST(DirSizeTest, ExampleDir) {
    char dirpath[] = "/home/thescorn/science/mine/asm";
    unsigned long long total = dir_size(dirpath);
    EXPECT_EQ(total, 350918);
}