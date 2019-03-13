// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <utilstrencodings.h>

#include <cryptonote_basic/cryptonote_format_utils.h>

class CAuxPow
{
public:
    // Cryptnote coinbase tx, which contains the block hash of kevacoin block.
    cryptonote::transaction miner_tx;

    // Merkle branch is used to establish that miner_tx is part of the
    // merkel tree whose root is merkle_root.
    std::vector<crypto::hash>  merkle_branch;

    // load
    template <template <bool> class Archive>
    bool do_serialize(Archive<false>& ar)
    {
        FIELD(miner_tx)
        FIELD(merkle_branch)
        return true;
    }

    // store
    template <template <bool> class Archive>
    bool do_serialize(Archive<true>& ar)
    {
        FIELD(miner_tx)
        FIELD(merkle_branch)
        return true;
    }
};

/**
 * This header is to store the proof-of-work of cryptonote mining.
 * Kevacoin uses Cryptonight PoW and uses its existing infrastructure
 * (mining pools and miners)
 */
class CryptoNoteHeader
{
public:
    uint8_t major_version;
    uint8_t minor_version;  // now used as a voting mechanism, rather than how this particular block is built
    uint64_t timestamp;
    uint256  prev_id;
    uint32_t nonce;
    uint256  merkle_root;
    size_t   nTxes; // Number of transactions.

private:
    bool is_aux_block;

public:
    std::shared_ptr<CAuxPow> aux_pow_ptr; // It is used only is_aux_block is true.

    CryptoNoteHeader() : is_aux_block(false)
    {
        SetNull();
    }

    void SetAuxBlock(bool isAuxBlock)
    {
        is_aux_block = isAuxBlock;
    }

    bool IsAuxBlock()
    {
        return is_aux_block;
    }

    void SetNull()
    {
        major_version = 0;
        minor_version = 0;
        prev_id.SetNull();
        timestamp = 0;
        nonce = 0;
        merkle_root.SetNull();
        nTxes = 0;
    }

    bool IsNull() const
    {
        return (timestamp == 0);
    }

    // load
    template <template <bool> class Archive>
    bool do_serialize(Archive<false>& ar)
    {
      VARINT_FIELD(major_version)
      VARINT_FIELD(minor_version)
      VARINT_FIELD(timestamp)
      crypto::hash prev_hash;
      FIELD(prev_hash)
      memcpy(prev_id.begin(), &prev_hash, prev_id.size());
      FIELD(nonce)
      crypto::hash merkle_hash;
      FIELD(merkle_hash)
      memcpy(merkle_root.begin(), &merkle_hash, merkle_root.size());
      VARINT_FIELD(nTxes)
      if (is_aux_block) {
        aux_pow_ptr = std::shared_ptr<CAuxPow>(new CAuxPow());
        FIELD(*aux_pow_ptr)
      }
      return true;
    }

    // store
    template <template <bool> class Archive>
    bool do_serialize(Archive<true>& ar)
    {
      VARINT_FIELD(major_version)
      VARINT_FIELD(minor_version)
      VARINT_FIELD(timestamp)
      crypto::hash prev_hash;
      memcpy(&prev_hash, prev_id.begin(), prev_id.size());
      FIELD(prev_hash)
      FIELD(nonce)
      crypto::hash merkle_hash;
      memcpy(&merkle_hash, merkle_root.begin(), merkle_root.size());
      FIELD(merkle_hash)
      VARINT_FIELD(nTxes)
      if (is_aux_block) {
        FIELD(*aux_pow_ptr)
      }
      return true;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead()) {
            std::string blob;
            READWRITE(blob);
            std::stringstream ss;
            ss << blob;
            // load
            binary_archive<false> ba(ss);
            ::serialization::serialize(ba, *this);
        } else {
            std::stringstream ss;
            // store
            binary_archive<true> ba(ss);
            ::serialization::serialize(ba, *this);
            std::string blob = ss.str();
            READWRITE(blob);
        }
    }
};

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    // nNonce is used to store chainID in merged mining.
    uint32_t nNonce;

    // CryptoNote header for emulation or merged mining
    CryptoNoteHeader cnHeader;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        // Genesis block does not have cnHeader.
        if (!hashPrevBlock.IsNull()) {
            cnHeader.SetAuxBlock(IsAuxpow());
            READWRITE(cnHeader);
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    uint256 GetPoWHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    static const int32_t DEFAULT_AUXPOW_CHAIN_ID = 0x000000AD;

    // ChainID is used for merged mining.
    inline void SetChainID(uint32_t chainID = CBlockHeader::DEFAULT_AUXPOW_CHAIN_ID)
    {
        nNonce = chainID;
    }

    inline uint32_t GetChainID() const
    {
        return nNonce;
    }

    /**
     * Check if the auxpow flag is set in the version.
     * @return True iff this block version is marked as auxpow.
     */
    inline bool IsAuxpow() const
    {
        return nNonce != 0;
    }

    static const uint256 DIFFICULTY_1;
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        //*((CBlockHeader*)this) = header;
        nVersion       = header.nVersion;
        hashPrevBlock  = header.hashPrevBlock;
        hashMerkleRoot = header.hashMerkleRoot;
        nTime          = header.nTime;
        nBits          = header.nBits;
        nNonce         = header.nNonce;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
