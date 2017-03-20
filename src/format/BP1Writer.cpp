/*
 * BP1Writer.cpp
 *
 *  Created on: Feb 1, 2017
 *      Author: wfg
 */


#include "format/BP1Writer.h"



namespace adios
{
namespace format
{


std::size_t BP1Writer::GetProcessGroupIndexSize( const std::string name, const std::string timeStepName,
                                                 const size_t numberOfTransports ) const noexcept
{
    //pgIndex + list of methods (transports)
    return ( name.length() + timeStepName.length() + 23 ) + ( 3 + numberOfTransports ); //should be sufficient for data and metadata pgindices
}


void BP1Writer::WriteProcessGroupIndex( const bool isFortran, const std::string name, const unsigned int processID,
                                        const std::string timeStepName, const unsigned int timeStep,
                                        const std::vector< std::shared_ptr<Transport> >& transports,
                                        capsule::STLVector& buffer, BP1MetadataSet& metadataSet ) const noexcept
{
    metadataSet.DataPGLengthPosition = buffer.m_DataPosition;

    // adapt this part to local variables
    const std::vector<int> methodIDs = GetMethodIDs( transports );

    const std::size_t dataPGLengthPosition = buffer.m_DataPosition;
    const std::size_t metadataPGLengthPosition = metadataSet.PGIndexPosition;

    metadataSet.PGIndexPosition += 2; //skip length of pg in metadata, 2 bytes, would write at the end
    buffer.m_DataPosition += 8; //skip length of pg in data, 8 bytes, would write at the end

    //write name to metadata
    const std::uint16_t lengthOfName = name.length();
    WriteNameRecord( name, lengthOfName, metadataSet.PGIndex, metadataSet.PGIndexPosition );

    //write if host language Fortran in metadata and data
    const char hostFortran = ( isFortran ) ? 'y' : 'n'; //if host language is fortran
    MemcpyToBuffer( metadataSet.PGIndex, metadataSet.PGIndexPosition, &hostFortran, 1 );
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &hostFortran, 1 );

    //name in data
    WriteNameRecord( name, lengthOfName, buffer.m_Data, buffer.m_DataPosition );

    //processID,
    MemcpyToBuffer( metadataSet.PGIndex, metadataSet.PGIndexPosition, &processID, 4 );
    //skip coordination var in data ....what is coordination var?
    buffer.m_DataPosition += 4;

    //time step name to metadata and data
    const std::uint16_t lengthOfTimeStep = timeStepName.length();
    WriteNameRecord( timeStepName, lengthOfTimeStep, metadataSet.PGIndex, metadataSet.PGIndexPosition );
    WriteNameRecord( timeStepName, lengthOfTimeStep, buffer.m_Data, buffer.m_DataPosition );

    //time step to metadata and data
    MemcpyToBuffer( metadataSet.PGIndex, metadataSet.PGIndexPosition, &timeStep, 4 );
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &timeStep, 4 );

    //offset to pg in data in metadata which is the current absolute position
    MemcpyToBuffer( metadataSet.PGIndex, metadataSet.PGIndexPosition, &buffer.m_DataAbsolutePosition, 8 );

    //Back to writing metadata pg index length (length of group)
    const std::uint16_t metadataPGIndexLength = metadataSet.PGIndexPosition - metadataPGLengthPosition - 2; //without length of group record
    std::memcpy( &metadataSet.PGIndex[metadataPGLengthPosition], &metadataPGIndexLength, 2 );


    //here write method in data
    const std::uint8_t methodsSize = methodIDs.size();
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &methodsSize, 1 ); //method count
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &methodsSize, 2 ); //method length, assume one byte for methodID for now

    for( auto& methodID : methodIDs )
    {
        MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &methodID, 1 ); //method ID, unknown for now
        buffer.m_DataPosition += 2; //skip method params length = 0 (2 bytes) for now
    }

    buffer.m_DataAbsolutePosition = buffer.m_DataPosition - dataPGLengthPosition; //update aboslute position

    buffer.m_DataPosition += 12; //add vars count and length
    buffer.m_DataAbsolutePosition += 12; //add vars count and length

    metadataSet.PGCount += 1;
}



void BP1Writer::Advance( BP1MetadataSet& metadataSet, capsule::STLVector& buffer )
{
    FlattenData( metadataSet, buffer );


}



void BP1Writer::Close( BP1MetadataSet& metadataSet, capsule::STLVector& buffer, Transport& transport, bool& isFirstClose,
    		           const bool haveMetadata, const bool haveTiming ) const noexcept
{
    if( isFirstClose == true )
    {
        FlattenData( metadataSet, buffer );
        FlattenMetadata( metadataSet, buffer );
        isFirstClose = false;
    }
    //implementing N-to-N for now, no aggregation

    transport.Write( buffer.m_Data.data(), buffer.m_DataPosition ); //single write


    if( haveMetadata == true )
    {
        //here call aggregator
    }

    transport.Close();
}



//PRIVATE FUNCTIONS
void BP1Writer::WriteNameRecord( const std::string name, const std::uint16_t length,
                                 std::vector<char>& buffer, std::size_t& position ) const noexcept
{
    MemcpyToBuffer( buffer, position, &length, 2 );
    MemcpyToBuffer( buffer, position, name.c_str( ), length );
}


void BP1Writer::WriteDimensionRecord( std::vector<char>& buffer, std::size_t& position,
                                      const std::vector<std::size_t>& localDimensions,
                                      const std::vector<std::size_t>& globalDimensions,
                                      const std::vector<std::size_t>& globalOffsets,
                                      const bool addType ) const noexcept
{
    if( addType == true )
    {
        constexpr char no = 'n'; //dimension format unsigned int value for now
        for( unsigned int d = 0; d < localDimensions.size(); ++d )
        {
            MemcpyToBuffer( buffer, position, &no, 1 );
            MemcpyToBuffer( buffer, position, &localDimensions[d], 8 );
            MemcpyToBuffer( buffer, position, &no, 1 );
            MemcpyToBuffer( buffer, position, &globalDimensions[d], 8 );
            MemcpyToBuffer( buffer, position, &no, 1 );
            MemcpyToBuffer( buffer, position, &globalOffsets[d], 8 );
        }
    }
    else
    {
        for( unsigned int d = 0; d < localDimensions.size(); ++d )
        {
            MemcpyToBuffer( buffer, position, &localDimensions[d], 8 );
            MemcpyToBuffer( buffer, position, &globalDimensions[d], 8 );
            MemcpyToBuffer( buffer, position, &globalOffsets[d], 8 );
        }
    }
}

void BP1Writer::WriteDimensionRecord( std::vector<char>& buffer, std::size_t& position,
                                      const std::vector<std::size_t>& localDimensions,
                                      const unsigned int skip,
                                      const bool addType ) const noexcept
{
    if( addType == true )
    {
        constexpr char no = 'n'; //dimension format unsigned int value (not using memberID for now)
        for( const auto& localDimension : localDimensions )
        {
            MemcpyToBuffer( buffer, position, &no, 1 );
            MemcpyToBuffer( buffer, position, &localDimension, 8 );
            position += skip;
        }
    }
    else
    {
        for( const auto& localDimension : localDimensions )
        {
            MemcpyToBuffer( buffer, position, &localDimension, 8 );
            position += skip;
        }
    }
}


void BP1Writer::FlattenData( BP1MetadataSet& metadataSet, capsule::STLVector& buffer ) const noexcept
{
    //vars count and Length
    std::memcpy( &buffer.m_Data[metadataSet.DataVarsCountPosition], &metadataSet.VarsCount, 4 ); //count
    const std::uint64_t dataVarsLength = buffer.m_DataPosition - metadataSet.DataVarsCountPosition - 8 - 4; //without record itself and vars count
    std::memcpy( &buffer.m_Data[metadataSet.DataVarsCountPosition+4], &dataVarsLength, 8 ); //length

    //attributes (empty for now) count and length by moving positions
    buffer.m_DataPosition += 12;
    buffer.m_DataAbsolutePosition += 12;

    //Finish writing pg group length and, vars count and length in Data
    const std::uint64_t dataPGLength = buffer.m_DataPosition - metadataSet.DataPGLengthPosition - 8; //without record itself, 12 due to empty attributes
    std::memcpy( &buffer.m_Data[metadataSet.DataPGLengthPosition], &dataPGLength, 8 );
}


void BP1Writer::FlattenMetadata( BP1MetadataSet& metadataSet, capsule::STLVector& buffer ) const noexcept
{
	//Finish writing metadata counts and lengths (IndexPosition)
    //pg index
    std::memcpy( &metadataSet.PGIndex[0], &metadataSet.PGCount, 8 ); //count
    const std::uint64_t pgIndexLength = metadataSet.PGIndexPosition - 16; //without record itself
    std::memcpy( &metadataSet.PGIndex[8], &pgIndexLength, 8 );
    //vars index
    std::memcpy( &metadataSet.VarsIndex[0], &metadataSet.VarsCount, 4 ); //count
    const std::uint64_t varsIndexLength = metadataSet.VarsIndexPosition - 12; //without record itself
    std::memcpy( &metadataSet.VarsIndex[4], &varsIndexLength, 8 );
    //attributes index
    std::memcpy( &metadataSet.AttributesIndex[0], &metadataSet.AttributesCount, 4 ); //count
    const std::uint64_t attributesIndexLength = metadataSet.AttributesIndexPosition - 12; //without record itself
    std::memcpy( &metadataSet.AttributesIndex[4], &attributesIndexLength, 8 );

    const std::size_t metadataSize = metadataSet.PGIndexPosition + metadataSet.VarsIndexPosition +
                                     metadataSet.AttributesIndexPosition + metadataSet.MiniFooterSize;

    buffer.m_Data.resize( buffer.m_DataPosition + metadataSize ); //resize data to fit metadata, must replace with growth buffer strategy

    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, metadataSet.PGIndex.data(), metadataSet.PGIndexPosition );
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, metadataSet.VarsIndex.data(), metadataSet.VarsIndexPosition );
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, metadataSet.AttributesIndex.data(), metadataSet.AttributesIndexPosition );

    //getting absolute offsets, minifooter is 28 bytes for now
    const std::uint64_t offsetPGIndex = buffer.m_DataAbsolutePosition;
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &offsetPGIndex, 8 );

    const std::uint64_t offsetVarsIndex = offsetPGIndex + metadataSet.PGIndexPosition;
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &offsetVarsIndex, 8 );

    const std::uint64_t offsetAttributeIndex = offsetVarsIndex + metadataSet.VarsIndexPosition;
    MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &offsetAttributeIndex, 8 );

    //version
    if( IsLittleEndian( ) )
    {
    	const std::uint8_t endian = 0;
    	MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &endian, 1 );
    	buffer.m_DataPosition += 2;
    	MemcpyToBuffer( buffer.m_Data, buffer.m_DataPosition, &m_Version, 1 );
    }
    else
    {

    }

    buffer.m_DataAbsolutePosition += metadataSize;
}





} //end namespace format
} //end namespace adios
