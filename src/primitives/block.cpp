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
    // Convert it to Cryptonight header format, i.e. nonce at 39th byte.
    uint8_t cnHeader[80] = {};
    // Copy nVersion, prevblockhas, and 3 bytes of merkleroot.
    memcpy(cnHeader, BEGIN(nVersion), 39);
    uint32_t offset = 39;
    // Copy nonce.
    memcpy(cnHeader + offset, BEGIN(nVersion) + 76, 4);
    offset += 4;
    // Copy remaining 29 bytes of hashMerkleRoot
    memcpy(cnHeader + offset, BEGIN(nVersion) + 39, 29);
    offset += 29;
    assert(offset == 72);
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
