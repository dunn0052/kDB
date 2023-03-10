#ifndef __CONSTANTS_HH
#define __CONSTANTS_HH

#include <string>

static const std::string SKM_EXT = ".skm";
static const std::string HEADER_EXT = ".hh";
static const std::string PY_EXT = ".py";
static const std::string DB_EXT = ".db";

static const std::string COMMON_INC_PATH = "common_inc/";
static const std::string PYTHON_API_PATH = "PythonAPI/";
static const std::string ALL_DB_HEADER_NAME = "allDBs";
static const std::string DB_MAP_HEADER_NAME = "DBMap";
static const std::string DB_DIR = "db/";
static const std::string DB_INC_DIR = DB_DIR + "inc/";
static const std::string DB_SKM_DIR = DB_DIR + "skm/";
static const std::string DB_PY_DIR = PYTHON_API_PATH + DB_DIR;
static const std::string DB_DB_DIR = DB_DIR + "db/";

static const std::string KDB_INSTALL_DIR = "KDB_INSTALL_DIR";
static const std::string KDB_INET_ADDRESS = "KDB_INET_ADDRESS";
static const std::string KDB_INET_PORT = "KDB_INET_PORT";

#endif