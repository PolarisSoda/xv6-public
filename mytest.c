#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    int a = 1;
    
    int pid = fork();
    if(pid != 0) setnice(pid,0); 
    for(int i=1; i<=100000000; i++) for(int j=1; j<=100000000; j++) a++;
    printf(1,"%d : \n",a);
    ps(0);
    exit();
}