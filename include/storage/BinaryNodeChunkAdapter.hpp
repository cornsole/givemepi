#pragma once

#include "binary/BinaryNode.hpp"
#include "storage/ChunkCodec.hpp"

#include <cstdint>

namespace pi::storage
{

/// Converts the algorithm-owned BinaryNode value at the storage boundary.
///
/// The adapter owns no storage and performs no filesystem I/O. Identity and
/// tree-level metadata are supplied by the merge coordinator so the adapter
/// cannot infer or silently change computation provenance.
class BinaryNodeChunkAdapter
{
public:
    [[nodiscard]]
    static Chunk toChunk(
        const binary::BinaryNode& node,
        const checkpoint::ComputationIdentity& computation,
        std::uint32_t treeLevel,
        CompressionAlgorithm compression = CompressionAlgorithm::none
    );

    [[nodiscard]]
    static binary::BinaryNode toBinaryNode(const Chunk& chunk);
};

} // namespace pi::storage
