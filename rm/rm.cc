#include "rm.h"
#include <math.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <sys/stat.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include <stdio.h>
#include <math.h>
#include <algorithm>

string TABLES_FILE = "Tables";
string COLS_TABLES_FILE = "Columns";

vector<Attribute> getTableAttribs()
{
    vector<Attribute> attributes;
    Attribute attr;

    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    attr.name = "table-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    attributes.push_back(attr);

    attr.name = "file-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    attributes.push_back(attr);

    attr.name = "system-table"; // 1 means it is system table. 0 means user table
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    return attributes;
}

vector<Attribute> getColumnTblAttribs()
{
    vector<Attribute> attributes;
    Attribute attr;

    attr.name = "table-id";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    attr.name = "column-name";
    attr.type = TypeVarChar;
    attr.length = 50;
    attributes.push_back(attr);

    attr.name = "column-type";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    attr.name = "column-length";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    attr.name = "column-position";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    attr.name = "column-exists";
    attr.type = TypeInt;
    attr.length = sizeof(int);
    attributes.push_back(attr);

    return attributes;
}

// Calculate actual bytes for nulls-indicator based on the given field counts
// 8 fields = 1 byte
int getActualByteForNulls(int fieldCount)
{

    return ceil((double)fieldCount / CHAR_BIT);
}

void prepareTableRecord(vector<Attribute> recordDescriptor, const int tableNameLen, const string &tableName, const int fileNameLen,
                        const string &fileName, const int tableId, const int systemTable, void *buffer, int *recordSize)
{
    int offset = 0;
    int fieldCount = recordDescriptor.size();

    // Null-indicators
    int nullFieldsIndicatorActualSize = getActualByteForNulls(fieldCount);
    char nullFieldsIndicator;
    memset(&nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);

    // Null-indicator for the fields
    memcpy((char *)buffer + offset, &nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    // Beginning of the actual data
    // Note that the left-most bit represents the first field. Thus, the offset is 7 from right, not 0.
    // e.g., if a record consists of four fields and they are all nulls, then the bit representation will be: [11110000]

    // table-id
    memcpy((char *)buffer + offset, &tableId, sizeof(int));
    offset += sizeof(int);

    // table-name
    memcpy((char *)buffer + offset, &tableNameLen, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, tableName.c_str(), tableNameLen);
    offset += tableNameLen;

    // file-name
    memcpy((char *)buffer + offset, &fileNameLen, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, fileName.c_str(), fileNameLen);
    offset += fileNameLen;

    // system-table
    memcpy((char *)buffer + offset, &systemTable, sizeof(int));
    offset += sizeof(int);

    *recordSize = offset;
}

void prepareColumnRecord(vector<Attribute> recordDescriptor, const int tableId,
                         const int columnNameLen, const string &columnName,
                         const int columnType, const int columnLength, const int columnPosition,
                         void *buffer, int *recordSize)
{
    int offset = 0;
    int fieldCount = recordDescriptor.size();

    // Null-indicators
    int nullFieldsIndicatorActualSize = getActualByteForNulls(fieldCount);
    char nullFieldsIndicator;
    memset(&nullFieldsIndicator, 0, nullFieldsIndicatorActualSize);

    // Null-indicator for the fields
    memcpy((char *)buffer + offset, &nullFieldsIndicator, nullFieldsIndicatorActualSize);
    offset += nullFieldsIndicatorActualSize;

    // table-id
    memcpy((char *)buffer + offset, &tableId, sizeof(int));
    offset += sizeof(int);

    // col-name
    memcpy((char *)buffer + offset, &columnNameLen, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, columnName.c_str(), columnNameLen);
    offset += columnNameLen;

    // col-type
    memcpy((char *)buffer + offset, &columnType, sizeof(int));
    offset += sizeof(int);

    // col-length
    memcpy((char *)buffer + offset, &columnLength, sizeof(int));
    offset += sizeof(int);

    // col-position
    memcpy((char *)buffer + offset, &columnPosition, sizeof(int));
    offset += sizeof(int);

    // col-exist
    int exists = 0;
    memcpy((char*)buffer + offset, &exists, sizeof(int));
    offset+=sizeof(int);
    
    *recordSize = offset;
}

RelationManager *RelationManager::instance()
{
    static RelationManager _rm;
    return &_rm;
}

RelationManager::RelationManager()
{
}

RelationManager::~RelationManager()
{
}

RC RelationManager::createCatalog()
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fhTable;

    // ----- creating tables file and inserting it with records -----
    if (rbfm->createFile(TABLES_FILE) == -1)
        return -1; // if file already exists, then return -1
    rbfm->openFile(TABLES_FILE, fhTable);
    vector<Attribute> tableRecordDescriptor = getTableAttribs();

    void *tableRecordData = malloc(200); // actually you need only 4+54+54+4+4 bytes
    int tableRecordDataLen;
    // inserting "Table" record
    RID rid;
    prepareTableRecord(tableRecordDescriptor, 6, "Tables", 6, TABLES_FILE, 1, 1, tableRecordData, &tableRecordDataLen);
    rbfm->insertRecord(fhTable, tableRecordDescriptor, tableRecordData, rid);

    // inserting "Columns" record
    memset(tableRecordData, 0, 200); // actually you need only 4+54+54+4+4 bytes
    prepareTableRecord(tableRecordDescriptor, 7, "Columns", 7, COLS_TABLES_FILE, 2, 1, tableRecordData, &tableRecordDataLen);
    rbfm->insertRecord(fhTable, tableRecordDescriptor, tableRecordData, rid);
    free(tableRecordData);

    rbfm->closeFile(fhTable);

    // ----- creating columns file and inserting it with records -----
    if (rbfm->createFile(COLS_TABLES_FILE) == -1)
        return -1; // if file already exists, then return -1
    FileHandle fhCol;
    rbfm->openFile(COLS_TABLES_FILE, fhCol);
    vector<Attribute> colRecordDescriptor = getColumnTblAttribs();

    void *colRecordData = malloc(400);
    memset(colRecordData, 0, 400);
    int colRecordDataLen;
    prepareColumnRecord(colRecordDescriptor, 1, 8, "table-id", TypeInt, 4, 1, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 1, 10, "table-name", TypeVarChar, 50, 2, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 1, 9, "file-name", TypeVarChar, 50, 3, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 1, 12, "system-table", TypeInt, 4, 4, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 8, "table-id", TypeInt, 4, 1, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 11, "column-name", TypeVarChar, 50, 2, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 11, "column-type", TypeInt, 4, 3, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 13, "column-length", TypeInt, 4, 4, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 15, "column-position", TypeInt, 4, 5, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);

    memset(colRecordData, 0, 400);
    prepareColumnRecord(colRecordDescriptor, 2, 13, "column-exists", TypeInt, 4, 6, colRecordData, &colRecordDataLen);
    rbfm->insertRecord(fhCol, colRecordDescriptor, colRecordData, rid);
    free(colRecordData);
    rbfm->closeFile(fhCol);

    return 0;
}

RC RelationManager::deleteCatalog()
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    rbfm->destroyFile(TABLES_FILE);
    rbfm->destroyFile(COLS_TABLES_FILE);
    return 0;
}

int getNextTableId()
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fhTable;
    rbfm->openFile(TABLES_FILE, fhTable);
    vector<Attribute> tableRecordDescriptor = getTableAttribs();
    int tableId = -1, maxTableId = -1;
    RBFM_ScanIterator rbfmIterator;
    void *value = NULL;
    vector<string> matchattrbs;
    matchattrbs.push_back(string("table-id"));
    rbfm->scan(fhTable, tableRecordDescriptor, string(""), NO_OP, value, matchattrbs, rbfmIterator);

    RID rid;
    void *data = malloc(200);
    while (RBFM_EOF != rbfmIterator.getNextRecord(rid, data))
    {
        memcpy((char *)&tableId, (char *)data + 1, sizeof(int));
        if (tableId > maxTableId)
            maxTableId = tableId;
    }
    free(data);
    free(value);
    rbfmIterator.close();
    return maxTableId += 1;
}
RC RelationManager::createTable(const string &tableName, const vector<Attribute> &attrs)
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();

    rbfm->createFile(tableName);
    FileHandle fhTable;
    rbfm->openFile(TABLES_FILE, fhTable);
    vector<Attribute> tableRecordDescriptor = getTableAttribs();
    int tableId = getNextTableId();
    int systemTable = 0, tableRecordDataLen;
    void *data = malloc(300);
    RID rid;
    prepareTableRecord(tableRecordDescriptor, tableName.size(), tableName, tableName.size(), tableName, tableId, systemTable, data, &tableRecordDataLen);
    rbfm->insertRecord(fhTable, tableRecordDescriptor, data, rid);
    rbfm->closeFile(fhTable);

    FileHandle fhCol;
    rbfm->openFile(COLS_TABLES_FILE, fhCol);
    vector<Attribute> colRecordDescriptor = getColumnTblAttribs();

    int noOfAttr = attrs.size();
    for (int i = 0; i < noOfAttr; ++i)
    {
        int pos = i + 1;
        string attrName = attrs[i].name;
        int attrNameLen = attrName.size();
        int columnRecordLen;
        memset(data, 0, 300);
        prepareColumnRecord(colRecordDescriptor, tableId, attrNameLen, attrName, attrs[i].type, attrs[i].length, pos, data, &columnRecordLen);
        rbfm->insertRecord(fhCol, colRecordDescriptor, data, rid);
    }

    rbfm->closeFile(fhCol);
    free(data);

    return 0;
}

int getTableIdFromName(string tableName, RID &rid)
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fileHandle;
    RBFM_ScanIterator rscan;
    rbfm->openFile(TABLES_FILE, fileHandle);
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    vector<string> attrNames;
    attrNames.push_back("table-id");
    int tableId;
    void *returnedData = malloc(10);
    void *tableNameData = malloc(100);
    int tableNameLen = tableName.size();
    memcpy((char *)tableNameData, &tableNameLen, sizeof(int));
    memcpy((char *)tableNameData + sizeof(int), tableName.data(), tableName.size());
    rbfm->scan(fileHandle, tableRecordDesc, "table-name", EQ_OP, tableNameData, attrNames, rscan);
    while (rscan.getNextRecord(rid, returnedData) != RBFM_EOF)
    {
        memcpy((char *)&tableId, (char *)returnedData + 1, sizeof(int)); // we dont want to read bit map.
        free(returnedData);
        free(tableNameData);
        rbfm->closeFile(fileHandle);
        return tableId;
    }
    free(tableNameData);
    free(returnedData);
    rbfm->closeFile(fileHandle);
    return -1;
}

vector<RID> getColumnRIDsForTableId(int tableId, bool fetchDroppedCol = true)
{
    // fetchDroppedCol flag governs whether to read dropped columns into the vector.
    // if set to true, it will return all the columns RIDs including dropped columns
    // else it will return only the column RIDs that are not dropped 
    RID tempRid;
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fhCol;
    RBFM_ScanIterator rscan;
    rbfm->openFile(COLS_TABLES_FILE, fhCol);
    vector<Attribute> colRecordDesc;
    colRecordDesc = getColumnTblAttribs();
    vector<string> attrNames;
    attrNames.push_back("column-exists");
    void *returnedData = malloc(10);
    RID rid;
    vector<RID> colRids;
    rbfm->scan(fhCol, colRecordDesc, "table-id", EQ_OP, &tableId, attrNames, rscan);
    // @todo: check if this logic is correct
    // i.e. you're passing rid by ref in getNextRecord and then pushing it to colRids.
    while (rscan.getNextRecord(rid, returnedData) != RBFM_EOF)
    {
        if(!fetchDroppedCol) {
            int colExists;
            memcpy(&colExists, (char *)returnedData + 1, sizeof(int));
            if(colExists==0) {
                colRids.push_back(rid);
            }
        }
        else
            colRids.push_back(rid);
            
    }
    free(returnedData);
    rbfm->closeFile(fhCol);
    return colRids;
}

string getFileNameFromTableRid(RID rid)
{
    // rid belonging to the record in the Table table
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fileHandle;
    RBFM_ScanIterator rscan;
    rbfm->openFile(TABLES_FILE, fileHandle);
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    vector<string> attrNames;
    attrNames.push_back("table-id");
    void *tableFileNameData = malloc(60); // you need only 54 actually
    rbfm->readAttribute(&fileHandle, tableRecordDesc, rid, "file-name", tableFileNameData);
    int strLen;
    memcpy((char *)&strLen, tableFileNameData, sizeof(int));
    string tableFileName;
    tableFileName.resize(strLen);
    memcpy(&tableFileName[0], (char *)tableFileNameData + sizeof(int), strLen);
    free(tableFileNameData);
    rbfm->closeFile(fileHandle);
    return tableFileName;
}

RC RelationManager::deleteTable(const string &tableName)
{
    RID tableRid;
    int tableId = getTableIdFromName(tableName, tableRid);
    if (tableId == -1)
        return -1;

    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle tblFileHandle;
    int systemTable;
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    rbfm->openFile(TABLES_FILE, tblFileHandle);
    rbfm->readAttribute(&tblFileHandle, tableRecordDesc, tableRid, "system-table", &systemTable);
    if (systemTable == 1)
    {
        rbfm->closeFile(tblFileHandle);
        return -1;
    }

    vector<RID> rids;
    rids = getColumnRIDsForTableId(tableId);
    FileHandle fileHandle;
    // first delete the columns from columns table
    rbfm->openFile(COLS_TABLES_FILE, fileHandle);
    vector<Attribute> colRecordDesc;
    colRecordDesc = getColumnTblAttribs();
    for (int i = 0; i < rids.size(); i++)
    {
        rbfm->deleteRecord(fileHandle, colRecordDesc, rids[i]);
    }
    rbfm->closeFile(fileHandle);

    // now destroy the file
    string tableFileName = getFileNameFromTableRid(tableRid);
    rbfm->destroyFile(tableFileName);

    // then delete the table entry from the tables record
    rbfm->deleteRecord(tblFileHandle, tableRecordDesc, tableRid);
    rbfm->closeFile(tblFileHandle);

    return 0;
}

struct AttribPos {
    Attribute attr;
    int colPos;

//    bool operator<(const  MyClass & other) //(1)
//    {
//        return i < other.i;
//    }

    bool operator< (const AttribPos &other) {
        return colPos < other.colPos;
    }
};

RC RelationManager::getAttributes(const string &tableName, vector<Attribute> &attrs)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;
    vector<RID> colRids;
    colRids = getColumnRIDsForTableId(tableId,false);
    vector<AttribPos> tempRecordDescWColPos;
    // vector<Attribute> tempRecordDesc;
    vector<Attribute> colRecordDesc = getColumnTblAttribs();
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fhColTable;
    rbfm->openFile(COLS_TABLES_FILE, fhColTable);
    for (int i = 0; i < colRids.size(); i++)
    {
        Attribute attrib;

        // reading column name:
        void *tempData = malloc(60);
        rbfm->readAttribute(&fhColTable, colRecordDesc, colRids[i], "column-name", tempData);
        int varcharlength;
        memcpy(&varcharlength, (char *)tempData, sizeof(int));
        char varchar[varcharlength + 1];
        memcpy(&varchar, (char *)tempData + sizeof(int), varcharlength);
        varchar[varcharlength] = '\0';
        free(tempData);
        attrib.name = varchar;

        // reading column-type
        int attrType;
        void *attrTypeData = malloc(5);
        rbfm->readAttribute(&fhColTable, colRecordDesc, colRids[i], "column-type", attrTypeData);
        memcpy((char *)&attrType, attrTypeData, sizeof(int));
        attrib.type = (AttrType)attrType;
        free(attrTypeData);

        // reading column-length
        int colLen;
        void *colLenData = malloc(5);
        rbfm->readAttribute(&fhColTable, colRecordDesc, colRids[i], "column-length", colLenData);
        memcpy((char *)&colLen, colLenData, sizeof(int));
        attrib.length = colLen;
        free(colLenData);

        // fetching the col-position
        int colPos;
        void *colPosData = malloc(5);
        rbfm->readAttribute(&fhColTable, colRecordDesc, colRids[i], "column-position", colPosData);
        memcpy((char *)&colPos, colPosData, sizeof(int));
        free(colPosData);

        // fetching the col-exist
        int colExist;
        void *colExistData = malloc(5);
        rbfm->readAttribute(&fhColTable, colRecordDesc, colRids[i], "column-exists", colExistData);
        memcpy((char *)&colExist, colExistData, sizeof(int));
        free(colExistData);

        AttribPos attribPos;
        attribPos.attr = attrib;
        attribPos.colPos = colPos;

//        tempRecordDesc[colPos - 1] = attrib;
        tempRecordDescWColPos.push_back(attribPos);
    }
    sort(tempRecordDescWColPos.begin(), tempRecordDescWColPos.end());
    for(int i=0; i<tempRecordDescWColPos.size(); i++)
    {
        attrs.push_back(tempRecordDescWColPos[i].attr);
    }
    return 0;
}

RC RelationManager::insertTuple(const string &tableName, const void *data, RID &rid)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;

    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle tblFileHandle;
    int systemTable;
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    rbfm->openFile(TABLES_FILE, tblFileHandle);
    rbfm->readAttribute(&tblFileHandle, tableRecordDesc, tableRID, "system-table", &systemTable);
    if (systemTable == 1)
    {
        rbfm->closeFile(tblFileHandle);
        return -1;
    }

    string tableFileName = getFileNameFromTableRid(tableRID);
    vector<Attribute> recordDesc;
    getAttributes(tableName, recordDesc);
    FileHandle fh;
    rbfm->openFile(tableFileName, fh);
    RC resp = rbfm->insertRecord(fh, recordDesc, data, rid);
    rbfm->closeFile(fh);
    return resp;
}

RC RelationManager::deleteTuple(const string &tableName, const RID &rid)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;

    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle tblFileHandle;
    int systemTable;
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    rbfm->openFile(TABLES_FILE, tblFileHandle);
    rbfm->readAttribute(&tblFileHandle, tableRecordDesc, tableRID, "system-table", &systemTable);
    if (systemTable == 1)
    {
        rbfm->closeFile(tblFileHandle);
        return -1;
    }

    string tableFileName = getFileNameFromTableRid(tableRID);
    vector<Attribute> recordDesc;
    getAttributes(tableName, recordDesc);
    FileHandle fh;
    rbfm->openFile(tableFileName, fh);
    RC resp = rbfm->deleteRecord(fh, recordDesc, rid);
    rbfm->closeFile(fh);
    return resp;
}

RC RelationManager::updateTuple(const string &tableName, const void *data, const RID &rid)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;
    
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle tblFileHandle;
    int systemTable;
    vector<Attribute> tableRecordDesc;
    tableRecordDesc = getTableAttribs();
    rbfm->openFile(TABLES_FILE, tblFileHandle);
    rbfm->readAttribute(&tblFileHandle, tableRecordDesc, tableRID, "system-table", &systemTable);
    if (systemTable == 1)
    {
        rbfm->closeFile(tblFileHandle);
        return -1;
    }

    string tableFileName = getFileNameFromTableRid(tableRID);
    vector<Attribute> recordDesc;
    getAttributes(tableName, recordDesc);
    FileHandle fh;
    rbfm->openFile(tableFileName, fh);
    RC resp = rbfm->updateRecord(fh, recordDesc, data, rid);
    rbfm->closeFile(fh);
    return resp;
}

RC RelationManager::readTuple(const string &tableName, const RID &rid, void *data)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;
    string tableFileName = getFileNameFromTableRid(tableRID);
    vector<Attribute> recordDesc;
    getAttributes(tableName, recordDesc);
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fh;
    rbfm->openFile(tableFileName, fh);
    RC resp = rbfm->readRecord(fh, recordDesc, rid, data);
    rbfm->closeFile(fh);
    return resp;
}

RC RelationManager::printTuple(const vector<Attribute> &attrs, const void *data)
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    RC resp = rbfm->printRecord(attrs, data);
    return resp;
}

RC RelationManager::readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data)
{
    RID tableRID;
    int tableId;
    tableId = getTableIdFromName(tableName, tableRID);
    if (tableId == -1)
        return -1;
    string tableFileName = getFileNameFromTableRid(tableRID);
    vector<Attribute> recordDesc;
    getAttributes(tableName, recordDesc);
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    FileHandle fh;
    rbfm->openFile(tableFileName, fh);
    RC resp = rbfm->readAttribute(&fh, recordDesc, rid, attributeName, data);
    if (resp == -1)
    {
        rbfm->closeFile(fh);
        return -1;
    }
    if (resp == -2)
    {
        rbfm->closeFile(fh);
        return 0;
    }
    else
    {
        Attribute attr;
        for (int i = 0; i < recordDesc.size(); i++)
        {
            if (recordDesc[i].name == attributeName)
            {
                attr = recordDesc[i];
                break;
            }
        }
        int moveBytes;
        if (attr.type == TypeVarChar)
        {
            int strLen;
            memcpy(&strLen, (char *)data, sizeof(int));
            strLen += 4;
            moveBytes = strLen;
        }
        else if (attr.type == TypeInt)
            moveBytes = sizeof(int);
        else
            moveBytes = sizeof(float);
        memmove((char *)data + 1, data, moveBytes);
        memset(data, 0, 1);
        rbfm->closeFile(fh);
        return 0;
    }
}

RC RelationManager::scan(const string &tableName,
                         const string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const vector<string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator)
{
    RecordBasedFileManager *rbfm = RecordBasedFileManager::instance();
    vector<Attribute> recordDescriptor;
    getAttributes(tableName, recordDescriptor);
    rbfm->openFile(tableName, rm_ScanIterator._fh);
    rbfm->scan(rm_ScanIterator._fh, recordDescriptor, conditionAttribute, compOp, value, attributeNames, rm_ScanIterator._rbfmScanItr);
    return 0;
}

RC RM_ScanIterator::getNextTuple(RID &rid, void *data)
{
    return _rbfmScanItr.getNextRecord(rid, data);
}

int getDataSize(const vector<Attribute> &recordDescriptor, const void* data) 
{
    int noOfFields = recordDescriptor.size();
    int bitMapSize = ceil(noOfFields/8.0);
    int offset = bitMapSize;
    unsigned char* nullbytes = new unsigned char[bitMapSize];
    memcpy(nullbytes, data, bitMapSize*sizeof(char));
    for(int i = 0; i< noOfFields; ++i) {
        if((nullbytes[i/8] >> (7-i%8)) & 1)
            continue;
        const Attribute& atrb = recordDescriptor[i];
        if(atrb.type == TypeVarChar) {
            int length;
            memcpy(&length, (char*)data+offset, sizeof(int));
            offset += length;
        }
        offset += 4;        
    }
    delete[] nullbytes;
    return offset;
}

// Extra credit work
RC RelationManager::dropAttribute(const string &tableName, const string &attributeName)
{
    int tableId;
    RID tableRID;
    tableId=getTableIdFromName(tableName,tableRID);
    if(tableId==-1)return -1;
    RecordBasedFileManager* rbfm = RecordBasedFileManager::instance();
    FileHandle fhCol;
    rbfm->openFile(COLS_TABLES_FILE, fhCol);

    vector<Attribute> colDesc=getColumnTblAttribs();

    RBFM_ScanIterator rbfmIterator;
    vector<string> matchattrbs;
    matchattrbs.push_back("column-name");
    rbfm->scan(fhCol, colDesc,"table-id", EQ_OP, (void*)&tableId, matchattrbs, rbfmIterator);
    void *data = malloc(200);
    RID rmRid;
    bool found = false;
    while (RBFM_EOF != rbfmIterator.getNextRecord(tableRID, data)) {
        int len = 0;
        memcpy((char*)&len, (char*)data+1, sizeof(int));
        char attribname[51];
        memcpy(attribname, (char*)data+1+sizeof(int), len * sizeof(char));
        attribname[len] = '\0';
        if (! attributeName.compare(attribname)) {
            rmRid = tableRID;
            found = true;
            break;
        }
    }
    rbfmIterator.close();
    if(found)
    {
        rbfm->readRecord(fhCol, colDesc, rmRid, data);
        int dataSize = getDataSize(colDesc, data);
        int exists = 1;
        memcpy((char*)data+(dataSize-sizeof(int)), (char*)&exists, sizeof(int));
        rbfm->updateRecord(fhCol,colDesc,data,rmRid);
    }

    free(data);
    rbfm->closeFile(fhCol);
    return 0;
}

// Extra credit work
RC RelationManager::addAttribute(const string &tableName, const Attribute &attr)
{
    return -1;
}
