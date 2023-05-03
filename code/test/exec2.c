#include "syscall.h"

int main() {
	Exec("../test/exit.noff");
	Exit(1);
}