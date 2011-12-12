
#ifndef BLOCKFILE_H
#define BLOCKFILE_H

#include "btc/bignum.h"
#include "btc/key.h"
#include "btc/script.h"
#include "btc/tx.h"

#include <boost/noncopyable.hpp>
#include <list>

/// BlockFile encapsulates the Block file on the disk. It supports different queries to the block file.

class CBlock;
class CBlockIndex;
class CTransaction;
class CDiskTxPos;

class BlockFile : private boost::noncopyable
{
public:
    BlockFile() : _currentBlockFile(1) {} /// load the block chain index from file

    bool writeToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet);
    bool readFromDisk(CBlock& block, unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true);
    bool readFromDisk(CBlock& block, const CBlockIndex* pindex, bool fReadTransactions=true);
    
    bool readFromDisk(CTransaction& tx, CDiskTxPos pos, FILE** pfileRet=NULL);    
    
    bool eraseBlockFromDisk(CBlockIndex bindex);

    static bool checkDiskSpace(uint64 nAdditionalBytes=0);

protected:
    FILE* openBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
    FILE* appendBlockFile(unsigned int& nFileRet);
    //    bool loadBlockIndex(bool fAllowNew=true);
    
private:
    FILE* _blockFile;    
    unsigned int _currentBlockFile;
};


#endif // BLOCKFILE_H