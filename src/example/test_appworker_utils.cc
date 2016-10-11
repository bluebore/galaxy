#include <gtest/gtest.h>
#include "src/appworker/utils.h"

TEST(TestUtils, Md5File) {
    FILE* f = fopen(".test.txt", "w+");
    fprintf(f, "0123456789\n");
    fclose(f);

    std::string md5 = baidu::galaxy::md5::Md5File(".test.txt");
    EXPECT_EQ("3749f52bb326ae96782b42dc0a97b4c1", md5);
    remove(".test.txt");
}

TEST(TestUtils, IsPortOpen) {
    bool ret = baidu::galaxy::net::IsPortOpen(22);
    EXPECT_TRUE(ret);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    // Runs all tests using Google Test.
    return RUN_ALL_TESTS();
}
