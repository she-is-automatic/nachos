#include "syscall.h"

int main() {
    SpaceId newProc;
    OpenFileId Fp;
    char buffer[50];	// 读出的数据
    int sz;				// 读出数据大小
    // 创建文件 
    Create("Ftest");
    // 打开文件
    Fp = Open("Ftest");
    // 写文件
    Write("exit.noffhello nachos!",15,Fp);
    // 读文件
    sz = Read(buffer,9,Fp);
    // 关闭文件
    Close(Fp);
    buffer[9]='\0';
    newProc=Exec(buffer);
    Join(newProc);
    // 退出程序
    Exit(0);
}
