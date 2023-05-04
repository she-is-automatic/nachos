// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "time.h"
// #include "fstest.cc"
#include <ctype.h>

#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"


extern char* EraseStrChar(char* str, char c);
extern char* setStrLength(char* str, int len);
extern char* numFormat(int num);
extern char* itostr(int num);
extern char* numFormat(int num);

extern void Append(char *from, char *to, int half);
extern void Copy(char *from, char *to);
extern void NAppend(char *from, char *to);
extern void Print(char *name);
extern void NCopy(char *from, char *to);
extern void PerformanceTest();



// 辅助函数
//-------------------------------------------------------------------------
// delete all char 'c' in str
//
//-------------------------------------------------------------------------
char buf[128];
char *EraseStrChar(char *str, char c)
{
    int i = 0;
    // printf("str=%s\n",str);
    while (*str != '\0')
    {
        if (*str != c)
        {
            buf[i] = *str;
            str++;
            i++;
        } // if
        else
            str++;
    } // while
    buf[i] = '\0';
    // printf("buf=%s\n",buf);
    return buf;
}
//---------------------------------------------------------------
//
//  Convert integer to str
//
//----------------------------------------------------------------
char istr[10] = "";
char *itostr(int num)
{
    int n;
    char ns[10];

    int k = 0;
    while (true)
    {
        n = num % 10;
        ns[k] = 0x30 + n;
        k++;
        num = num / 10;
        if (num == 0)
            break;
    }

    for (int i = 0; i < k; i++)
        istr[i] = ns[k - i - 1];

    // printf("str=%s\n",str);
    return istr;
}
//---------------------------------------------------------------
//
//  set a string to fix length, with blank append at the end of it,
//  or truncate it to the fix length
//
//
//   parametors:
//        str, len
//
//---------------------------------------------------------------
char strSpace[10];
// 字符串str固定长度为len，后补空格
char *setStrLength(char *str, int len)
{
    // printf("str = %s\n",str);
    int strLength = strlen(str);

    if (strLength >= len)
    {
        for (int i = 0; i < len; i++, str++)
            strSpace[i] = *str;
    }
    else
    {
        for (int i = 0; i < strLength; i++, str++)
            strSpace[i] = *str;

        for (int i = strLength; i < len; i++)
            strSpace[i] = ' ';
    }
    strSpace[len] = '\0';
    // printf("strSpace = %s, len=%d\n",strSpace,strlen(strSpace));
    return strSpace;
}

//-----------------------------------------------------------------
//
// divide an integer with ",", such as int (1,234,567) to char * (1,234,567)
//
//-----------------------------------------------------------------
char numStr[10];
char *numFormat(int num)
{
    numStr[0] = '\0';
    char nstr[10] = "";
    int k = 0;
    while (true)
    {
        nstr[k++] = num % 10 + 0x30;
        num = num / 10;
        if (num == 0)
            break;
    }
    nstr[k] = '\0';

    int j = k - 1;
    if (strlen(nstr) >= 4 && strlen(nstr) <= 6)
        j = k;
    else if (strlen(nstr) >= 7 && strlen(nstr) <= 9)
        j = k + 1;
    else if (strlen(nstr) >= 10 && strlen(nstr) <= 12)
        j = k + 2;

    numStr[j + 1] = '\0';
    for (int i = 0; i < k; i++)
    {
        if (i > 0 && i % 3 == 0)
        {
            numStr[j--] = ',';
            numStr[j--] = nstr[i];
        }
        else
            numStr[j--] = nstr[i];
    }
    // printf("numStr= %s\n",numStr);
    return numStr;
}
//--------------------------------------------------------

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------
// PC自增
void AdvancePC()
{
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));         // 前一个PC
    machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));         // 当前PC
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4); // 下一个PC
}
// 作为新应用程序进程所关联的核心线程的执行代码
void StartProcess(int spaceId)
{
    ASSERT(currentThread->userProgramId() == spaceId);
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , StartProcess!\n", spaceId);

    currentThread->space->InitRegisters(); // 初始化寄存器
    currentThread->space->RestoreState();  // 加载页表到Machine中
    machine->Run();                        // 执行应用程序

    ASSERT(FALSE);
}
// 作为子进程所关联的核心线程的执行代码
void StartProcessFork(int spaceId)
{
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , StartForkProcess!\n", spaceId);
    printf("Childthread begin id=%d \n", (currentThread->space)->getSpaceId());
    if (currentThread->space != NULL)
    {
        currentThread->RestoreUserState();    // 恢复寄存器
        currentThread->space->RestoreState(); // 加载页表寄存器
    }
    else
    {
        printf("Childthread begin space =NULL\n");
    }
    AdvancePC();
    machine->Run();
}

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2); // 从2号寄存器取得系统调用号

    if (which == SyscallException)
    {
        switch (type)
        {
        case SC_Halt: // 停机指令
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Halt\n" NONE, (currentThread->space)->getSpaceId());

            DEBUG('a', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;
        }
        case SC_Exit: // 退出指令，返回退出码
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Exit\033[m\n", (currentThread->space)->getSpaceId());

            int exitCode = machine->ReadRegister(4); // 从4号寄存器取出退出码
            machine->WriteRegister(2, exitCode);     // 返回值为退出码

            currentThread->setExitCode(exitCode); // 设置当前进程退出码

            // 释放该进程的内存空间及其页表，释放分配给该进程的实页（帧）
            delete currentThread->space;
            // 结束该进程对应的线程
            currentThread->Finish();
            AdvancePC();
            break;
        }

        case SC_Exec:
        {
            printf("\n\033[0;32;34mCurrentThreadPID: %d , SC_Exec\033[m\n", (currentThread->space)->getSpaceId());

            // read argument (i.e. filename) of Exec(filename)
            char filename[128];
            int addr = machine->ReadRegister(4);
            int ii = 0;
            // read filename from mainMemory
            do
            {
                machine->ReadMem(addr + ii, 1, (int *)&filename[ii]);
            } while (filename[ii++] != '\0');

            //---------------------------------------------------------
            //
            // inner commands --begin
            //
            //---------------------------------------------------------
            if (filename[0] == 'l' && filename[1] == 's') // ls，列出文件
            {
                printf("File(s) on Nachos DISK:\n");
                fileSystem->List();
                machine->WriteRegister(2, 127);
                AdvancePC();
                break;
            }
            else if (filename[0] == 'r' && filename[1] == 'm') // rm file
            {
                // 截取字符串，获取要删除的文件名
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = ' ';
                fn[1] = ' ';
                char *file = EraseStrChar(fn, ' ');

                // 文件名合法，在文件系统中删除该文件，返回
                if (file != NULL && strlen(file) > 0)
                {
                    fileSystem->Remove(file);
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("Remove: invalid file name.\n");
                    machine->WriteRegister(2, -1);  // 返回-1
                }
                AdvancePC();
                break;
            }                                                                        // rm
            else if (filename[0] == 'c' && filename[1] == 'a' && filename[2] == 't') // cat file
            {
                // 获取要输出的文件名
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = ' ';
                fn[1] = ' ';
                fn[2] = ' ';
                char *file = EraseStrChar(fn, ' ');
                
                if (file != NULL && strlen(file) > 0)
                {
                    Print(file);
                    printf("\n");
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("Cat: invalid file name.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            else if (filename[0] == 'c' && filename[1] == 'f') // create a nachos file
            {         
                // 获取文件名
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = ' ';
                fn[1] = ' ';
                char *file = EraseStrChar(fn, ' ');

                if (file != NULL && strlen(file) > 0)
                {
                    fileSystem->Create(file, 0);
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("Create: invalid file name.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            else if (filename[0] == 'c' && filename[1] == 'p') // cp source dest
            {
                // 获取source和dest文件名
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                char source[128];
                char dest[128];
                int startPos = 3;
                int j = 0;
                int k = 0;
                for (int i = startPos; i < 128 /*, fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        source[j++] = fn[i];
                    else
                        break;
                source[j] = '\0';
                j++;

                for (int i = startPos + j; i < 128 /*,fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        dest[k++] = fn[i];
                    else
                        break;
                dest[k] = '\0';

                if (source == NULL || strlen(source) <= 0)  // source不合法
                {
                    printf("cp: invalid file name.\n");
                    machine->WriteRegister(2, -1);
                    AdvancePC();
                    break;
                }
                if (dest != NULL && strlen(dest) > 0)   // source和dest都合法
                {
                    NCopy(source, dest);
                    machine->WriteRegister(2, 127);
                }
                else    // dest不合法
                {
                    printf("cp: invalid file name.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            // uap source(Unix) dest(nachos)
            else if ((filename[0] == 'u' && filename[1] == 'a' && filename[2] == 'p') || (filename[0] == 'u' && filename[1] == 'c' && filename[2] == 'p'))

            {
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = '#';
                fn[1] = '#';
                fn[2] = '#';
                fn[3] = '#';
                char source[128];
                char dest[128];
                int startPos = 4;
                int j = 0;
                int k = 0;
                for (int i = startPos; i < 128 /*, fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        source[j++] = fn[i];
                    else
                        break;
                source[j] = '\0';
                j++;

                for (int i = startPos + j; i < 128 /*,fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        dest[k++] = fn[i];
                    else
                        break;
                dest[k] = '\0';

                if (source == NULL || strlen(source) <= 0)
                {
                    printf("uap or ucp: Source file invalid file name.\n");
                    machine->WriteRegister(2, -1);
                    AdvancePC();
                    break;
                }
                if (dest != NULL && strlen(dest) > 0)
                {
                    if (filename[0] == 'u' && filename[1] == 'c' && filename[2] == 'p')
                        Copy(source, dest); // ucp
                    else
                        Append(source, dest, 0); // append dest file at the end of source file
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("uap or ucp: Dest file invalid file name.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            // nap source dest
            else if (filename[0] == 'n' && filename[1] == 'a' && filename[2] == 'p')
            {
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = '#';
                fn[1] = '#';
                fn[2] = '#';
                fn[3] = '#';

                char source[128];
                char dest[128];
                int j = 0;
                int k = 0;
                int startPos = 4;
                for (int i = startPos; i < 128 /*, fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        source[j++] = fn[i];
                    else
                        break;
                source[j] = '\0';
                j++;

                for (int i = startPos + j; i < 128 /*,fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        dest[k++] = fn[i];
                    else
                        break;
                dest[k] = '\0';

                if (source == NULL || strlen(source) <= 0)
                {
                    printf("nap: Source file invalid file name.\n");
                    machine->WriteRegister(2, -1);
                    AdvancePC();
                    break;
                }
                if (dest != NULL && strlen(dest) > 0)
                {
                    NAppend(source, dest);
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("ap: Dest file invalid file name.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            // rename source dest
            else if (filename[0] == 'r' && filename[1] == 'e' && filename[2] == 'n')
            {
                char fn[128];
                strncpy(fn, filename, strlen(filename) - 5);
                fn[strlen(filename) - 5] = '\0';
                fn[0] = '#';
                fn[1] = '#';
                fn[2] = '#';
                fn[3] = '#';
                char source[128];
                char dest[128];
                int j = 0;
                int k = 0;
                int startPos = 4;
                for (int i = startPos; i < 128 /*, fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        source[j++] = fn[i];
                    else
                        break;
                source[j] = '\0';
                j++;
                // printf("j = %d\n",j);

                for (int i = startPos + j; i < 128 /*,fn[i] != '\0'*/; i++)
                    if (fn[i] != ' ')
                        dest[k++] = fn[i];
                    else
                        break;
                dest[k] = '\0';

                if (source == NULL || strlen(source) <= 0)
                {
                    printf("rename: Source file not exists.\n");
                    machine->WriteRegister(2, -1);
                    AdvancePC();
                    break;
                }
                if (dest != NULL && strlen(dest) > 0)
                {
                    fileSystem->Rename(source, dest);
                    machine->WriteRegister(2, 127);
                }
                else
                {
                    printf("rename: Missing dest file.\n");
                    machine->WriteRegister(2, -1);
                }
                AdvancePC();
                break;
            }
            else if (strstr(filename, "format") != NULL) // 格式化磁盘
            {
                printf("strstr(filename,\"format\"=%s \n", strstr(filename, "format"));
                printf("WARNING: Format Nachos DISK will erase all the data on it.\n");
                printf("Do you want to continue (y/n)? ");
                char ch;
                while (true)
                {
                    ch = getchar();
                    if (toupper(ch) == 'Y' || toupper(ch) == 'N')
                        break;
                } // while
                if (toupper(ch) == 'N')
                {
                    machine->WriteRegister(2, 127); //
                    AdvancePC();
                    break;
                }

                printf("Format the DISK and create a file system on it.\n");
                fileSystem->FormatDisk(true);
                machine->WriteRegister(2, 127); //
                AdvancePC();
                break;
            }
            else if (strstr(filename, "fdisk") != NULL) // 查看磁盘内容
            {
                fileSystem->Print();
                machine->WriteRegister(2, 127); //
                AdvancePC();
                break;
            }
            else if (strstr(filename, "perf") != NULL) // Performance
            {
                PerformanceTest();
                machine->WriteRegister(2, 127); //
                AdvancePC();
                break;
            }
            else if (filename[0] == 'p' && filename[1] == 's') // ps
            {
                printf("CurrentThread -> ");
                currentThread->Print();
                scheduler->Print();
                machine->WriteRegister(2, 127); //
                AdvancePC();
                break;
            }
            else if (strstr(filename, "help") != NULL) // help
            {
                printf("Commands and Usage:\n");
                printf("ls                : list files on DISK.\n");
                printf("fdisk             : display DISK information.\n");
                printf("format            : format DISK with creating a file system on it.\n");
                printf("performence       : test DISK performence.\n");
                printf("cf  name          : create a file \"name\" on DISK.\n");
                printf("cp  source dest   : copy Nachos file \"source\" to \"dest\".\n");
                printf("nap source dest   : Append Nachos file \"source\" to \"dest\"\n");
                printf("ucp source dest   : Copy Unix file \"source\" to Nachos file \"dest\"\n");
                printf("uap source dest   : Append Unix file \"source\" to Nachos file \"dest\"\n");
                printf("cat name          : print content of file \"name\".\n");
                printf("rm  name          : remove file \"name\".\n");
                printf("rename source dest: Rename Nachos file \"source\" to \"dest\".\n");
                printf("ps                : display the system threads.\n");
                //-----------------------------------------------------------

                machine->WriteRegister(2, 127); //
                AdvancePC();
                break;
            }
            
            // 检查可执行文件的文件名是否合法，不允许有空格，必须以.noff结尾
            else // check if the parameter of exec(file), i.e file, is valid
            {
                if (strchr(filename, ' ') != NULL || strstr(filename, ".noff") == NULL)
                // not an inner command, and not a valid Nachos executable, then return
                {
                    machine->WriteRegister(2, -1);
                    AdvancePC();
                    break;
                }
            }
            //---------------------------------------------------------
            //
            // inner commands --end
            //
            //---------------------------------------------------------
            //---------------------------------------------------------
            //
            // loading an executable and execute it
            //
            //---------------------------------------------------------

            // call open() in FILESYS, see filesys.h
            // 打开文件
            OpenFile *executable = fileSystem->Open(filename);
            if (executable == NULL)
            {
                // printf("\nUnable to open file %s\n", filename);
                DEBUG('f', "\nUnable to open file %s\n", filename);
                machine->WriteRegister(2, -1);
                AdvancePC();
                break;
            }

            // 分配内存
            // new address space
            AddrSpace* space = new AddrSpace(executable);
            delete executable; // close file

            DEBUG('H', "Execute system call Exec(\"%s\"), it's SpaceId(pid) = %d \n", filename, space->getSpaceId());
            
            // 新建线程forkedThreadName，让其执行startProcess
            // new and fork thread
            char *forkedThreadName = filename;
            char *fname = strrchr(filename, '/');
            if (fname != NULL) // there exists "/" in filename
                forkedThreadName = strtok(fname, "/");
            Thread *thread = new Thread(forkedThreadName);
            currentThread->ConnectThread(thread);
            thread->Fork(StartProcess, space->getSpaceId());
            thread->space = space;
            printf("user process \"%s(%d)\" map to kernel thread \" %s \"\n", filename, space->getSpaceId(), forkedThreadName);

            // return spaceID
            machine->WriteRegister(2, space->getSpaceId());
            
            AdvancePC();
            break;
        }

        case SC_Join: // 父进程等待子进程，返回等待的子进程的退出码
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Join\n" NONE, (currentThread->space)->getSpaceId());
            int SpaceId = machine->ReadRegister(4); // 获取参数spaceId
            currentThread->Join(SpaceId);

            // Joinee 的退出码 waitProcessExitCode 写入2号寄存器
            machine->WriteRegister(2, currentThread->waitExitCode());
            AdvancePC();
            break;
        }
        case SC_Yield:
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Yield\n" NONE, (currentThread->space)->getSpaceId());

            currentThread->Yield();
            AdvancePC();
            break;
        }
        case SC_Fork: // 创建子进程，子进程返回值为0，父进程返回值为子进程pid
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Fork\n" NONE, (currentThread->space)->getSpaceId());

            // 创建子进程，构建进程树
            Thread *thread = new Thread("new thread");
            currentThread->ConnectThread(thread);

            // 复制父进程地址空间
            thread->space = new AddrSpace(thread->GetParentThread()->space);
            printf("My ChildThread PID= %d\n", thread->space->getSpaceId());
            printf("PageTable of FatherThread:\n");
            currentThread->space->Print(); // 输出该作业的页表信息
            printf("PageTable of ChildThread:\n");
            thread->space->Print(); // 输出子线程的页表信息

            // 复制父进程所有的寄存器,以便于与父进程在同一地址开始执行
            for (int i = 0; i < NumTotalRegs; i++)
                thread->setuserRegisters(i, machine->ReadRegister(i));

            // 子进程fork的返回值为0
            thread->setuserRegisters(2, 0);
            // 启动子进程
            thread->Fork(StartProcessFork, thread->space->getSpaceId());

            // 父进程fork返回值为子进程号
            machine->WriteRegister(2, thread->space->getSpaceId());
            AdvancePC();
            break;
        }
        case SC_Time: // 系统时间
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Time\n" NONE, (currentThread->space)->getSpaceId());

            time_t timep;
            time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
            printf("--------------------------------------------\n");
            printf("%s", ctime(&timep)); // 用ctime将秒数转化成字符串格式
            printf("--------------------------------------------\n");
            AdvancePC();
            break;
        }
        case SC_Create: // 创建文件
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Create\n" NONE, (currentThread->space)->getSpaceId());

            // 从4号寄存器读入参数文件名
            int base = machine->ReadRegister(4);
            int value;
            int count = 0;
            char *fileName = new char[128];
            do
            {
                machine->ReadMem(base + count, 1, &value);
                fileName[count] = *(char *)&value;
                count++;
            } while (*(char *)&value != '\0' && count < 128);

            // when calling Create(),  thread go to sleep, waked up when I/O finish
            if (!fileSystem->Create(fileName, 0)) // call Create() in FILESYS,see filesys.h
                printf("create file %s failed!\n", fileName);
            else
                DEBUG('f', "create file %s succeed!\n", fileName);

            // PC+1
            AdvancePC();
            break;
        }
        case SC_Open: // 打开文件，返回文件描述符fd
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Open\n" NONE, (currentThread->space)->getSpaceId());

            // 从4号寄存器读入参数文件名
            int base = machine->ReadRegister(4);
            int value;
            int count = 0;
            char *fileName = new char[128];
            do
            {
                machine->ReadMem(base + count, 1, &value);
                fileName[count] = *(char *)&value;
                count++;
            } while (*(char *)&value != '\0' && count < 128);

            int fileid;
            // call Open() in FILESYS,see filesys.h,Nachos Open()
            OpenFile *openfile = fileSystem->Open(fileName);
            if (openfile == NULL)
            { // file not existes, not found
                printf("File \"%s\" not Exists, could not open it.\n", fileName);
                fileid = -1;
            }
            else
            { // file found
                // set the opened file id in AddrSpace, which wiil be used in Read() and Write()
                fileid = currentThread->space->getFileDescriptor(openfile);
                if (fileid < 0)
                    printf("Too many files opened!\n");
                else
                    DEBUG('f', "file :%s open secceed!  the file id is %d\n", fileName, fileid);
            }
            machine->WriteRegister(2, fileid);
            AdvancePC();
            break;
        }
        case SC_Write: // 将内容写入文件，返回写入的字节数
        {
            int base = machine->ReadRegister(4);   // buffer
            int size = machine->ReadRegister(5);   // bytes written to file
            int fileId = machine->ReadRegister(6); // fd
            int value;
            int count = 0;

            // 将要写入的内容放入buffer
            char *buffer = new char[128];
            do
            {
                machine->ReadMem(base + count, 1, &value);
                buffer[count] = *(char *)&value;
                count++;
            } while ((*(char *)&value != '\0') && (count < size));
            buffer[size] = '\0';

            // 根据fileId打开要写入的文件
            OpenFile *openfile = currentThread->space->getFileId(fileId);
            // printf("openfile =%d\n",openfile);
            if (openfile == NULL)
            {
                printf("Failed to Open file \"%d\" .\n", fileId);
                AdvancePC();
                break;
            }

            // stdout 和 stderr 都打印到屏幕
            if (fileId == 1 || fileId == 2)
            {
                openfile->WriteStdout(buffer, size);
                delete[] buffer;
                AdvancePC();
                break;
            }

            // 要写入的位置
            int WritePosition = openfile->Length();
            openfile->Seek(WritePosition); // append write

            int writtenBytes;
            // writtenBytes = openfile->AppendWriteAt(buffer,size,WritePosition);
            writtenBytes = openfile->Write(buffer, size);
            if ((writtenBytes) == 0)
                DEBUG('f', "\nWrite file failed!\n");
            else
            {
                if (fileId != 1 && fileId != 2)
                {
                    DEBUG('f', "\n\"%s\" has wrote in file %d succeed!\n", buffer, fileId);
                    DEBUG('H', "\n\"%s\" has wrote in file %d succeed!\n", buffer, fileId);
                    DEBUG('J', "\n\"%s\" has wrote in file %d succeed!\n", buffer, fileId);
                }
                // printf("\n\"%s\" has wrote in file %d succeed!\n",buffer,fileId);
            }

            // delete openfile;
            delete[] buffer;
            AdvancePC();
            break;
        }
        case SC_Read: // 从文件中读取数据，返回读出的字节数
        {
            int base = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int fileId = machine->ReadRegister(6);

            OpenFile *openfile = currentThread->space->getFileId(fileId);

            // 从文件读入buffer
            char buffer[size];
            int readnum = 0;
            if (fileId == 0) // stdin
                readnum = openfile->ReadStdin(buffer, size);
            else
                readnum = openfile->Read(buffer, size);

            // 从buffer写回主存
            for (int i = 0; i < readnum; i++)
                machine->WriteMem(base, 1, buffer[i]);
            buffer[readnum] = '\0';

            /************************************************  DEBUG用  ****************************************************/
            /*****************************************  数字0-9转换成字符0-9 ************************************************/
            for (int i = 0; i < readnum; i++)
                if (buffer[i] >= 0 && buffer[i] <= 9)
                    buffer[i] = buffer[i] + 0x30;

            char *buf = buffer;
            if (readnum > 0)
            {
                if (fileId != 0)
                {
                    DEBUG('f', "Read file (%d) succeed! the content is \"%s\" , the length is %d\n", fileId, buf, readnum);
                    DEBUG('H', "Read file (%d) succeed! the content is \"%s\" , the length is %d\n", fileId, buf, readnum);
                    DEBUG('J', "Read file (%d) succeed! the content is \"%s\" , the length is %d\n", fileId, buf, readnum);
                }
            }
            else
                printf("\nRead file failed!\n");

            /************************************************************************************************************/
            machine->WriteRegister(2, readnum);
            AdvancePC();
            break;
        }
        case SC_Close: // 关闭文件
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Close\n" NONE, (currentThread->space)->getSpaceId());

            int fileId = machine->ReadRegister(4);
            OpenFile *openfile = currentThread->space->getFileId(fileId);
            if (openfile != NULL)
            {
                // 文件头写回磁盘
                openfile->WriteBack(); // write file header back to DISK

                delete openfile; // close file

                // 在打开文件表中释放文件描述符
                currentThread->space->releaseFileDescriptor(fileId);

                DEBUG('f', "File %d  closed succeed!\n", fileId);
                DEBUG('H', "File %d  closed succeed!\n", fileId);
                DEBUG('J', "File %d  closed succeed!\n", fileId);
            }
            else
                printf("Failded to Close File %d.\n", fileId);
            AdvancePC();
            break;
        }
        }
    }
    else
    {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
