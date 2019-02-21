// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>
//#include <crypto/scrypt.h>
extern "C"
{
    #include <cryptonight/hash-ops.h>
}

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash() const
{
    uint256 thash;
    // Convert it to Cryptonight header format.
    uint8_t cnHeader[76] = {};
    // Bitcoin header size is 80 bytes, while CN is 76 bytes.
    // To reduce it to 76, combine nVersion(4 bytes) and hashPrevBlock(32 byte)
    // into 32 byte SHA2 hash.
    uint8_t combineHash[32] = {};
    CSHA256().Write((uint8_t*)BEGIN(nVersion), 36).Finalize(combineHash);
    memcpy(cnHeader, combineHash, sizeof(combineHash));
    uint32_t offset = sizeof(combineHash);
    // Copy 7 bytes of hashMerkleRoot
    memcpy(cnHeader + offset, BEGIN(nVersion) + 36, 7);
    offset += 7;
    // Copy nonce.
    assert(offset == 39);
    memcpy(cnHeader + offset, BEGIN(nVersion) + 76, 4);
    offset += 4;
    // Copy the rest of hashMerkleRoot (25 bytes)
    memcpy(cnHeader + offset, BEGIN(nVersion) + 36 + 7, 25);
    offset += 25;
    assert(offset == 68);
    // Copy the rest of the header (timestamp and bits).
    memcpy(cnHeader + offset, BEGIN(nVersion) + 68, 8);
    // Compute the CN hash.
    cn_slow_hash(cnHeader, sizeof(cnHeader), BEGIN(thash), 2, 0);
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
