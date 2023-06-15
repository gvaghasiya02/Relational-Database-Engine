#include <cmath>
#include "rbfm.h"
#include <stdlib.h>
#include "iostream"
#include <string.h>
#include <cassert>

unsigned sizeOfSlotCount = sizeof(unsigned);
unsigned sizeOfSlotNum = sizeof(unsigned);
unsigned sizeOfNextRecordWritePtr = sizeof(unsigned);
// nextRecordWritePtrOffset tells from which byte the recordWritePtr is stored
unsigned nextRecordWritePtrOffset = PAGE_SIZE - sizeOfSlotCount - sizeOfNextRecordWritePtr;

RecordBasedFileManager *RecordBasedFileManager::_rbf_manager = 0;
PagedFileManager *pfm = PagedFileManager::instance();
RecordBasedFileManager *RecordBasedFileManager::instance()
{
    if (!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

unsigned getSlotCount(const void *pageData)
{
    // last 4 bytes store slotCount
    unsigned slotCount;
    memcpy((char *)&slotCount, (char *)pageData + PAGE_SIZE - sizeOfSlotCount, sizeOfSlotCount);
    return slotCount;
}

bool fitInPage(unsigned nextRecordWritePtr, unsigned slotCount, unsigned recordSize)
{
    // returns true if record can fit in the page
    unsigned slotOffset = PAGE_SIZE - sizeOfSlotCount - sizeOfNextRecordWritePtr - slotCount * sizeOfSlotNum;
    if ((slotOffset - nextRecordWritePtr) > recordSize + sizeOfSlotNum)
    {
        return true;
    }
    return false;
}

void setSlotCount(const void *pageData, unsigned slotCount)
{
    // last 4 bytes store slotCount
    *(unsigned *)((char *)pageData + PAGE_SIZE - sizeOfSlotCount) = slotCount;
}

void setRIDPointer(const void *pageData, int ridPointer, int slotNumNew, int pageNumNew, int recordOffset)
{
    *(int *)((char *)pageData + recordOffset) = ridPointer;
    *(int *)((char *)pageData + recordOffset + sizeof(int)) = pageNumNew;
    *(int *)((char *)pageData + recordOffset + 2 * sizeof(int)) = slotNumNew;
}
void setRecordOffsetInSlotNum(void *pageData, unsigned slotCount, int recordOffset)
{
    // this data is stored in the slot number and it tells from which offset the record read starts from
    unsigned slotNumOffset = PAGE_SIZE - sizeOfSlotCount - sizeOfNextRecordWritePtr - (slotCount)*sizeOfSlotCount;
    *(int *)((char *)pageData + slotNumOffset) = recordOffset;
}

unsigned getNextRecordWritePtr(const void *pageData)
{
    // last 8 bytes to last 4 bytes store offset saying where to start writing next record
    unsigned ptr;
    memcpy((char *)&ptr, (char *)pageData + nextRecordWritePtrOffset, sizeOfNextRecordWritePtr);
    return ptr;
}

void setNextRecordWriteOffset(const void *pageData, unsigned offset)
{
    *(unsigned *)((char *)pageData + nextRecordWritePtrOffset) = offset;
}

int getAttributesOffsetSize(const int numAttributes)
{
    // returns the number of bytes required to store the offset of all the attributes' offset
    return numAttributes * sizeof(unsigned);
}

int getBitMapSize(const int numAttributes)
{
    // returns the number of bytes required to store the bitmap for null values representation in the record
    return ceil(double(numAttributes) / CHAR_BIT);
}

int getRecordOffset(void *pageData, unsigned slotNum)
{
    unsigned slotNumbOffset = PAGE_SIZE - sizeOfSlotCount - sizeOfNextRecordWritePtr - (slotNum + 1) * sizeOfSlotCount;
    int ptr;
    memcpy((char *)&ptr, (char *)pageData + slotNumbOffset, sizeOfSlotNum);
    return ptr;
}

int getRecordDataSize(void *pageData, int recordOffset, int numAttributes)
{
    int recordSize;
    int attribOffsetSize = getAttributesOffsetSize(numAttributes);
    memcpy((char *)&recordSize, (char *)pageData + recordOffset + attribOffsetSize - sizeof(int), sizeof(int));
    return recordSize;
}

unsigned createAttrOffsetAndGetRecordSize(const vector<Attribute> &recordDescriptor, const void *data, void *attrOffset)
{
    const int numAttributes = recordDescriptor.size();
    int attributesOffsetSize = getAttributesOffsetSize(numAttributes); // to store the offset where each field ends in the record
    int bitMapSize = getBitMapSize(numAttributes);
    int recordDataSize = bitMapSize;

    for (int i = 0; i < numAttributes; i++)
    {
        AttrType attrType = recordDescriptor[i].type;
        string attrName = recordDescriptor[i].name;
        if (!(((char *)data)[i / 8] & 1 << (7 - i % 8)))
        { // if not null
            if (attrType == TypeVarChar)
            {
                recordDataSize += 4 + *(int *)((char *)data + recordDataSize);
            }
            else
            {
                // int and floats take same space :P
                recordDataSize += sizeof(int);
            }
        }
        *((unsigned *)attrOffset + i) = unsigned(recordDataSize);
    }
    //     attrOffsetSize bitMap
    //              |      |
    // formatted data:  |13 17 21 25|0|8Anteater 25 177.6 6620
    //                    bytes ->  0 1       13  17    21    25
    // recordDataSize = 25
    // recordSize = attributesOffsetSize + recordDataSize
    return attributesOffsetSize + recordDataSize;
}

unsigned shiftRecords(int shiftBytes, const void *pageData, int moveFirstPtr, int moveLastPtr)
{
    // shifts records by shiftBytes. Moves the records either left or right (direction='l', 'r')
    memmove((char *)pageData + moveFirstPtr + shiftBytes, (char *)pageData + moveFirstPtr, moveLastPtr - moveFirstPtr);
}

unsigned updateSlotPtr(int shiftBytes, void *pageData, int updateThreshold)
{
    // used to update record pointer in the slots after records have been shifted.
    // updateThreshold tells that only those pointers that is >= the updateThreshold
    // should be updated
    unsigned int slotCount = getSlotCount(pageData);
    int recordOffset;
    for (int i = 0; i < slotCount; i++)
    {
        recordOffset = getRecordOffset(pageData, i);
        if (recordOffset >= updateThreshold)
        {
            recordOffset = recordOffset + shiftBytes;
            setRecordOffsetInSlotNum(pageData, i + 1, recordOffset);
        }
    }
}

RC RecordBasedFileManager::createFile(const string &fileName)
{
    return pfm->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName)
{
    return pfm->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    return pfm->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle)
{
    return pfm->closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid)
{
    int numAttributes = recordDescriptor.size();
    int attrOffsetSize = getAttributesOffsetSize(numAttributes);
    int numPages = fileHandle.getNumberOfPages();

    void *attrOffset = malloc(attrOffsetSize);
    int recordSize = createAttrOffsetAndGetRecordSize(recordDescriptor, data, attrOffset);
    void *formattedRecord = malloc(recordSize);
    memcpy((char *)formattedRecord, attrOffset, attrOffsetSize);
    memcpy((char *)formattedRecord + attrOffsetSize, data, recordSize - attrOffsetSize);
    bool createNewPage = true;
    void *pageData = malloc(PAGE_SIZE);
    if (numPages == 0)
    {

        memset(pageData, 0, PAGE_SIZE);
        memcpy(pageData, formattedRecord, recordSize);
        unsigned slotCount = getSlotCount(pageData);
        unsigned nextWritePtr = getNextRecordWritePtr(pageData);
        slotCount += 1;
        setSlotCount(pageData, slotCount);
        setRecordOffsetInSlotNum(pageData, slotCount, nextWritePtr);
        nextWritePtr += unsigned(recordSize);
        setNextRecordWriteOffset(pageData, nextWritePtr);

        fileHandle.appendPage(pageData);
        rid.pageNum = 0;
        rid.slotNum = slotCount - 1;

        createNewPage = false;
    }
    else
    {
        for (int i = 0; i < numPages; i++)
        {

            memset(pageData, 0, PAGE_SIZE);
            fileHandle.readPage(i, pageData);
            unsigned nextWritePtr = getNextRecordWritePtr(pageData);
            unsigned slotCount = getSlotCount(pageData);
            bool fillExistingSlot = false;
            int writeSlotCount;
            if (fitInPage(nextWritePtr, slotCount, recordSize))
            {
                memcpy((char *)pageData + nextWritePtr, formattedRecord, recordSize);
                int recordOffset;
                for (int sc = 1; sc <= slotCount; sc++)
                {
                    recordOffset = getRecordOffset(pageData, sc - 1);
                    if (recordOffset == -1)
                    {
                        fillExistingSlot = true;
                        writeSlotCount = sc;
                        break;
                    }
                }
                if (!fillExistingSlot)
                {
                    writeSlotCount = slotCount + 1;
                    setSlotCount(pageData, writeSlotCount);
                }
                setRecordOffsetInSlotNum(pageData, writeSlotCount, nextWritePtr);
                nextWritePtr += unsigned(recordSize);
                setNextRecordWriteOffset(pageData, nextWritePtr);

                fileHandle.writePage(i, pageData);
                rid.pageNum = i;
                rid.slotNum = writeSlotCount - 1;

                createNewPage = false;
                break;
            }
        }
        if (createNewPage)
        {
            // if no place works; then append new page

            memset(pageData, 0, PAGE_SIZE);
            memcpy(pageData, formattedRecord, recordSize);
            unsigned slotCount = getSlotCount(pageData);
            unsigned nextWritePtr = getNextRecordWritePtr(pageData);
            slotCount += 1;
            setSlotCount(pageData, slotCount);
            setRecordOffsetInSlotNum(pageData, slotCount, nextWritePtr);
            nextWritePtr += unsigned(recordSize);
            setNextRecordWriteOffset(pageData, nextWritePtr);

            fileHandle.appendPage(pageData);
            rid.pageNum = numPages;
            rid.slotNum = slotCount - 1;
        }
    }
    free(formattedRecord);
    free(attrOffset);
    free(pageData);
    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data)
{
    void *page = malloc(PAGE_SIZE);
    assert(page);

    if (fileHandle.readPage(rid.pageNum, page) == -1)
        return -1;

    int recordOffset = getRecordOffset(page, rid.slotNum);
    if (recordOffset == -1)
    {
        free(page);
        return -1;
    }
    else
    {
        int ptr;
        memcpy((char *)&ptr, (char *)page + recordOffset, sizeof(int));
        if (ptr == -1)
        {
            int slotNum, pageNum;
            memcpy((char *)&pageNum, (char *)page + recordOffset + sizeof(int), sizeof(int));
            memcpy((char *)&slotNum, (char *)page + recordOffset + 2 * sizeof(int), sizeof(int));
            RID rid1;
            rid1.slotNum = slotNum;
            rid1.pageNum = pageNum;
            free(page);
            return readRecord(fileHandle, recordDescriptor, rid1, data);
        }
        else
        {
            int attribOffsetSize = getAttributesOffsetSize(recordDescriptor.size());
            int recordLength = getRecordDataSize(page, recordOffset, recordDescriptor.size());
            assert(recordOffset < PAGE_SIZE);
            assert(recordLength < PAGE_SIZE);
            memcpy(data, (char *)page + recordOffset + attribOffsetSize, recordLength);
            free(page);
            return 0;
        }
    }
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data)
{
    int numAttributes = recordDescriptor.size();
    int bitMapSize = getBitMapSize(numAttributes);
    int offsetCounter = bitMapSize;
    char bitMap[bitMapSize];

    memcpy(&bitMap, data, bitMapSize * sizeof(char));

    for (int i = 0; i < numAttributes; ++i)
    {
        const Attribute &attribute = recordDescriptor[i];
        if ((bitMap[i / 8] >> (7 - i % 8)) & 1)
        {
            cout << attribute.name << ":NULL ";
            continue;
        }
        if (attribute.type == TypeVarChar)
        {
            int varcharlength;
            memcpy(&varcharlength, (char *)data + offsetCounter, sizeof(int));
            offsetCounter += 4;
            char varchar[varcharlength + 1];
            memcpy(&varchar, (char *)data + offsetCounter, varcharlength);
            varchar[varcharlength] = '\0';
            offsetCounter += varcharlength;
            cout << attribute.name << ":" << varchar << " ";
        }
        else if (attribute.type == TypeInt)
        {
            int intValue;
            memcpy(&intValue, (char *)data + offsetCounter, sizeof(int));
            offsetCounter += sizeof(int);
            cout << attribute.name << ":" << intValue << " ";
        }
        else if (attribute.type == TypeReal)
        {
            float realValue;
            memcpy(&realValue, (char *)data + offsetCounter, sizeof(float));
            offsetCounter += sizeof(float);
            cout << attribute.name << ":" << realValue << " ";
        }
    }
    cout << endl;
    return 0;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid)
{

    void *pageData = malloc(PAGE_SIZE);
    fileHandle.readPage(rid.pageNum, pageData);
    int recordOffset = getRecordOffset(pageData, rid.slotNum);
    if (recordOffset == -1)
    {
        free(pageData);
        return -1;
    }
    int ptr;
    memcpy((char *)&ptr, (char *)pageData + recordOffset, sizeof(int));
    if (ptr == -1)
    {
        int slotNum, pageNum;
        memcpy((char *)&pageNum, (char *)pageData + recordOffset + sizeof(int), sizeof(int));
        memcpy((char *)&slotNum, (char *)pageData + recordOffset + 2 * sizeof(int), sizeof(int));
        RID rid1;
        rid1.slotNum = slotNum;
        rid1.pageNum = pageNum;
        free(pageData);
        return deleteRecord(fileHandle, recordDescriptor, rid1);
    }
    else
    {
        int numAttrib = recordDescriptor.size();
        int recordSize = getAttributesOffsetSize(numAttrib) + getRecordDataSize(pageData, recordOffset, numAttrib);
        int moveFirstPtr = recordOffset + recordSize;
        int moveLastPtr = getNextRecordWritePtr(pageData);
        shiftRecords(recordSize * -1, pageData, moveFirstPtr, moveLastPtr);
        setRecordOffsetInSlotNum(pageData, rid.slotNum + 1, -1);
        updateSlotPtr(recordSize * -1, pageData, recordOffset + recordSize);
        setNextRecordWriteOffset(pageData, moveLastPtr - recordSize);
        fileHandle.writePage(rid.pageNum, pageData);
        free(pageData);
        return 0;
    }
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid)
{
    // getting formatted record
    int numAttributes = recordDescriptor.size();
    int attrOffsetSize = getAttributesOffsetSize(numAttributes);
    void *attrOffset = malloc(attrOffsetSize);
    int recordSizeNew = createAttrOffsetAndGetRecordSize(recordDescriptor, data, attrOffset);
    void *formattedRecord = malloc(recordSizeNew);
    memcpy((char *)formattedRecord, attrOffset, attrOffsetSize);
    memcpy((char *)formattedRecord + attrOffsetSize, data, recordSizeNew - attrOffsetSize);

    // read old record
    void *page = malloc(PAGE_SIZE);
    assert(page);

    if (fileHandle.readPage(rid.pageNum, page) == -1)
    {
        free(page);
        return -1;
    }

    int recordOffset = getRecordOffset(page, rid.slotNum);
    if (recordOffset == -1)
    {
        free(page);
        return -1;
    }
    if (recordOffset >= 0)
    {
        int ptr;
        memcpy((char *)&ptr, (char *)page + recordOffset, sizeof(int));
        if (ptr == -1)
        {
            int slotNum, pageNum;
            memcpy((char *)&pageNum, (char *)page + recordOffset + sizeof(int), sizeof(int));
            memcpy((char *)&slotNum, (char *)page + recordOffset + 2 * sizeof(int), sizeof(int));
            RID rid1;
            rid1.slotNum = slotNum;
            rid1.pageNum = pageNum;
            int result = updateRecord(fileHandle, recordDescriptor, data, rid1);
            if (result == -1)
            {
                free(page);
                return -1;
            }
        }
        else
        {
            int attribOffsetSize = getAttributesOffsetSize(recordDescriptor.size());
            int recordSizeOld = getRecordDataSize(page, recordOffset, recordDescriptor.size()) + attribOffsetSize;
            if (recordSizeOld == recordSizeNew)
            {
                memcpy((char *)page + recordOffset, formattedRecord, recordSizeNew);
                fileHandle.writePage(rid.pageNum, page);
                free(page);
                free(attrOffset);
                free(formattedRecord);
                return 0;
            }
            else if (recordSizeOld > recordSizeNew)
            {
                memcpy((char *)page + recordOffset, formattedRecord, recordSizeNew);
                int moveFirstPtr = recordOffset + recordSizeOld; // point to old record
                int moveLastPtr = getNextRecordWritePtr(page);
                shiftRecords(recordSizeNew - recordSizeOld, page, moveFirstPtr, moveLastPtr);
                updateSlotPtr(recordSizeNew - recordSizeOld, page, recordOffset + recordSizeOld);
                setNextRecordWriteOffset(page, moveLastPtr + recordSizeNew - recordSizeOld);
                fileHandle.writePage(rid.pageNum, page);
                free(page);
                free(attrOffset);
                free(formattedRecord);
                return 0;
            }
            else
            {
                int nextWritePtr = getNextRecordWritePtr(page);
                int slotCount = getSlotCount(page);
                unsigned slotOffset = PAGE_SIZE - sizeOfSlotCount - sizeOfNextRecordWritePtr - slotCount * sizeOfSlotNum;
                if ((slotOffset - nextWritePtr) > recordSizeNew - recordSizeOld + sizeOfSlotNum)
                {
                    int moveFirstPtr = recordOffset + recordSizeOld;
                    int moveLastPtr = getNextRecordWritePtr(page);
                    shiftRecords(recordSizeOld * -1, page, moveFirstPtr, moveLastPtr);
                    setNextRecordWriteOffset(page, moveLastPtr - recordSizeOld + recordSizeNew);
                    updateSlotPtr(recordSizeOld * -1, page, recordOffset + recordSizeOld);
                    setRecordOffsetInSlotNum(page, rid.slotNum + 1, moveLastPtr - recordSizeOld);
                    memcpy((char *)page + moveLastPtr - recordSizeOld, formattedRecord, recordSizeNew);
                    fileHandle.writePage(rid.pageNum, page);
                    free(page);
                    free(attrOffset);
                    free(formattedRecord);
                    return 0;
                }
                else
                {
                    // if record doesn't fit in current page
                    bool createNewPage = true;
                    int numPages = fileHandle.getNumberOfPages();
                    void *pageData = malloc(PAGE_SIZE);
                    int slotNumNew, pageNumNew;
                    for (int i = 0; i < numPages; i++)
                    {

                        memset(pageData, 0, PAGE_SIZE);
                        fileHandle.readPage(i, pageData);
                        unsigned nextWritePtr = getNextRecordWritePtr(pageData);
                        unsigned slotCount = getSlotCount(pageData);
                        bool fillExistingSlot = false;
                        int writeSlotCount;
                        if (fitInPage(nextWritePtr, slotCount, recordSizeNew))
                        {
                            memcpy((char *)pageData + nextWritePtr, formattedRecord, recordSizeNew);
                            int recordOffset;
                            for (int sc = 1; sc <= slotCount; sc++)
                            {
                                recordOffset = getRecordOffset(pageData, sc - 1);
                                if (recordOffset == -1)
                                {
                                    fillExistingSlot = true;
                                    writeSlotCount = sc;
                                    break;
                                }
                            }
                            if (!fillExistingSlot)
                            {
                                writeSlotCount = slotCount + 1;
                                setSlotCount(pageData, writeSlotCount);
                            }
                            setRecordOffsetInSlotNum(pageData, writeSlotCount, nextWritePtr);
                            nextWritePtr += unsigned(recordSizeNew);
                            setNextRecordWriteOffset(pageData, nextWritePtr);

                            fileHandle.writePage(i, pageData);
                            pageNumNew = i;
                            slotNumNew = writeSlotCount - 1;

                            createNewPage = false;
                            break;
                        }
                    }
                    if (createNewPage)
                    {
                        // if no place works; then append new page

                        memset(pageData, 0, PAGE_SIZE);
                        memcpy(pageData, formattedRecord, recordSizeNew);
                        unsigned slotCount = getSlotCount(pageData);
                        unsigned nextWritePtr = getNextRecordWritePtr(pageData);
                        slotCount += 1;
                        setSlotCount(pageData, slotCount);
                        setRecordOffsetInSlotNum(pageData, slotCount, nextWritePtr);
                        nextWritePtr += unsigned(recordSizeNew);
                        setNextRecordWriteOffset(pageData, nextWritePtr);

                        fileHandle.appendPage(pageData);
                        pageNumNew = numPages;
                        slotNumNew = slotCount - 1;
                    }
                    free(pageData);
                    free(attrOffset);
                    free(formattedRecord);
                    int ridIndicator = -1;
                    setRIDPointer(page, ridIndicator, slotNumNew, pageNumNew, recordOffset);
                    int moveFirstPtr = recordOffset + recordSizeOld;
                    int moveLastPtr = getNextRecordWritePtr(page);
                    shiftRecords(3 * sizeof(int) - recordSizeOld, page, moveFirstPtr, moveLastPtr);
                    setNextRecordWriteOffset(page, moveLastPtr - recordSizeOld + 3 * sizeof(int));
                    updateSlotPtr(3 * sizeof(int) - recordSizeOld, page, recordOffset + recordSizeOld);
                    fileHandle.writePage(rid.pageNum, page);
                    free(page);
                    return 0;
                }
            }
        }
    }
}

int getAttributeOffset(void *pageData, const vector<Attribute> &recordDescriptor, const RID &rid, const string &attributeName)
{
    // returns on which byte the attribute starts in a record. Offset is relative to the recordOffset.
    // returns -1 if it is a tombstone
    // returns -2 if it is a null attribute
    int recordOffset = getRecordOffset(pageData, rid.slotNum);
    if (recordOffset == -1)
        return -1;

    int attribOffsetSize = getAttributesOffsetSize(recordDescriptor.size());

    int bitmapSize = getBitMapSize(recordDescriptor.size());
    unsigned char *bitMap = new unsigned char[bitmapSize];
    memcpy(bitMap, (char *)pageData + recordOffset + attribOffsetSize, bitmapSize * sizeof(char));
    Attribute attrib;

    for (int i = 0; i < recordDescriptor.size(); ++i)
    {
        if (attributeName == recordDescriptor[i].name)
        {
            attrib = recordDescriptor[i];
            // if it is a null attribute
            if ((bitMap[i / 8] >> (7 - i % 8)) & 1)
            {
                delete[] bitMap;
                return -2;
            }
            int attribOffset;
            if (i == 0)
            {
                attribOffset = attribOffsetSize + bitmapSize;
                // if it is the 0th attribute, then the beginning of that attribute
                // will be attribOffsetSize + bitmapSize
            }
            else
            {
                int attribOffsetPtr = (i - 1) * sizeof(unsigned); // one attrib offset occupies unsigned num of bytes
                memcpy(&attribOffset, (char *)pageData + recordOffset + attribOffsetPtr, sizeof(int));
                attribOffset += attribOffsetSize;
                // lets say, we want 1st index attribute, then the value stored in the 0th index of attributeOffset list
                // will say the beginning of the 1st attribute (as the values stored in the ith index of attributeOffset list
                // points to where the ith attribute ends or where the i+1th attribute starts from.
            }
            delete[] bitMap;
            return attribOffset;
        }
    }
    delete[] bitMap;
}

RC RecordBasedFileManager::readAttribute(FileHandle *fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, const string &attributeName, void *data)
{
    void *pageData = malloc(PAGE_SIZE);

    int sz = recordDescriptor.size();
    bool attributeExist=false;
    for (const Attribute& attrb : recordDescriptor)
        if (attrb.name == attributeName)
            attributeExist=true;
    if(!attributeExist) 
    {
        free(pageData);
        return -1;
    }
    if (fileHandle->readPage(rid.pageNum, pageData) == -1)
    {
        free(pageData);
        return -1;
    }

    int recordOffset = getRecordOffset(pageData, rid.slotNum);
    if (recordOffset == -1)
    {
        free(pageData);
        return -1;
    }
    else
    {
        int ptr;
        memcpy((char *)&ptr, (char *)pageData + recordOffset, sizeof(int));
        if (ptr == -1)
        {
            int slotNum, pageNum;
            memcpy((char *)&pageNum, (char *)pageData + recordOffset + sizeof(int), sizeof(int));
            memcpy((char *)&slotNum, (char *)pageData + recordOffset + 2 * sizeof(int), sizeof(int));
            RID rid1;
            rid1.slotNum = slotNum;
            rid1.pageNum = pageNum;
            free(pageData);
            return readAttribute(fileHandle, recordDescriptor, rid1, attributeName, data);
        }
        else
        {

            int attribOffsetSize = getAttributesOffsetSize(recordDescriptor.size());

            int bitmapSize = getBitMapSize(recordDescriptor.size());
            unsigned char *bitMap = new unsigned char[bitmapSize];
            memcpy(bitMap, (char *)pageData + recordOffset + attribOffsetSize, bitmapSize * sizeof(char));
            Attribute attrib;
            for (int i = 0; i < recordDescriptor.size(); ++i)
            {
                if (attributeName == recordDescriptor[i].name)
                {
                    attrib = recordDescriptor[i];
                }
            }
            int attribOffset = getAttributeOffset(pageData, recordDescriptor, rid, attributeName);

            if (attribOffset == -2) // if attribute is null
            {
                char nullValue = 1 << 7;
                memcpy((char *)data, &nullValue, sizeof(char));
                free(pageData);
                delete[] bitMap;
                return -2;
            }
            int numBytesCopy;
            if (attrib.type == TypeInt)
                numBytesCopy = sizeof(int);
            else if (attrib.type == TypeReal)
                numBytesCopy = sizeof(float);
            else // for strings
            {
                memcpy(&numBytesCopy, (char *)pageData + recordOffset + attribOffset, sizeof(int));
                numBytesCopy += 4;
                // since we're copying only the actual string data and the preceding byte (that
                // represents the string length), we're increasing numBytesCopy by 4.
            }
            memcpy((char *)data, (char *)pageData + recordOffset + attribOffset, numBytesCopy);
            free(pageData);
            delete[] bitMap;
            return 0;
        }
    }
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor,
                                const string &conditionAttribute, const CompOp compOp, const void *value,
                                const vector<string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator)
{

    rbfm_ScanIterator.fileHandle = &fileHandle;
    rbfm_ScanIterator.recordDescriptor = recordDescriptor;
    rbfm_ScanIterator.conditionAttribute = conditionAttribute;
    rbfm_ScanIterator.compOp = compOp;
    rbfm_ScanIterator.value = (void *)value;
    rbfm_ScanIterator.attributeNames = attributeNames;
    rbfm_ScanIterator.rbfm = this;
    rbfm_ScanIterator.totalNumPages = fileHandle.getNumberOfPages();
    rbfm_ScanIterator.nextRid.slotNum = 0;
    rbfm_ScanIterator.nextRid.pageNum = 0;
}

template <typename T>
bool compare(T givenValue, T recordValue, CompOp compOp)
{
    if (compOp == EQ_OP)
        return givenValue == recordValue;
    if (compOp == NE_OP)
        return givenValue != recordValue;
    if (compOp == GT_OP)
        return givenValue < recordValue;
    if (compOp == GE_OP)
        return givenValue <= recordValue;
    if (compOp == LT_OP)
        return givenValue > recordValue;
    if (compOp == LE_OP)
        return givenValue >= recordValue;
}

bool RBFM_ScanIterator::isConditionMet(void *pageData, AttrType attrType, RID ridTemp)
{
    if (compOp == NO_OP)
        return true;
    int recordOffset = getRecordOffset(pageData, ridTemp.slotNum);
    int attribOffset = getAttributeOffset(pageData, recordDescriptor, ridTemp, conditionAttribute);
    if (attribOffset == -2)
        return false; // assuming there will be no comparison if value is null.
    // @todo: add >/< comparison in string
    if (attrType == TypeVarChar)
    {
        int strLen;
        memcpy(&strLen, (char *)pageData + recordOffset + attribOffset, sizeof(int));
        char *strBuffer=new char[strLen + 1];
        memcpy((char *)strBuffer,(char *)pageData + recordOffset + attribOffset+sizeof(int),strLen);
        strBuffer[strLen]='\0';
        int valLen;
        memcpy(&valLen, (char *)value, sizeof(int));
        char *valStr=new char[valLen + 1];
        memcpy((char *)valStr,(char *)value +sizeof(int),strLen);
        valStr[valLen]='\0';
        // @note: We're assuming here that the value is in the form of "8Anteater"
        int cmpStringVal= strcmp(strBuffer ,valStr);
        delete[] strBuffer;
        delete[] valStr;

        if (cmpStringVal==0 & (compOp == EQ_OP || compOp==LE_OP || compOp==GE_OP))
        return true;

        else if (cmpStringVal<0 && (compOp==LT_OP || compOp==LE_OP || compOp==NE_OP))
        return true;

        else if (cmpStringVal>0 && (compOp==GT_OP || compOp==GE_OP || compOp==NE_OP))
        return true;

        else
        return false;
    }
    else
    {
        if (attrType == TypeInt)
        {
            int givenValue;
            int recordValue;
            memcpy(&recordValue, (char *)pageData + recordOffset + attribOffset, sizeof(int));
            memcpy(&givenValue, (char *)value, sizeof(int));
            return compare<int>(givenValue, recordValue, compOp);
        }
        else if (attrType == TypeReal)
        {
            float givenValue;
            float recordValue;
            memcpy(&recordValue, (char *)pageData + attribOffset, sizeof(float));
            memcpy(&givenValue, (char *)value, sizeof(float));
            return compare<float>(givenValue, recordValue, compOp);
        }
    }
}

AttrType getAttrTypeFromName(vector<Attribute> recordDescriptor, string attribName)
{
    for (int i = 0; i < recordDescriptor.size(); i++)
    {
        if (recordDescriptor[i].name == attribName)
        {
            return recordDescriptor[i].type;
        }
    }
}

RC resolveRID(FileHandle &fh, RID rid, RID &resolvedRID)
{
    void *pageData = malloc(PAGE_SIZE);
    fh.readPage(rid.pageNum, pageData);
    int recordOffset = getRecordOffset(pageData, rid.slotNum);
    if (recordOffset == -1)
    {
        resolvedRID = rid;
        free(pageData);
        return 0;
    }
    int ptr;
    memcpy((char *)&ptr, (char *)pageData + recordOffset, sizeof(int));
    if (ptr == -1)
    {
        int slotNum, pageNum;
        memcpy((char *)&pageNum, (char *)pageData + recordOffset + sizeof(int), sizeof(int));
        memcpy((char *)&slotNum, (char *)pageData + recordOffset + 2 * sizeof(int), sizeof(int));
        RID rid1;
        rid1.slotNum = slotNum;
        rid1.pageNum = pageNum;
        free(pageData);
        return resolveRID(fh, rid1, resolvedRID);
    }
    else
    {
        resolvedRID = rid;
        free(pageData);
        return 0;
    }
}

RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data)
{
    if (nextRid.pageNum >= totalNumPages)
    {
        return RBFM_EOF;
    }
    void *pageData = malloc(PAGE_SIZE);
    fileHandle->readPage(nextRid.pageNum, pageData);
    int totalSlotCount = getSlotCount(pageData);
    if (nextRid.slotNum >= totalSlotCount)
    {
        nextRid.pageNum += 1;
        nextRid.slotNum = 0;
        free(pageData);
        return getNextRecord(rid, data);
    }

    RID resolvedRid;
    resolveRID(*fileHandle, nextRid, resolvedRid);
    fileHandle->readPage(resolvedRid.pageNum, pageData);
    int recordOffset = getRecordOffset(pageData, resolvedRid.slotNum); // bc getRecordOffset expects slotNumber
    if (recordOffset == -1)
    {
        // if it is a tombstone recordPtr, then move on.
        nextRid.slotNum += 1;
        free(pageData);
        return getNextRecord(rid, data);
    }
    // we found a record! Now check for condition
    else
    {
        int bitMapSizeNewRecord = getBitMapSize(attributeNames.size());
        Attribute conditionAttributeObj; // conditionAttribute is a str, conditionAttributeObj is an Attribute obj.
        for (int i = 0; i < recordDescriptor.size(); i++)
        {
            if (conditionAttribute == recordDescriptor[i].name)
            {
                conditionAttributeObj = recordDescriptor[i];
                break;
            }
        }
        // S1: check for condition
        if (isConditionMet(pageData, conditionAttributeObj.type, resolvedRid)) // bc the actual record data lies in resolvedRid
        {
            rid = nextRid; // BUT we don't want the user to know all the resolvedRid thingy. So giving user the nextRid
            // S2: if condition is met; then do the copying dance...
            int bitMapSize = getBitMapSize(attributeNames.size());
            memset(data, 0, bitMapSize);
            int nextAttribWritePtr = bitMapSize;
            int attribOffset;
            AttrType aType;
            // S3: go over all the attributes that are meant to be copied
            for (int an = 0; an < attributeNames.size(); an++)
            {
                string attribNameTemp = attributeNames[an];
                // attribOffset here is only for determining if the record we're copying is null
                attribOffset = getAttributeOffset(pageData, recordDescriptor, resolvedRid, attribNameTemp);
                // S4: If the attr you're copying is not null, then copy the data and update nextAttribWritePtr
                if (attribOffset != -2)
                {
                    rbfm->readAttribute(fileHandle, recordDescriptor, resolvedRid, attribNameTemp,
                                        (char *)data + nextAttribWritePtr);
                    // after the data is copies, you need to increase the nextAttribWritePtr
                    // if it is int/float increasing is straightforward. i.e. increase by 4
                    // if it is string, increment by strLength + 4
                    aType = getAttrTypeFromName(recordDescriptor, attribNameTemp);
                    if (aType == TypeVarChar)
                    {
                        int strLen;
                        // NOTE: Here nextAttribWritePtr still points to end of the previous inserted
                        // record. i.e. the beginning of this new inserted record.
                        memcpy(&strLen, (char *)data + nextAttribWritePtr, sizeof(int));
                        strLen += 4;
                        nextAttribWritePtr += strLen;
                    }
                    else if (aType == TypeInt)
                        nextAttribWritePtr += sizeof(int);
                    else
                        nextAttribWritePtr += sizeof(float);
                }
                // S5: If the attr you're copying is null, then update the bitMap
                else
                {
                    // if null, then set the bit to 1 in bitMap
                    ((char *)data)[an / 8] |= (1 << (7 - an) % 8); // THANKS CHATGPT. You're amazing <3
                }
            }
            // S6: Once the record is read, point nextRid to the next slot.
            // let the subsequent call decide what to do next.
            nextRid.slotNum += 1;
            free(pageData);
            return 0;
        }
        else // S6: If condition is not meant then, call getNextRecord again :(
        {
            nextRid.slotNum += 1;
            free(pageData);
            return getNextRecord(rid, data);
        }
    }
    free(pageData);
    return RBFM_EOF;
}
