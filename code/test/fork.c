#include "syscall.h"

int main() {
	OpenFileId Fp;
	SpaceId newProc = Fork();
	if(newProc==0)
	{
		Create("Ftest");
    		// 打开文件
    		Fp= Open("Ftest");
    		// 写文件
    		Write("hello",5,Fp);
		Close(Fp);
	}
	else
	{
		Join(newProc);
		Fp = Open("Ftest");
    		Write("world",5,Fp);
		Close(Fp);
	}
    	Exit(0);
}
