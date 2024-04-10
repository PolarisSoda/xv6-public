#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    int a = 1;
    
    int pid = fork();
    if(pid != 0) setnice(pid,0); 
    for(int i=1; i<=0x7FFFFFFF; i++) {
        for(int j=1; j<=0x7FFFFFFF; j++) {
            if(a == 1) a--;
            else a++;
        }
    }
    ps(0);
    if(pid != 0) wait();
    exit();
}