#pragma once

#include "../foundation/type.h"
#include "../util/guid.h"
#include "../3rd_party/pugixml/pugixml.hpp"

using NODEXml = pugi::xml_node;
using NODEXmlDoc = pugi::xml_document;

bool WriteGUID( NODEXml node, const char* attr_name, const guid_t& guid );
bool WriteInt32 ( NODEXml node, const char* attr_name, i32 value );
bool WriteUint32( NODEXml node, const char* attr_name, u32 value );
bool WriteUint64( NODEXml node, const char* attr_name, u64 value );
bool WriteFloat ( NODEXml node, const char* attr_name, f32 value );
bool WriteFloat3( NODEXml node, const char* attr_name, const f32 value[3] );
bool WriteFloat4( NODEXml node, const char* attr_name, const f32 value[4] );
bool WriteString( NODEXml node, const char* attr_name, const char* value );

bool ReadGUID  ( guid_t* guid, const NODEXml node, const char* attr_name );
bool ReadInt32 ( i32* value  , const NODEXml node, const char* attr_name );
bool ReadUint32( u32* value  , const NODEXml node, const char* attr_name );
bool ReadUint64( u64* value  , const NODEXml node, const char* attr_name );
bool ReadFloat ( f32* value  , const NODEXml node, const char* attr_name );
bool ReadFloat3( f32 value[3], const NODEXml node, const char* attr_name );
bool ReadFloat4( f32 value[4], const NODEXml node, const char* attr_name );
bool ReadString( char** value, const NODEXml node, const char* attr_name );

struct NODE;
struct NODEContainerImpl;
struct BXIAllocator;
struct BXIFilesystem;

struct NODETextSerializer
{
    NODEXmlDoc xml_doc;
    NODEXml xml_root;
    u32 is_reader : 1;
};
bool InitWriter( NODETextSerializer* serializer );

bool WtiteToFile( const char* filename, const NODETextSerializer& serializer, BXIFilesystem* fsys );
bool ReadFromFile( NODETextSerializer* serializer, const char* filename, BXIFilesystem* fsys, BXIAllocator* allocator );

void WriteNodeTree( NODEXml parent, const NODE* root );
NODE* ReadNodeTree( NODEContainerImpl* node_sys, const NODEXml xml_root );
