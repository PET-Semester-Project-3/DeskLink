#ifndef FLASHSTORAGE_H
#define FLASHSTORAGE_H

#include <cstdint>
#include <string>

#define ID_MAX_LEN 36 // UUID format is 36 chars


// Structure to hold data in flash
// Contains magic in order to ensure that the data is valid and not corrupted / never saved
struct FlashData 
{
    uint32_t magic;
    char last_id[ID_MAX_LEN + 1]; // +1 for null terminator
};


// Save last_id to flash
void save_last_id(const std::string& id);

// Load last_id from flash
std::string load_last_id();

#endif