#include "FlashStorage.h"
#include "GlobalVariables.h"
#include "WebConnection.h"


#include "FlashStorage.h"
#include "GlobalVariables.h"
#include "WebConnection.h"

void save_last_id(const std::string& id)
{
    FlashData data;
    data.magic = MAGIC;
    strncpy(data.last_id, id.c_str(), ID_MAX_LEN);
    data.last_id[ID_MAX_LEN] = '\0'; // ensure null termination

    uint32_t ints = save_and_disable_interrupts();

    // Erase a full sector (4 KB)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write exactly sizeof(FlashData) bytes
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&data, sizeof(FlashData));

    restore_interrupts(ints);

    printf("Saved last_id=%s to flash.\n", id.c_str());
}

std::string load_last_id()
{
    const FlashData* data = reinterpret_cast<const FlashData*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // If flash never written → magic is 0xFFFFFFFF or garbage
    if (data->magic != MAGIC)
    {
        printf("Flash uninitialized, using last_id=\"\".\n");
        return "";
    }

    printf("Flash OK, loaded last_id=%s\n", data->last_id);
    return std::string(data->last_id);
}