#include "syscall.h"

int
main()
{
    SpaceId newProc;
    char prompt[8], ch, buffer[60];
    int i;
    int output;
    int input;

    Create("OutputFile");
    output = Open("OutputFile");
    prompt[0] = 'N';
    prompt[1] = 'a';
    prompt[2] = 'c';
    prompt[3] = 'h';
    prompt[4] = 'o';
    prompt[5] = 's';
    prompt[6] = '$';
    prompt[7] = ' ';
    Write(prompt,8,output);
    Close(output);
    
    input=Open("InputFile");
   
    i=17;
   Read(buffer,17,input);
    Close(input);
    buffer[i]='\0';
    if(i>0){
	Halt();
        newProc=Exec(buffer);
        if(newProc>=0){
	    Join(newProc);	
	}
}
}

