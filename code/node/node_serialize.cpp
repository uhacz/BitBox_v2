#include "node_serialize.h"
#include "../3rd_party/pugixml/pugixml.hpp"
#include "node.h"
#include "filesystem/filesystem_plugin.h"
#include "foundation/string_util.h"
#include "node_system_impl.h"

bool InitWriter( NODETextSerializer* serializer )
{
    serializer->xml_doc.reset();
    serializer->xml_root = serializer->xml_doc.append_child( "NODE" );
    serializer->is_reader = 0;
    return !serializer->xml_doc.empty();
}


struct NODEXmlDataWriter : pugi::xml_writer
{
    virtual void write( const void* data, size_t size ) override
    {
        WriteFileSync( _fs, _filename, data, (u32)size );
    }

    BXIFilesystem* _fs = nullptr;
    const char* _filename = nullptr;
};
bool WtiteToFile( const char* filename, const NODETextSerializer& serializer, BXIFilesystem* fsys )
{
    NODEXmlDataWriter xml_writer;
    xml_writer._fs = fsys;
    xml_writer._filename = filename;

    serializer.xml_doc.save( xml_writer );
    return true;
}

bool ReadFromFile( NODETextSerializer* serializer, const char* filename, BXIFilesystem* fsys, BXIAllocator* allocator )
{
    BXFileWaitResult file = LoadFileSync( fsys, filename, BXEFIleMode::TXT, allocator );
    bool result = false;
    if( file.status == BXEFileStatus::READY )
    {
        serializer->xml_doc.reset();
        pugi::xml_parse_result parse_result = serializer->xml_doc.load_buffer( file.file.bin, file.file.size );
        if( parse_result )
        {
            serializer->xml_root = serializer->xml_doc.root();
            serializer->is_reader = 1;
            result = true;
        }
        else
        {
            SYS_LOG_ERROR( "XML file (%s) parse error: %s", filename, parse_result.description() );
        }
    }

    fsys->CloseFile( &file.handle );

    return result;
}


static NODEXml WriteNode( NODEXml parent, const NODE* node )
{
    NODEXml node_xml = parent.append_child( "node" );
    if( node_xml.empty() )
        return NODEXml();

    guid_string_t guid_str = {};

    {
        auto attr = node_xml.append_attribute( "guid" );
        ToString( &guid_str, node->Guid() );
        attr.set_value( guid_str.data );
    }

    {
        auto attr = node_xml.append_attribute( "name" );
        attr.set_value( node->Name() );
    }

    if( node->Parent() )
    {
        auto attr = node_xml.append_attribute( "parent" );
        ToString( &guid_str, node->Parent()->Guid() );
        attr.set_value( guid_str.data );
    }

    {
        NODECompSpan components = node->Components();
        if( components.size() )
        {
            auto comp_node_xml = node_xml.append_child( "components" );
            // store guids of components
        }
    }

    return node_xml;
}

static void WriteNodeR( NODEXml parent_xml, const NODE* node )
{
    WriteNode( parent_xml, node );

    for( const NODE* child : node->Children() )
    {
        WriteNodeR( parent_xml, child );
    }
}

void WriteNodeTree( NODEXml parent, const NODE* root )
{
    WriteNodeR( parent, root );
    //for( const NODE* child : root->Children() )
    //{
    //    WriteNodeR( parent, child );
    //}
}


static NODE* ReadNode( NODEContainerImpl* node_sys, const NODEXml xml_node )
{
    const char* node_name = xml_node.attribute( "name" ).as_string();
    const char* node_guid_str = xml_node.attribute( "guid" ).as_string();

    guid_t node_guid;
    FromString( &node_guid, node_guid_str );
    NODE* node = node_sys->CreateNode( node_guid, node_name );

    auto parent_attr = xml_node.attribute( "parent" );
    if( !parent_attr.empty() )
    {
        const char* parent_guid_str = parent_attr.as_string();
        guid_t parent_guid;
        FromString( &parent_guid, parent_guid_str );
        NODE* parent_node = node_sys->FindNode( parent_guid );
        SYS_ASSERT( parent_node != nullptr );
        
        node_sys->LinkNode( parent_node, node );
    }

    return node;
}

NODE* ReadNodeTree( NODEContainerImpl* node_sys, const NODEXml xml_root )
{
    NODEXml xml_nodes;
    for( NODEXml xml_child : xml_root.children() )
    {
        if( string::equal( xml_child.name(), "NODE" ) )
        {
            xml_nodes = xml_child;
            break;
        }
    }

    if( xml_nodes.empty() )
        return nullptr;

    
    NODEXml xml_child = xml_nodes.first_child();
    NODE* result = ReadNode( node_sys, xml_child );

    if( !result )
        return nullptr;

    for( xml_child = xml_child.next_sibling(); !xml_child.empty(); xml_child = xml_child.next_sibling() )
    {
        ReadNode( node_sys, xml_child );
    }

    return result;
}



