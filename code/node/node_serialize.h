#pragma once

#include "../foundation/type.h"
#include "../util/guid.h"

namespace pugi
{
    class xml_node;
}//
using NODEXml = pugi::xml_node;

struct NODETextSerializer
{
    NODEXml* xml;
    u32 is_reader : 1;
};

struct BXIFilesystem;
bool WtiteToFile( const char* filename, const NODETextSerializer& serializer, BXIFilesystem* fsys );
bool ReadFromFile( NODETextSerializer* serializer, const char* filename, BXIFilesystem* fsys );

class NODE;
NODEXml* WriteNode( NODEXml* parent, const NODE* node );
bool WriteGUID( pugi::xml_node* node, const char* attr_name, const guid_t& guid );

bool WriteInt32 ( NODEXml* node, const char* attr_name, i32 value );
bool WriteUint32( NODEXml* node, const char* attr_name, u32 value );
bool WriteUint64( NODEXml* node, const char* attr_name, u64 value );
bool WriteFloat ( NODEXml* node, const char* attr_name, f32 value );
bool WriteFloat3( NODEXml* node, const char* attr_name, const f32 value[3] );
bool WriteFloat4( NODEXml* node, const char* attr_name, const f32 value[4] );
bool WriteString( NODEXml* node, const char* attr_name, const char* value );


NODE* ReadNode( NODEXml* node );
bool ReadGUID( guid_t* guid, const NODEXml* node, const char* attr_name );
bool ReadInt32 ( i32* value  , const NODEXml* node, const char* attr_name );
bool ReadUint32( u32* value  , const NODEXml* node, const char* attr_name );
bool ReadUint64( u64* value  , const NODEXml* node, const char* attr_name );
bool ReadFloat ( f32* value  , const NODEXml* node, const char* attr_name );
bool ReadFloat3( f32 value[3], const NODEXml* node, const char* attr_name );
bool ReadFloat4( f32 value[4], const NODEXml* node, const char* attr_name );
bool ReadString( char** value, const NODEXml* node, const char* attr_name );
