
// Copyright (c) 2014-2015 The Dash developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef THRONE_H
#define THRONE_H

#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "core.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "main.h"
#include "throne-pos.h"

#define THRONE_NOT_PROCESSED               0 // initial state
#define THRONE_IS_CAPABLE                  1
#define THRONE_NOT_CAPABLE                 2
#define THRONE_STOPPED                     3
#define THRONE_INPUT_TOO_NEW               4
#define THRONE_PORT_NOT_OPEN               6
#define THRONE_PORT_OPEN                   7
#define THRONE_SYNC_IN_PROCESS             8
#define THRONE_REMOTELY_ENABLED            9

#define THRONE_MIN_CONFIRMATIONS           15
#define THRONE_MIN_DSEEP_SECONDS           (30*60)
#define THRONE_MIN_DSEE_SECONDS            (5*60)
#define THRONE_PING_SECONDS                (1*60)
#define THRONE_EXPIRATION_SECONDS          (65*60)
#define THRONE_REMOVAL_SECONDS             (70*60)

using namespace std;

class CThrone;
class CThronePayments;
class CThronePaymentWinner;

extern CCriticalSection cs_thronepayments;
extern map<uint256, CThronePaymentWinner> mapSeenThroneVotes;
extern map<int64_t, uint256> mapCacheBlockHashes;
extern CThronePayments thronePayments;

void ProcessMessageThronePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool GetBlockHash(uint256& hash, int nBlockHeight);

//
// The Throne Class. For managing the Darksend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CThrone
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        THRONE_ENABLED = 1,
        THRONE_EXPIRED = 2,
        THRONE_VIN_SPENT = 3,
        THRONE_REMOVE = 4,
        THRONE_POS_ERROR = 5
    };

    CTxIn vin;
    CService addr;
    CPubKey pubkey;
    CPubKey pubkey2;
    std::vector<unsigned char> sig;
    int activeState;
    int64_t sigTime; //dsee message times
    int64_t lastDseep;
    int64_t lastTimeSeen;
    int cacheInputAge;
    int cacheInputAgeBlock;
    bool unitTest;
    bool allowFreeTx;
    int protocolVersion;
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int nVote;
    int64_t lastVote;
    int nScanningErrorCount;
    int nLastScanningErrorBlockHeight;

    CThrone();
    CThrone(const CThrone& other);
    CThrone(CService newAddr, CTxIn newVin, CPubKey newPubkey, std::vector<unsigned char> newSig, int64_t newSigTime, CPubKey newPubkey2, int protocolVersionIn);

    void swap(CThrone& first, CThrone& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubkey, second.pubkey);
        swap(first.pubkey2, second.pubkey2);
        swap(first.sig, second.sig);
        swap(first.activeState, second.activeState);
        swap(first.sigTime, second.sigTime);
        swap(first.lastDseep, second.lastDseep);
        swap(first.lastTimeSeen, second.lastTimeSeen);
        swap(first.cacheInputAge, second.cacheInputAge);
        swap(first.cacheInputAgeBlock, second.cacheInputAgeBlock);
        swap(first.unitTest, second.unitTest);
        swap(first.allowFreeTx, second.allowFreeTx);
        swap(first.protocolVersion, second.protocolVersion);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nVote, second.nVote);
        swap(first.lastVote, second.lastVote);
        swap(first.nScanningErrorCount, second.nScanningErrorCount);
        swap(first.nLastScanningErrorBlockHeight, second.nLastScanningErrorBlockHeight);
    }

    CThrone& operator=(CThrone from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CThrone& a, const CThrone& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CThrone& a, const CThrone& b)
    {
        return !(a.vin == b.vin);
    }

    uint256 CalculateScore(int mod=1, int64_t nBlockHeight=0);

    IMPLEMENT_SERIALIZE
    (
        // serialized format:
        // * version byte (currently 0)
        // * all fields (?)
        {
                LOCK(cs);
                unsigned char nVersion = 0;
                READWRITE(nVersion);
                READWRITE(vin);
                READWRITE(addr);
                READWRITE(pubkey);
                READWRITE(pubkey2);
                READWRITE(sig);
                READWRITE(activeState);
                READWRITE(sigTime);
                READWRITE(lastDseep);
                READWRITE(lastTimeSeen);
                READWRITE(cacheInputAge);
                READWRITE(cacheInputAgeBlock);
                READWRITE(unitTest);
                READWRITE(allowFreeTx);
                READWRITE(protocolVersion);
                READWRITE(nLastDsq);
                READWRITE(nVote);
                READWRITE(lastVote);
                READWRITE(nScanningErrorCount);
                READWRITE(nLastScanningErrorBlockHeight);
        }
    )

    void UpdateLastSeen(int64_t override=0)
    {
        if(override == 0){
            lastTimeSeen = GetAdjustedTime();
        } else {
            lastTimeSeen = override;
        }
    }

    inline uint64_t SliceHash(uint256& hash, int slice)
    {
        uint64_t n = 0;
        memcpy(&n, &hash+slice*64, 64);
        return n;
    }

    void Check();

    bool UpdatedWithin(int seconds)
    {
        // LogPrintf("UpdatedWithin %d, %d --  %d \n", GetAdjustedTime() , lastTimeSeen, (GetAdjustedTime() - lastTimeSeen) < seconds);

        return (GetAdjustedTime() - lastTimeSeen) < seconds;
    }

    void Disable()
    {
        lastTimeSeen = 0;
    }

    bool IsEnabled()
    {
        return activeState == THRONE_ENABLED;
    }

    int GetThroneInputAge()
    {
        if(chainActive.Tip() == NULL) return 0;

        if(cacheInputAge == 0){
            cacheInputAge = GetInputAge(vin);
            cacheInputAgeBlock = chainActive.Tip()->nHeight;
        }

        return cacheInputAge+(chainActive.Tip()->nHeight-cacheInputAgeBlock);
    }

    void ApplyScanningError(CThroneScanningError& mnse)
    {
        if(!mnse.IsValid()) return;

        if(mnse.nBlockHeight == nLastScanningErrorBlockHeight) return;
        nLastScanningErrorBlockHeight = mnse.nBlockHeight;

        if(mnse.nErrorType == SCANNING_SUCCESS){
            nScanningErrorCount--;
            if(nScanningErrorCount < 0) nScanningErrorCount = 0;
        } else { //all other codes are equally as bad
            nScanningErrorCount++;
            if(nScanningErrorCount > THRONE_SCANNING_ERROR_THESHOLD*2) nScanningErrorCount = THRONE_SCANNING_ERROR_THESHOLD*2;
        }
    }

    std::string Status() {
        std::string strStatus = "ACTIVE";

        if(activeState == CThrone::THRONE_ENABLED) strStatus   = "ENABLED";
        if(activeState == CThrone::THRONE_EXPIRED) strStatus   = "EXPIRED";
        if(activeState == CThrone::THRONE_VIN_SPENT) strStatus = "VIN_SPENT";
        if(activeState == CThrone::THRONE_REMOVE) strStatus    = "REMOVE";
        if(activeState == CThrone::THRONE_POS_ERROR) strStatus = "POS_ERROR";

        return strStatus;
    }

};

// for storing the winning payments
class CThronePaymentWinner
{
public:
    int nBlockHeight;
    CTxIn vin;
    CScript payee;
    std::vector<unsigned char> vchSig;
    uint64_t score;

    CThronePaymentWinner() {
        nBlockHeight = 0;
        score = 0;
        vin = CTxIn();
        payee = CScript();
    }

    uint256 GetHash(){
        uint256 n2 = Hash(BEGIN(nBlockHeight), END(nBlockHeight));
        uint256 n3 = vin.prevout.hash > n2 ? (vin.prevout.hash - n2) : (n2 - vin.prevout.hash);

        return n3;
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vin);
        READWRITE(score);
        READWRITE(vchSig);
     )
};

//
// Throne Payments Class
// Keeps track of who should get paid for which blocks
//

class CThronePayments
{
private:
    std::vector<CThronePaymentWinner> vWinning;
    int nSyncedFromPeer;
    std::string strMasterPrivKey;
    std::string strTestPubKey;
    std::string strMainPubKey;
    bool enabled;
    int nLastBlockHeight;

public:

    CThronePayments() {
        strMainPubKey = "03e9bcaecb27ba0c09c04046f1cd7613006880ade95412c30c8afbade1475dabd6";
        strTestPubKey = "03e9bcaecb27ba0c09c04046f1cd7613006880ade95412c30c8afbade1475dabd6";
        enabled = false;
    }

    bool SetPrivKey(std::string strPrivKey);
    bool CheckSignature(CThronePaymentWinner& winner);
    bool Sign(CThronePaymentWinner& winner);

    // Deterministically calculate a given "score" for a throne depending on how close it's hash is
    // to the blockHeight. The further away they are the better, the furthest will win the election
    // and get paid this block
    //

    uint64_t CalculateScore(uint256 blockHash, CTxIn& vin);
    bool GetWinningThrone(int nBlockHeight, CTxIn& vinOut);
    bool AddWinningThrone(CThronePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);
    void Relay(CThronePaymentWinner& winner);
    void Sync(CNode* node);
    void CleanPaymentList();
    int LastPayment(CThrone& mn);

    //slow
    bool GetBlockPayee(int nBlockHeight, CScript& payee);
};


#endif
