// #include <calculator.h>
#include <unity.h>

#include "generator.h"
// #include <lib/generator.h>

// #include <iostream>
// using std::cerr;
// using std::endl;
#include <fstream>
// using std::ofstream;
#include <cstdlib> // for exit function

// void setUp(void) {
// // set stuff up here
// }

// void tearDown(void) {
// // clean stuff up here
// }

void test_function_calculator_addition(void)
{
    TEST_ASSERT_EQUAL(32, 25 + 7);
}

void sinGrow()
{
    FILE *file = fopen("/tmp/pio.txt", "w");

    // fputs("abc\n", file);

    for (int i = 0; i < 10; i++)
    {
        int nextValue = abc();
        // fputc(i, file);
        fprintf(file, "%d", i);
        fputs(",", file);
        // fputc(nextValue, file);
        fprintf(file, "%d", nextValue);
        fputs("\n", file);
    }

    fclose(file);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_function_calculator_addition);
    UNITY_END();

    sinGrow();

    return 0;
}