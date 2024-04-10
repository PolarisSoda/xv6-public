#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    int a = 1;
    for(int i=1; i<=1000000; i++) for(int j=1; j<=10000; j++) a++; 
    ps(0);
    exit();
}