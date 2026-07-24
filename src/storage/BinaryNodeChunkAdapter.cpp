#include "storage/BinaryNodeChunkAdapter.hpp"

#include <limits>
#include <stdexcept>

namespace pi::storage
{

Chunk BinaryNodeChunkAdapter::toChunk(
    const binary::BinaryNode& node,
    const checkpoint::ComputationIdentity& computation,
    std::uint32_t treeLevel,
    CompressionAlgorithm compression
)
{
    if (node.start() >= node.end())
    {
        throw std::invalid_argument(
            "Cannot convert an empty or inverted BinaryNode to a chunk");
    }
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
    {
        if (node.end() > std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error(
                "BinaryNode range does not fit the durable chunk identity");
        }
    }

    const auto location = checkpoint::BlockLocation::create(
        static_cast<std::uint64_t>(node.start()),
        static_cast<std::uint64_t>(node.end()),
        treeLevel,
        computation);
    const auto identity = ChunkIdentity::create(computation, location);

    return Chunk{
        ChunkCodec::createMetadata(
            identity, node.P(), node.Q(), node.T(), compression),
        node.P(),
        node.Q(),
        node.T()};
}

binary::BinaryNode BinaryNodeChunkAdapter::toBinaryNode(const Chunk& chunk)
{
    chunk.metadata.validate();
    const auto& location = chunk.metadata.identity.location;
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
    {
        if (location.end > std::numeric_limits<std::size_t>::max())
        {
            throw std::overflow_error(
                "Chunk range does not fit the BinaryNode platform range");
        }
    }

    binary::BinaryNode node(
        static_cast<std::size_t>(location.start),
        static_cast<std::size_t>(location.end));
    node.P().set(chunk.p);
    node.Q().set(chunk.q);
    node.T().set(chunk.t);
    return node;
}

} // namespace pi::storage
