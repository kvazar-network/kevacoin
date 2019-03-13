// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <primitives/block.h>
#include <streams.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>

extern "C" void cn_slow_hash(const void *data, size_t length, char *hash, int variant, int prehashed, uint64_t height);

const uint256 CBlockHeader::DIFFICULTY_1 = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

uint256 CBlockHeader::GetHash() const
{
    CHashWriter hashWriter(SER_GETHASH, PROTOCOL_VERSION);
    hashWriter.write(BEGIN(nVersion), 80);
    return hashWriter.GetHash();
}

uint256 CBlockHeader::GetPoWHash() const
{
    uint256 thash;
    if (hashPrevBlock.IsNull()) {
        // Genesis block
        cn_slow_hash(BEGIN(nVersion), 80, BEGIN(thash), 2, 0, 0);
        return thash;
    }

    if (!IsAuxpow()) {
        // prev_id of CN header is used to store the kevacoin block hash.
        // The value of prev_id and block hash must be the same to prove
        // that PoW has been properly done.
        if (GetHash() != cnHeader.prev_id) {
            return DIFFICULTY_1;
        }
        cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(cnHeader);
        cn_slow_hash(blob.data(), blob.size(), BEGIN(thash), 2, 0, 0);
        return thash;
    }

    // Merged mining.
    cryptonote::tx_extra_keva_blockhash keva_blockhash;
    if (!cryptonote::get_keva_blockhash_from_extra(cnHeader.aux_pow_ptr->miner_tx.extra, keva_blockhash)) {
        return DIFFICULTY_1;
    }
    uint256 actualHash = GetHash();
    if (memcmp(keva_blockhash.merkle_root.data, actualHash.begin(), thash.size()) != 0) {
        return DIFFICULTY_1;
    }

    crypto::hash miner_tx_hash;
    if (!cryptonote::get_transaction_hash(cnHeader.aux_pow_ptr->miner_tx, miner_tx_hash)) {
        return DIFFICULTY_1;
    }

    crypto::hash aux_blocks_merkle_root;
    crypto::tree_hash_from_branch(
        reinterpret_cast<const char (*)[32]>(cnHeader.aux_pow_ptr->merkle_branch.data()),
        cnHeader.aux_pow_ptr->merkle_branch.size(),
        reinterpret_cast<char*>(&miner_tx_hash), 0,
        reinterpret_cast<char*>(&aux_blocks_merkle_root));

    if (memcmp(aux_blocks_merkle_root.data, cnHeader.merkle_root.begin(), cnHeader.merkle_root.size()) != 0) {
        return DIFFICULTY_1;
    }
    cryptonote::blobdata blob = cryptonote::t_serializable_object_to_blob(cnHeader);
    cn_slow_hash(blob.data(), blob.size(), BEGIN(thash), 2, 0, 0);
    return thash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
