#pragma once

#include <cstdint>
#include <vector>

// Shared map-cell base used by the released r1189 client.
// Flag values are byte-compatible with the shipped .map files.
class MapCellT
{
public:
    enum Flags : std::uint8_t
    {
        None         = 0x00,
        CollideBlock = 0x08,
        Unwalkable   = 0x10,
    };

    explicit MapCellT(const int numLayers = 0)
        : m_numLayers(numLayers), m_layerScale(static_cast<std::size_t>(numLayers), 1.0f)
    {
    }

    virtual ~MapCellT() = default;

    int getFlags() const { return m_flags; }
    bool hasFlag(const Flags flag) const { return (m_flags & static_cast<int>(flag)) != 0; }
    void addFlag(const Flags flag) { m_flags |= static_cast<int>(flag); }
    void removeFlag(const Flags flag) { m_flags &= ~static_cast<int>(flag); }
    void setFlags(const int flags) { m_flags = flags & 0xff; }

    int getNumLayers() const { return m_numLayers; }

    float getLayerScale(const int layer) const
    {
        if (layer < 0 || layer >= static_cast<int>(m_layerScale.size()))
            return 1.0f;
        return m_layerScale[static_cast<std::size_t>(layer)];
    }

    void setLayerScale(const int layer, const float scale)
    {
        if (layer < 0)
            return;
        if (layer >= static_cast<int>(m_layerScale.size()))
            m_layerScale.resize(static_cast<std::size_t>(layer + 1), 1.0f);
        m_layerScale[static_cast<std::size_t>(layer)] = scale;
    }

private:
    int m_flags{0};
    int m_numLayers{0};
    std::vector<float> m_layerScale;
};
