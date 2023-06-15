#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <stdlib.h>
#include "iostream"
#include <string.h>
#include <math.h>
#include "pfm.h"

using namespace std;

bool FileExists(string fileName)
{
    struct stat stFileInfo;

    if (stat(fileName.c_str(), &stFileInfo) == 0)
        return true;
    else
        return false;
}

PagedFileManager *PagedFileManager::_pf_manager = 0;

PagedFileManager *PagedFileManager::instance()
{
    if (!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}

PagedFileManager::PagedFileManager()
{
}

PagedFileManager::~PagedFileManager()
{
}

RC PagedFileManager::createFile(const string &fileName)
{
    ifstream ifs(fileName.c_str());
    ofstream ofs;
    bool fileExists = FileExists(fileName);
    if (fileExists)
    {
        // cout << "File already exists";
        return -1;
    }
    ofs.open(fileName.c_str(), ios::binary);
    if (ofs)
        return 0;
    return -1;
}

RC PagedFileManager::destroyFile(const string &fileName)
{
    bool fileExists = FileExists(fileName);
    if (!fileExists)
    {
        // cout << "File does not exist. Unable to delete.";
        return -1;
    }
    int status = remove(fileName.c_str());
    if (status == 0)
        return 0;
    else
        return -1;
}

RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    return fileHandle.openFile(fileName);
}

RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    return fileHandle.closeFile();
}

FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
}

FileHandle::~FileHandle()
{
}

RC FileHandle::openFile(const string &fileName)
{
    bool fileExists = FileExists(fileName);
    if (!fileExists)
        return -1;
    // if file is already open then throw an error
    if (_fs.is_open())
        return -1;
    _fs.open(fileName.c_str(), ios::out | ios::binary | ios::in);
    if (getTotalPages() == 0)
        saveCounters();
    else
        readCounters();
    if (_fs)
        return 0;
    return -1;
}

RC FileHandle::closeFile()
{
    saveCounters();
    _fs.close();
    return 0;
    // @TODO: logic for failed operation to return -1
}

RC FileHandle::readPage(PageNum pageNum, void *data)
{
    if ((unsigned)pageNum >= ((unsigned)getNumberOfPages()))
        return -1;
    // @todo: add corner cases
    unsigned readPt = (pageNum + 1) * PAGE_SIZE;
    _fs.seekg(readPt);
    // @todo: scenario where data size != PAGE_SIZE
    // @todo: Verify write is successful
    _fs.read((char *)data, PAGE_SIZE);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    if ((unsigned)pageNum >= ((unsigned)getNumberOfPages()))
        return -1;
    // @todo: check if data size is exactly PAGE_SIZE
    unsigned writePt = (pageNum + 1) * PAGE_SIZE;
    _fs.seekp(writePt);
    char *data_n = (char *)data;
    _fs.write(data_n, PAGE_SIZE);
    writePageCounter++;
    return 0;
}

RC FileHandle::appendPage(const void *data)
{
    _fs.seekp(0, ios::end);
    // @todo: check if seekp is multiple of PAGE_SIZE
    // @todo: check that *data exactly the size of PAGE_SIZE
    char *data_n = (char *)data;
    _fs.write(data_n, PAGE_SIZE);
    // @todo: verify the file size is now still a multiple of PAGE_SIZE after page append
    appendPageCounter++;
    return 0;
}

unsigned FileHandle::getFileSize()
{
    // @todo: Edge cases like file doesn't exist etc
    _fs.seekg(0, ios::beg);
    unsigned fsize_i = _fs.tellg();
    _fs.seekg(0, ios::end);
    unsigned fsize_e = _fs.tellg();
    unsigned fsize = fsize_e - fsize_i;
    return fsize;
}

unsigned FileHandle::getTotalPages()
{
    unsigned fileSize = getFileSize();
    unsigned numPages = fileSize / PAGE_SIZE;
    return numPages;
}

unsigned FileHandle::getNumberOfPages()
{
    return appendPageCounter;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
    return 0;
}

unsigned FileHandle::saveCounters()
{
    void *data = malloc(PAGE_SIZE);
    memcpy((char *)data, (char *)&readPageCounter, sizeof(unsigned));
    memcpy((char *)data + sizeof(unsigned), (char *)&writePageCounter, sizeof(unsigned));
    memcpy((char *)data + 2 * sizeof(unsigned), (char *)&appendPageCounter, sizeof(unsigned));
    _fs.seekp(0, ios::beg);
    _fs.write((char *)data, PAGE_SIZE);
    free(data);
    return 0;
}

unsigned FileHandle::readCounters()
{
    void *data = malloc(PAGE_SIZE);
    _fs.seekg(0, ios::beg);
    _fs.read((char *)data, PAGE_SIZE);
    memcpy((char *)&readPageCounter, (char *)data, sizeof(unsigned));
    memcpy((char *)&writePageCounter, (char *)data + sizeof(unsigned), sizeof(unsigned));
    memcpy((char *)&appendPageCounter, (char *)data + 2 * sizeof(unsigned), sizeof(unsigned));
    free(data);
    return 0;
}