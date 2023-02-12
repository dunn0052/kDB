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
    const std::string& inc_path);

RETCODE GenerateAllDBFiles(const std::string& skm_path,
    const std::string& inc_path);

#endif