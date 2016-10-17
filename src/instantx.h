
// Copyright (c) 2009-2012 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef INSTANTX_H
#define INSTANTX_H

<<<<<<< HEAD
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "main.h"
#include "spork.h"

/*
    At 15 signatures, 1/2 of the masternode network can be owned by
    one party without comprimising the security of InstantX
    (1000/2150.0)**10 = 0.00047382219560689856
    (1000/2900.0)**10 = 2.3769498616783657e-05

    ### getting 5 of 10 signatures w/ 1000 nodes of 2900
    (1000/2900.0)**5 = 0.004875397277841433
*/
#define INSTANTX_SIGNATURES_REQUIRED           6
#define INSTANTX_SIGNATURES_TOTAL              10
=======
#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
>>>>>>> origin/dirty-merge-dash-0.11.0

using namespace std;
using namespace boost;

class CConsensusVote;
class CTransaction;
class CTransactionLock;

<<<<<<< HEAD
static const int MIN_INSTANTX_PROTO_VERSION = 70103;
=======
static const int MIN_INSTANTX_PROTO_VERSION = 70066;
>>>>>>> origin/dirty-merge-dash-0.11.0

extern map<uint256, CTransaction> mapTxLockReq;
extern map<uint256, CTransaction> mapTxLockReqRejected;
extern map<uint256, CConsensusVote> mapTxLockVote;
extern map<uint256, CTransactionLock> mapTxLocks;
extern std::map<COutPoint, uint256> mapLockedInputs;
extern int nCompleteTXLocks;


int64_t CreateNewLock(CTransaction tx);

bool IsIXTXValid(const CTransaction& txCollateral);

// if two conflicting locks are approved by the network, they will cancel out
bool CheckForConflictingLocks(CTransaction& tx);

void ProcessMessageInstantX(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

//check if we need to vote on this transaction
void DoConsensusVote(CTransaction& tx, int64_t nBlockHeight);

//process consensus vote message
<<<<<<< HEAD
bool ProcessConsensusVote(CNode *pnode, CConsensusVote& ctx);
=======
bool ProcessConsensusVote(CConsensusVote& ctx);
>>>>>>> origin/dirty-merge-dash-0.11.0

// keep transaction locks in memory for an hour
void CleanTransactionLocksList();

int64_t GetAverageVoteTime();

class CConsensusVote
{
public:
<<<<<<< HEAD
    CTxIn vinMasternode;
    uint256 txHash;
    int nBlockHeight;
    std::vector<unsigned char> vchMasterNodeSignature;
=======
    CTxIn vinThrone;
    uint256 txHash;
    int nBlockHeight;
    std::vector<unsigned char> vchThroNeSignature;
>>>>>>> origin/dirty-merge-dash-0.11.0

    uint256 GetHash() const;

    bool SignatureValid();
    bool Sign();

<<<<<<< HEAD
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txHash);
        READWRITE(vinMasternode);
        READWRITE(vchMasterNodeSignature);
        READWRITE(nBlockHeight);
    }
=======
    IMPLEMENT_SERIALIZE
    (
        READWRITE(txHash);
        READWRITE(vinThrone);
        READWRITE(vchThroNeSignature);
        READWRITE(nBlockHeight);
    )
>>>>>>> origin/dirty-merge-dash-0.11.0
};

class CTransactionLock
{
public:
    int nBlockHeight;
    uint256 txHash;
    std::vector<CConsensusVote> vecConsensusVotes;
    int nExpiration;
    int nTimeout;

    bool SignaturesValid();
    int CountSignatures();
    void AddSignature(CConsensusVote& cv);

    uint256 GetHash()
    {
        return txHash;
    }
};


<<<<<<< HEAD
#endif
=======
#endif
>>>>>>> origin/dirty-merge-dash-0.11.0
