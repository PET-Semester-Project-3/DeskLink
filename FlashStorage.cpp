#include "FlashStorage.h"
#include "GlobalVariables.h"
#include "WebConnection.h"


void save_last_id(uint32_t id)
{
    FlashData data;
    data.magic = MAGIC;
    data.last_id = id;

    uint32_t ints = save_and_disable_interrupts();

    // Erase a full sector (4 KB)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write exactly sizeof(FlashData) bytes
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&data, sizeof(FlashData));

    restore_interrupts(ints);

    printf("Saved last_id=%u to flash.\n", id);
}


uint32_t load_last_id()
{
    const FlashData* data = reinterpret_cast<const FlashData*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // If flash never written → magic is 0xFFFFFFFF or garbage
    if (data->magic != MAGIC)
    {
        printf("Flash uninitialized, using last_id=-1.\n");
        return -1;
    }

    printf("Flash OK, loaded last_id=%u\n", data->last_id);
    return data->last_id;
}