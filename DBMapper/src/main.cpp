#include <Database.hh>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <test.hh>

int main()
{
    RETCODE retcode = RTN_OK;

    Database test = Database();
    DATABASE db = "test";

    retcode |= test.Open(db);

    if(RTN_OK == retcode )
    {
        std::cout << "Opened test successfully!\n";
    }
    else
    {
        std::cout << "Failed to open db! Exiting.\n";
        return 1;
    }

    O_TEST_1* testobj = test.GetObj<O_TEST_1>(db, 10);
    strcpy(testobj->TEST_3 ,"abcd");
    std::cout << "Got object RECORD: " << testobj->record
                   << "\nTEST_3 value: " << testobj->TEST_3;
}