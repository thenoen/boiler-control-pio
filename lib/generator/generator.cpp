#include <math.h>

int state = 0;
int val = 0;

int abc()
{
    val++;
    state += 0.1;
    return val * 2;
}

int sinShrink()
{
    // y= 150 + sin(0.8x)*3 - 0.1x
    int y = 150 + sin(state * 0.8) * 3 - 0.6 * state;
    state++;
    return y;
}