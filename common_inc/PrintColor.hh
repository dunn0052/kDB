#ifndef PRINT_COLOR_HH
#define PRINT_COLOR_HH

#include <sstream>
namespace Color
{
    enum Code
    {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_YELLOW   = 33,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_YELLOW   = 43,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };

    class Modifier
    {
        Code m_Code;
    public:

        Modifier(Code code)
            : m_Code(code)
            {}

        friend std::ostream&
        operator<<(std::ostream& os, const Modifier& mod)
        {
            return os << "\033[" << mod.m_Code << "m";
        }
    };
}

#endif