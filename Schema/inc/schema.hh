#ifndef SCHEMA__HH
#define SCHEMA__HH

#include <CLI.hh>
#include <OFRI.hh>
#include <retcode.hh>

#include <vector>
#include <fstream>
#include <sstream>
#include <ObjectSchema.hh>
#include <Logger.hh>

RETCODE GenerateObjectDBFiles(const OBJECT& objectName,
    const std::string& skm_path,
    const std::string& inc_path,
    std::ofstream& dbMapStream,
    std::ofstream& dbMapPyStream,
    bool strict = false);

RETCODE GenerateAllDBFiles(const std::string& skm_path,
    const std::string& inc_path,
    const std::string& py_path,
    bool strict);

static const size_t WORD_SIZE = __WORDSIZE >> 3; // Number of bytes in a word

#endif