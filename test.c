#include <stdio.h>

extern float foo();

int main() {
    float v = foo();
    printf("%f\n", v);
    return 0;
}
