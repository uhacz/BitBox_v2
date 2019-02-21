#pragma once

#include <foundation/string_util.h>
#include <entity/entity_system.h>
#include "util/file_system_name.h"

struct BXIAllocator;
struct BXIFilesystem;

namespace common
{
    struct FolderContext
    {
        void SetUp( BXIFilesystem* fs, BXIAllocator* allocator );

        void RequestRefresh();
        void SetFolder( const char* folder );

        const string_buffer_t& FileList() const;
        const char* Folder() const { return _folder.c_str(); }

    private:
        string_t _folder;

        mutable string_buffer_t _file_list;
        mutable uint8_t _flag_refresh_file_list = 0;

        BXIFilesystem* _filesystem = nullptr;
        BXIAllocator* _allocator = nullptr;
    };

    void RefreshFiles( string_buffer_t* flist, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator );
    string_buffer_it MenuFileSelector( const string_buffer_t& file_list );

    void CreateDstFilename( FSName* dst_file, const char* dst_folder, const string_t& src_file, const char* ext );
    void CreateDstFilename( string_t* dst_file, const char* dst_folder, const string_t& src_file, const char* ext, BXIAllocator* allocator );
}//

namespace common
{
    template< typename T >
    inline ECSComponentProxy<T> CreateComponentIfNotExists( ECS* ecs, ECSEntityID entity )
    {
        ECSEntityProxy eproxy( ecs, entity );
        ECSComponentID comp_id = eproxy.Lookup<T>();

        ECSComponentProxy<T> proxy( ecs, comp_id );
        if( !proxy )
        {
            proxy = ECSComponentProxy<T>::New( ecs );
            proxy.SetOwner( entity );
        }

        return proxy;
    }


}

