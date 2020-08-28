#include "BoxTonies.h"

void BoxTonies::loadTonieByUid(uint8_t uid[8]) {
    uint8_t* path;

    asprintf(
        (char**)&path,
        "%s%02X%02X%02X%02X/%02X%02X%02X%02X",
        CONTENT_BASE,
        uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]
    );

    loadTonieByPath(path);
}

void BoxTonies::loadTonieByPath(uint8_t* path) {
    Log.info("Loading Tonie from path %s...", path);
    
    if (tonieFile.open((char*)path, FA_OPEN_EXISTING | FA_READ)) {
        uint8_t buffer[512]; //TODO: buffer >512 size may scramble the stream 4096 block needed
        uint32_t read = tonieFile.read(buffer, sizeof(buffer));
        uint16_t bufferLen = read;
        if (read > 0) {
            uint16_t cursor = 0;
            uint8_t readBytes;

            uint8_t magicBytes[] = { 0x00, 0x00, 0x0F, 0xFC };//==4092 which is the length of the protobuf block
            
            if (memcmp(magicBytes, buffer, 4) == 0) {
                cursor += 4;
                while (cursor < bufferLen-1) {
                    uint8_t fieldId = buffer[cursor]>>3;
                    uint8_t fieldType = buffer[cursor]&0b00000111;
                    cursor++;
                    /*
                    switch (fieldId) {
                    case 0: //Variant
                        break;
                    case 1: //Fixed64
                        break;
                    case 2: //Length-delimited
                        break;
                    case 5: //Fixed32
                        break;
                    
                    default:
                        break;
                    }*/

                    if (fieldId == 1 && fieldType == 2) { //Audio data SHA-1 hash
                        uint64_t size = readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                        cursor += readBytes;
                        if (size == 20) {
                            memcpy(&header.hash[0], &buffer[cursor], size);
                            cursor += size;
                        } else {
                            Log.error("... hash length should be 20 but is %i", size);
                            break;
                        }
                    } else if (fieldId == 2 && fieldType == 0) { //Audio data length in bytes
                        header.audioLength = (uint32_t)readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                        cursor += readBytes;
                    } else if (fieldId == 3 && fieldType == 0) { //Audio-ID of OGG audio file, which is the unix time stamp of file creation
                        header.audioId = (uint32_t)readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                        cursor += readBytes;
                    } else if (fieldId == 4 && fieldType == 2) { //[array of variant] Ogg page numbers for Chapters
                        header.audioChapterCount = (uint8_t)readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                        cursor += readBytes;
                        header.audioChapters = new uint32_t[header.audioChapterCount]; //TODO! clear mem
                        for (uint8_t i=0; i<header.audioChapterCount; i++) {
                            uint32_t chapter = (uint32_t)readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                            cursor += readBytes;
                            header.audioChapters[i] = chapter;
                        }
                    } else if (fieldId == 5 && fieldType == 2) { //fill bytes „00“ up to <header_len>
                        uint64_t fillByteCount = readVariant(&buffer[cursor], bufferLen-cursor, readBytes);
                        cursor += readBytes;

                        if (fillByteCount + cursor != 4096)
                            Log.error("... Header length should be 4096 but is %i", fillByteCount + cursor);
                        
                        break; //everything read.
                    } else {
                        Log.error("... found unexpected protobuf field with id=%i and type=%i", fieldId, fieldType);
                        //clear header
                        break;
                    }
                    logTonieHeader();
                }
            } else {
                Log.error("... unexpected beginning of file %X %X %X %X", buffer[0], buffer[1], buffer[2], buffer[3]);
            }
        } else {
            Log.error("... could not data from file.");
        }
        tonieFile.close();
    } else {
        Log.error("... could not open Tonie.");
    }
}

uint64_t BoxTonies::readVariant(uint8_t* buffer, uint16_t length, uint8_t& readBytes) {
    uint64_t ret = 0;
    readBytes = 0;
    while (readBytes<length) {
        uint8_t data = buffer[readBytes];

        ret |= ((uint64_t)(data & 0x7F)) << (7 * readBytes);

        readBytes++;
        if ((data & 0x80) == 0)
            break;
    };

    return ret;
}

void BoxTonies::logTonieHeader() {
    Log.info("Tonie Header");
    Log.disableNewline(true);
    Log.info(" Hash:");
    for (uint8_t i = 0; i < 20; i++){
        Log.printf(" %x", header.hash[i]);
    }
    Log.disableNewline(false);
    Log.println();
    Log.info(" Length: %ib", header.audioLength);
    Log.info(" ID: %i", header.audioId);
    Log.info(" Chapters: %i", header.audioChapterCount);
    for (uint8_t i = 0; i < header.audioChapterCount; i++){
        Log.info("  %i: %i", i+1, header.audioChapters[i]);
    }
    Log.println();
}