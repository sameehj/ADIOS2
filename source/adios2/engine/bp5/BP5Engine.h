/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * BP5Writer.h
 *
 */

#ifndef ADIOS2_ENGINE_BP5_BP5ENGINE_H_
#define ADIOS2_ENGINE_BP5_BP5ENGINE_H_

#include "adios2/common/ADIOSConfig.h"
#include "adios2/core/Engine.h"
#include "adios2/helper/adiosComm.h"
#include "adios2/toolkit/burstbuffer/FileDrainerSingleThread.h"
#include "adios2/toolkit/format/bp5/BP5Serializer.h"
#include "adios2/toolkit/transportman/TransportMan.h"

namespace adios2
{
namespace core
{
namespace engine
{

class BP5Engine
{
public:
    int m_RankMPI = 0;
    /* metadata index table*/
    std::unordered_map<uint64_t, std::vector<uint64_t>> m_MetadataIndexTable;

    struct Minifooter
    {
        std::string VersionTag;
        uint64_t PGIndexStart = 0;
        uint64_t VarsIndexStart = 0;
        uint64_t AttributesIndexStart = 0;
        int8_t Version = -1;
        bool IsLittleEndian = true;
        bool HasSubFiles = false;
    };

    format::BufferSTL m_MetadataIndex;

    /** Positions of flags in Index Table Header that Reader uses */
    static constexpr size_t m_IndexHeaderSize = 64;
    static constexpr size_t m_EndianFlagPosition = 36;
    static constexpr size_t m_BPVersionPosition = 37;
    static constexpr size_t m_ActiveFlagPosition = 38;
    static constexpr size_t m_BPMinorVersionPosition = 39;
    static constexpr size_t m_WriterCountPosition = 40;
    static constexpr size_t m_AggregatorCountPosition = 44;
    static constexpr size_t m_ColumnMajorFlagPosition = 48;
    static constexpr size_t m_VersionTagPosition = 0;
    static constexpr size_t m_VersionTagLength = 32;

    std::vector<std::string>
    GetBPSubStreamNames(const std::vector<std::string> &names) const noexcept;

    std::vector<std::string>
    GetBPMetadataFileNames(const std::vector<std::string> &names) const
        noexcept;
    std::vector<std::string>
    GetBPMetaMetadataFileNames(const std::vector<std::string> &names) const
        noexcept;
    std::string GetBPMetadataFileName(const std::string &name) const noexcept;
    std::string GetBPMetaMetadataFileName(const std::string &name) const
        noexcept;
    std::vector<std::string>
    GetBPMetadataIndexFileNames(const std::vector<std::string> &names) const
        noexcept;

    std::string GetBPMetadataIndexFileName(const std::string &name) const
        noexcept;

    std::string GetBPSubStreamName(const std::string &name, const size_t id,
                                   const bool hasSubFiles = true,
                                   const bool isReader = false) const noexcept;

#define BP5_FOREACH_PARAMETER_TYPE_4ARGS(MACRO)                                \
    MACRO(OpenTimeoutSecs, Int, int, 3600)                                     \
    MACRO(BeginStepPollingFrequencySecs, Int, int, 0)                          \
    MACRO(StreamReader, Bool, bool, false)                                     \
    MACRO(BurstBufferDrain, Bool, bool, true)                                  \
    MACRO(NodeLocal, Bool, bool, false)                                        \
    MACRO(BurstBufferPath, String, std::string, "\"\"")                        \
    MACRO(verbose, Int, int, 0)                                                \
    MACRO(CollectiveMetadata, Bool, bool, true)                                \
    MACRO(ReaderShortCircuitReads, Bool, bool, false)

    struct BP5Params
    {
#define declare_struct(Param, Type, Typedecl, Default) Typedecl Param;
        BP5_FOREACH_PARAMETER_TYPE_4ARGS(declare_struct)
#undef declare_struct
    };

    void ParseParams(IO &io, BP5Params &Params);
    BP5Params m_Parameters;

private:
};

} // namespace engine
} // namespace core
} // namespace adios2
#endif

/*
 *   Data Formats:
 *   MetadataIndex file (md.idx)
 *	BP5 header for "Index Table" (64 bytes)
 *      for each Writer, what aggregator writes its data
 *             uint16_t * WriterCount;
 *	for each timestep:
 *		uint64_t 0 :  CombinedMetaDataPos
 *		uint64_t 1 :  CombinedMetaDataSize
 *		for each Writer
 *		 uint64_t  DataPos (in the file above)
 *
 *   MetaMetadata file (mmd.0) contains FFS format information
 *	for each meta metadata item:
 *		uint64_t  MetaMetaIDLen
 *		uint64_t  MetaMetaInfoLen
 *		char[MeatMetaIDLen] MetaMetaID
 *		char[MetaMetaInfoLen] MetaMetanfo
 *  Notes:   This file should be quite small, with size dependent upon the
 *number of different "formats" written by any rank.
 *
 *
 *   MetaData file (md.0) contains encoded metadata for each timestep, for each
 *rank BP5 header for "Metadata" (64 bytes) for each timestep: uint64_t
 *TotalSize of this metadata block (including this length) uint64_t[WriterCount]
 *Length of each writer rank's metadata for each rank FFS-encoded metadata block
 *of length corresponding to entry above
 *
 *
 *   Data file (data.x) contains a block of data for each timestep, for each
 *rank
 */
