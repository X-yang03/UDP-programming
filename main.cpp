#include "UDP programming.h"

void print_menu() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    char menu[100] = { 0 };
    strcpy(menu, intro);
    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO bInfo;
    GetConsoleScreenBufferInfo(hOutput, &bInfo);//��ȡ���ڳ���
    int len = bInfo.dwSize.X / 2 - strlen(menu) / 2;//�ն��ٸ���
    std::cout << std::setw(len) << " " << menu << std::endl;
    std::cout << std::setw(len) << " " << "First start a Recieve end , Then start a Send end!" << std::endl;
}

int main()
{
    print_menu();

    char com[10];
    while (true) {
        std::cin >> com;
        if (strcmp(com, "send") == 0) {
            _Server s;
            s.start_server();
            system("pause");
            
        }
        else if (strcmp(com, "recv") == 0) {
            _Client c;
            c.client_init();
            system("pause");
        }
    }

    return 0;
}