#include "syscall.h"

int main() {
	Exec("exit.noff");
	Exec("../test/halt.noff");
    	Exit(0);
}
