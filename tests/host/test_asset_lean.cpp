/* The file and asset slots of a program that registers a few things (saturn/asset_memory.h): the library's table
 * (src/storage/files/asset_memory.cpp, 32 + 16 + 32 + 32 + 4 slots) is not linked. Past its own capacity the program
 * gets SAT_ERR_CAPACITY, as past the default; the scenarios of test_file_asset_logic.cpp that need more slots are
 * not run. */
#define SAT_TEST_SMALL_ASSET_MEMORY 1
#include "test_file_asset_logic.cpp"
