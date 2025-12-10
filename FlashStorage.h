#ifndef FLASHSTORAGE_H
#define FLASHSTORAGE_H

#include <cstdint>

// Structure to hold data in flash
// Contains magic in order to ensure that the data is valid and not corrupted / never saved
struct FlashData 
{
    uint32_t magic;
    uint32_t last_id;
};


// Save last_id to flash
void save_last_id(uint32_t id);

// Load last_id from flash
uint32_t load_last_id();



#endif