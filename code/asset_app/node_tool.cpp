#include "node_tool.h"
#include <3rd_party/imgui/imgui.h>
#include "../node/node.h"
#include "common/base_engine_init.h"
//
//
//
void NODETool::StartUp( CMNEngine* e, const char* src_root, BXIAllocator* allocator )
{
    _allocator = allocator;
    _root_src_folder_ctx.SetUp( e->filesystem, allocator );
    _root_src_folder_ctx.SetFolder( src_root );
}

void NODETool::ShutDown( CMNEngine* e )
{

}

void NODETool::Tick( CMNEngine* e, const TOOLContext& ctx, float dt )
{
    NODEContainer* node_container = e->nodes;
    if( ImGui::Begin( "NODE", nullptr, ImGuiWindowFlags_MenuBar ) )
    {
        if( ImGui::BeginMenuBar() )
        {
            if( ImGui::BeginMenu( "File" ) )
            {
                if( ImGui::BeginMenu( "Load" ) )
                {
                    const string_buffer_t& file_list = _root_src_folder_ctx.FileList();
                    string_buffer_it selected_it = common::MenuFileSelector( file_list );
                    if( !selected_it.null() )
                    {
                        node_container->Unserialize( selected_it.pointer );
                        _current_src_file.Clear();
                        _current_src_file.AppendRelativePath( selected_it.pointer );
                    }
                    ImGui::EndMenu();
                }
                if( ImGui::BeginMenu( "Save" ) )
                {
                    bool doSave = ImGui::InputText( "Filename", _current_src_file._data, _current_src_file.MAX_SIZE );
                    ImGui::SameLine();
                    doSave |= ImGui::Button( "Save" );
                    if( doSave )
                    {
                        if( _current_src_file.RelativePathLength() )
                        {
                            node_container->Serialize( _current_src_file.RelativePath() );
                            _root_src_folder_ctx.RequestRefresh();
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        _DrawTree( node_container );
    }

    ImGui::End();
}

void NODETool::_DrawTree( NODEContainer* node_container )
{
    NODE* root = node_container->GetRoot();
    if( !root )
        return;

    constexpr u32 MAX_DEPTH = 255;

    struct Item
    {
        NODE* node;
        u32 state;
    };
    Item stack[MAX_DEPTH];
    u32 stack_size = 0;

    stack[stack_size++] = { root, 0 };
    while( stack_size )
    {
        Item item = stack[--stack_size];
        if( item.state == 1 )
        {
            ImGui::TreePop();
            continue;
        }

        NODE* node = item.node;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if( _current_node == node )
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        auto children = node->Children();
        if( children.size() == 0 )
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }


        if( ImGui::TreeNodeEx( node, flags, "%s", node->Name() ) )
        {
            stack[stack_size++] = { node, 1 };

            if( ImGui::IsItemClicked() )
            {
                _current_node = node;
            }

            if( ImGui::IsItemClicked( 1 ) )
            {
                _current_node = node;
                _flags.node_menu = 1;
                ImGui::OpenPopupOnItemClick( node->Name(), 1 );
            }

            if( ImGui::BeginDragDropSource() )
            {
                ImGui::SetDragDropPayload( "node", &node, sizeof( void* ) );
                ImGui::EndDragDropSource();
            }
            if( ImGui::BeginDragDropTarget() )
            {
                if( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "node" ) )
                {
                    NODE* payload_node = (*(NODE**)payload->Data);
                    if( payload_node != node )
                    {
                        node_container->LinkNode( node, payload_node );
                    }
                }
                ImGui::EndDragDropTarget();
            }


            for( i32 ichild = children.size() - 1; ichild >= 0; --ichild )
            {
                stack[stack_size++] = { children[ichild], 0 };
            }
        }
    }

    if( _flags.node_menu )
    {
        if( ImGui::BeginPopupContextWindow( _current_node->Name(), 1 ) )
        {
            char node_name[255];
            node_name[0] = 0;

            if( ImGui::InputText( "Add: ", node_name, 255, ImGuiInputTextFlags_EnterReturnsTrue ) )
            {
                if( string::length( node_name ) )
                {
                    node_container->CreateNode( node_name, _current_node );
                    _flags.node_menu = 0;
                }
            }
            if( _current_node != node_container->GetRoot() )
            {
                if( ImGui::Button( "Remove" ) )
                {
                    node_container->DestroyNode( &_current_node );
                    _flags.node_menu = 0;
                }
            }
            ImGui::EndPopup();
        }
    }
}
