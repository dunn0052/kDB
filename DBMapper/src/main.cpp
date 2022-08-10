#include <Database.hh>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <test.h>

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

    O_TEST* testobj = test.GetObj<O_TEST>(db, 10);
    strcpy(testobj->TEST_3 ,"abcd");
    std::cout << "Got object RECORD: " << testobj->rec
                   << "\nTEST_3 value: " << testobj->TEST_3;
}