#include "UDP programming.h"

int main()
{
    printf("Type 'send' to start as server\n Type 'recv' to start as client\n");

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