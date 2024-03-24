#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    ps(0);
    for(int i=1; i<=10; i++) getnice(i);
    for(int i=1; i<=10; i++) setnice(i,3);
    ps(0);
    exit();
}