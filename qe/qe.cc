#include <numeric>
#include <cmath>
#include <set>
#include "qe.h"
#include <string.h>
#include <iostream>
#include "qe.h"
#include <math.h>
#include <string.h>
#include <algorithm> // std::min_element
#include <iterator>  // std::begin, std::end

using namespace std;

int getRecordContentDataSize(const vector<Attribute> &recordDescriptor, const void *data)
{
    // returns actual recordDataSize EXCLUDING THE OFFSET SIZE!!!!!!
    int noOfFields = recordDescriptor.size();
    int bitMapSize = ceil(noOfFields / 8.0);
    int offset = bitMapSize;
    unsigned char *nullbytes = new unsigned char[bitMapSize];
    memcpy(nullbytes, data, bitMapSize * sizeof(char));
    for (int i = 0; i < noOfFields; ++i)
    {
        if ((nullbytes[i / 8] >> (7 - i % 8)) & 1)
            continue;
        const Attribute &atrb = recordDescriptor[i];
        if (atrb.type == TypeVarChar)
        {
            int length;
            memcpy(&length, (char *)data + offset, sizeof(int));
            offset += length;
        }
        offset += 4;
    }
    delete[] nullbytes;
    return offset - bitMapSize;
}

RC fetchAttrData(void *recordData, vector<Attribute> &attrs, string attribName, void *data)
{
    // fetches single attribute data from record data (*data)
    int sz = attrs.size();
    bool attributeExist = false;
    for (const Attribute &attrb : attrs)
        if (attrb.name == attribName)
            attributeExist = true;
    if (!attributeExist)
        return -1;
    int offset = 0;
    int nullIndicatorSize = ceil((double)attrs.size() / 8);
    offset += nullIndicatorSize;
    // int attribIndex;
    // for(int i=0; i< attrs.size(); i++) {
    //     if attribName ==
    // }
    for (int i = 0; i < attrs.size(); i++)
    {
        if (!(((char *)recordData)[i / 8] & 1 << (7 - i % 8)))
        {
            if (attrs[i].type == TypeVarChar)
            {
                int strLen;
                memcpy(&strLen, (char *)recordData + offset, sizeof(int));
                strLen += 4;
                memset(data, 0, 1);
                memcpy((char *)data + 1, (char *)recordData + offset, strLen);
                offset += strLen;
            }
            else
            {
                memset(data, 0, 1);
                memcpy((char *)data + 1, (char *)recordData + offset, sizeof(int));
                offset += 4;
            }
        }
        else
        {
            memset(data, 1, 1);
        }
        if (attrs[i].name == attribName)
            return 0;
    }
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

bool isConditionMet(void *data, CompOp op, Value compareData)
{
    if (op == NO_OP)
        return true;
    if (compareData.type == TypeVarChar)
    {
        int strLen;
        memcpy(&strLen, (char *)data + 1, sizeof(int));
        strLen += 4;
        void *strBuffer = malloc(strLen);
        memcpy(strBuffer, (char *)data + 1, strLen);
        // @note: We're assuming here that the value is in the form of "8Anteater"
        int cmpStringVal = memcmp((char *)strBuffer + sizeof(int), (char *)compareData.data + sizeof(int), strLen - 4);
        free(strBuffer);

        if (cmpStringVal == 0 & (op == EQ_OP || op == LE_OP || op == GE_OP))
            return true;

        else if (cmpStringVal < 0 && (op == LT_OP || op == LE_OP || op == NE_OP))
            return true;

        else if (cmpStringVal > 0 && (op == GT_OP || op == GE_OP || op == NE_OP))
            return true;

        else
            return false;
    }
    else
    {
        if (compareData.type == TypeInt)
        {
            int givenValue;
            int recordValue;
            memcpy(&recordValue, (char *)data + 1, sizeof(int));
            memcpy(&givenValue, (char *)compareData.data, sizeof(int));
            return compare<int>(givenValue, recordValue, op);
        }
        else if (compareData.type == TypeReal)
        {
            float givenValue;
            float recordValue;
            memcpy(&recordValue, (char *)data + 1, sizeof(float));
            memcpy(&givenValue, (char *)compareData.data, sizeof(float));
            return compare<float>(givenValue, recordValue, op);
        }
    }
}

Filter::Filter(Iterator *input, const Condition &condition)
    : _itr(input), _condition(condition)
{
}

void Filter::getAttributes(vector<Attribute> &attrs) const
{
    return _itr->getAttributes(attrs);
}

RC Filter::getNextTuple(void *data)
{
    RC resp = 0;
    vector<Attribute> attrs;
    _itr->getAttributes(attrs);
    while (resp != RM_EOF)
    {
        RC resp = _itr->getNextTuple(data);
        if (resp)
            return resp;
        void *attrData = malloc(PAGE_SIZE);
        int status = fetchAttrData(data, attrs, _condition.lhsAttr, attrData);
        if (status)
            return status;
        if (!_condition.bRhsIsAttr)
        {
            if (isConditionMet(attrData, _condition.op, _condition.rhsValue))
            {
                // if true then copy data
                free(attrData);
                return 0;
            }
            else
            {
                free(attrData);
                return getNextTuple(data);
            }
        }
        else
        {
            // do we need to filter other table's attribute here?
            return -1;
        }
    }
    return -1;
}

Project::Project(Iterator *input, // Iterator of input R
                 const vector<string> &attrNames)
    : _itr(input), _attrNames(attrNames)
{
    vector<Attribute> fattrs;
    _itr->getAttributes(fattrs);

    set<string> attrset(_attrNames.begin(), _attrNames.end());

    for (int i = 0; i < fattrs.size(); ++i)
    {
        if (attrset.end() == attrset.find(fattrs[i].name))
            continue;
        _attribs.push_back(fattrs[i]);
    }
}; // vector containing attribute names

RC Project::getNextTuple(void *data)
{
    // call filter's getNextTuple
    // return data of only those attributes who are passed in Project's constructor
    vector<Attribute> readAttribs;
    _itr->getAttributes(readAttribs);
    RC resp = 0;

    void *recordData = malloc(PAGE_SIZE);
    resp = _itr->getNextTuple(recordData);
    if (resp)
        return resp;
    int writeNullByteSize = ceil((double)_attribs.size() / 8);
    int readNullByteSize = ceil((double)readAttribs.size() / 8);
    // make all the null bytes as 0. Only flip to 1 when we see an actual null
    memset(data, 0, writeNullByteSize);
    int writeAttribIdx = 0;
    int readOffset = readNullByteSize;
    int writeOffset = writeNullByteSize;

    for (int i = 0; i < readAttribs.size(); i++)
    {
        // if attrs match, then copy..
        if (readAttribs[i].name == _attribs[writeAttribIdx].name)
        {
            if (!(((char *)recordData)[i / 8] & 1 << (7 - i % 8)))
            {
                if (_attribs[writeAttribIdx].type == TypeVarChar)
                {
                    int strLen;
                    memcpy(&strLen, (char *)recordData + readOffset, sizeof(int));
                    strLen += 4;
                    memcpy((char *)data + writeOffset, (char *)recordData + readOffset, strLen);
                    writeOffset += strLen;
                }
                else
                {
                    memcpy((char *)data + writeOffset, (char *)recordData + readOffset, sizeof(int));
                    writeOffset += 4;
                }
            }
            else
            {
                ((char *)data)[writeAttribIdx / 8] |= (1 << (7 - writeAttribIdx) % 8);
            }
            writeAttribIdx += 1;
        }
        if (readAttribs[i].type == TypeVarChar)
        {
            int strLen;
            memcpy(&strLen, (char *)recordData + readOffset, sizeof(int));
            strLen += 4;
            readOffset += strLen;
        }
        else
        {
            readOffset += 4;
        }
    }
    return 0;
}

void Project::getAttributes(vector<Attribute> &attrs) const
{
    attrs = _attribs;
};

BNLJoin::BNLJoin(Iterator *leftIn,           // Iterator of input R
                 TableScan *rightIn,         // TableScan Iterator of input S
                 const Condition &condition, // Join condition
                 const unsigned numPages     // # of pages that can be loaded into memory,
                                             //   i.e., memory block size (decided by the optimizer)
                 )
    : _leftin(leftIn), _rightin(rightIn), _condition(condition), _numPages(numPages){};

RC BNLJoin::getNextTuple(void *data)
{
    void *lRecordData = malloc(PAGE_SIZE);
    void *rRecordData = malloc(PAGE_SIZE);
    vector<Attribute> lAttrs, rAttrs;
    _leftin->getAttributes(lAttrs);
    _rightin->getAttributes(rAttrs);
    Value rAttrValueObj; // very bad name :(
    for (int i = 0; i < rAttrs.size(); i++)
    {
        if (rAttrs[i].name == _condition.rhsAttr)
        {
            rAttrValueObj.type = rAttrs[i].type;
            break;
        }
    }
    while (_leftin->getNextTuple(lRecordData) != EOF)
    {
        if (_firstCall)
        {
            _rightin->setIterator();
            _firstCall = false;
        }
        while (_rightin->getNextTuple(rRecordData) != EOF)
        {
            void *lData = malloc(PAGE_SIZE);
            void *rData = malloc(PAGE_SIZE);
            rAttrValueObj.data = (char *)rData + 1;

            fetchAttrData(lRecordData, lAttrs, _condition.lhsAttr, lData);
            fetchAttrData(rRecordData, rAttrs, _condition.rhsAttr, rData);

            if (isConditionMet(lData, _condition.op, rAttrValueObj))
            {
                int len = getRecordContentDataSize(lAttrs, lRecordData);
                // join lhs rhs data
                int nlhs = lAttrs.size();
                int nrhs = rAttrs.size();
                int nnullbytes = ceil((nlhs + nrhs) / 8.0);
                char *nb1 = new char[nnullbytes];
                memset(nb1, 0, nnullbytes);
                char *nb2 = new char[nnullbytes];
                memset(nb2, 0, nnullbytes);

                int nnb1 = ceil(nlhs / 8.0);
                int nnb2 = ceil(nrhs / 8.0);
                memcpy((char *)nb1, (char *)lRecordData, nnb1);
                memcpy((char *)nb2, (char *)rRecordData, nnb2);

                *nb1 = *nb1 | (*nb2 >> nlhs);
                memcpy((char *)data, (char *)nb1, nnullbytes);
                int ofst = nnullbytes;
                memcpy((char *)data + ofst, (char *)lRecordData + nnb1, len);
                ofst += len;
                int len2 = getRecordContentDataSize(rAttrs, rRecordData);
                memcpy((char *)data + ofst, (char *)rRecordData + nnb2, len2);
                ofst += len2;
                free(lData);
                free(rData);
                free(lRecordData);
                free(rRecordData);
                return 0;
            }
        }

        //        else
        //        {
        //            free(lRecordData);
        //            free(rRecordData);
        ////            return getNextTuple(data);
        //        }
        _firstCall = true;
    }
    free(lRecordData);
    free(rRecordData);
    return QE_EOF;
};

void BNLJoin::getAttributes(vector<Attribute> &attrs) const
{
    vector<Attribute> lattrs;
    _leftin->getAttributes(lattrs);

    vector<Attribute> rattrs;
    _rightin->getAttributes(rattrs);

    attrs.insert(attrs.end(), lattrs.begin(), lattrs.end());
    attrs.insert(attrs.end(), rattrs.begin(), rattrs.end());
};

Aggregate::Aggregate(Iterator *input,   // Iterator of input R
                     Attribute aggAttr, // The attribute over which we are computing an aggregate
                     AggregateOp op     // Aggregate operation
                     )
    : _itr(input), _attr(aggAttr), _op(op)
{
}

RC Aggregate::getNextTuple(void *data)
{
    if (_EOF_reached)
        return -1;
    vector<float> values;
    int recordCount = 0;
    float sum = 0;
    void *recordData = malloc(PAGE_SIZE);
    vector<Attribute> attributes;
    _itr->getAttributes(attributes);
    while (_itr->getNextTuple(recordData) != EOF)
    {
        int valInt;
        float valFloat;
        void *attrData = malloc(10);
        fetchAttrData(recordData, attributes, _attr.name, attrData);
        if (!(((char *)attrData)[0 / 8] & 1 << (7 - 0 % 8)))
        {
            // if not null then:
            if (_attr.type == TypeInt)
            {
                memcpy((char *)&valInt, (char *)attrData + 1, sizeof(int));
                values.push_back((float)valInt);
                sum += valInt;
            }
            else
            {
                memcpy((char *)&valFloat, (char *)attrData + 1, sizeof(int));
                values.push_back((float)valFloat);
                sum += valFloat;
            }
        }
        recordCount += 1;
    }

    float avgValue = -1.0;
    if (recordCount > 0)
    {
        avgValue = sum / recordCount;
    }
    float aggValue;
    if (_op == MIN)
    {
        aggValue = *min_element(values.begin(), values.end());
    }
    else if (_op == MAX)
    {
        aggValue = *max_element(values.begin(), values.end());
    }
    else if (_op == COUNT)
    {
        aggValue = (float)values.size();
    }
    else if (_op == SUM)
    {
        aggValue = sum;
    }
    else if (_op == AVG)
    {
        aggValue = avgValue;
    }
    else
    {
        cout << "NO OP specified";
    }
    memset((char *)data, 0, 1);
    memcpy((char *)data + 1, (char *)&aggValue, sizeof(float));
    _EOF_reached = true;
    return 0;
}

void Aggregate::getAttributes(vector<Attribute> &attrs) const
{
    Attribute oattr = _attr;
    string aname;
    switch (_op)
    {
    case MIN:
        aname = "MIN";
        break;
    case MAX:
        aname = "MAX";
        break;
    case COUNT:
        aname = "COUNT";
        break;
    case SUM:
        aname = "SUM";
        break;
    case AVG:
        aname = "AVG";
        break;
    default:
        aname = "NONE";
        break;
    }
    aname = aname + "(" + oattr.name + ")";
    oattr.name = aname;

    if (_op == COUNT)
        oattr.type = TypeInt;

    if (_op == AVG)
        oattr.type = TypeReal;

    attrs.push_back(oattr);
};