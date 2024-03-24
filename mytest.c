#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    int i;
    ps(0);
    for(int i=1; i<=10; i++) printf(1,getnice(i));
    for(int i=1; i<=10; i++) setnice(i,3);
    ps(0);
    exit();
}