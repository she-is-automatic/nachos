#include "syscall.h"

int main() {
    SpaceId newProc = Exec("exit.noff");
    Join(newProc);
    Exit(0);
}

