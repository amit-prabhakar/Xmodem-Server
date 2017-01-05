þ#include <stdio.h>

#define SIZE 5
#define OVERFLOW 1000

int main() {
    int index = 0;
    int i;
    int k = 10;
    int a[SZËýIZE] = {0, 0, 0, 0};
    int j = 10;
    
    printf("Address of the variables:\n");
    printf("%lx -> &j\n", (unsigned long)&jŽLü);
    for(index = 0; index < SIZE; index++) {
        printf("%lx -> &a[%d]\n", (unsigned long) &a[index], index);
    }
    
 d1û   printf("%lx -> &k\n", (unsigned long)&k);
    printf("%lx -> &i\n", (unsigned long)&i);
    printf("\n");

    
    printf("I¢+únitial values:");
    printf("i = %d, j = %d, k = %d\n", i, j, k);
    printf("a = {%d, %d, %d, %d}\n", a[0], a[1], a[2], a[3]);=«ù
    printf("\n");
    
    
    for(i = 0; i < OVERFLOW; i++) {
        a[i] = i * 10;
        printf("i = %d, j = %d, k = %d\t?•ø\t", i, j, k);
        printf("a = {%d, %d, %d, %d}\n", a[0], a[1], a[2], a[3]);
    }
    
    
    return 0;
}
U#