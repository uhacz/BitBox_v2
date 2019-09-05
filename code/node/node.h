#pragma once

#include "node_common.h"

void InitContext( NODEAttachContext* out, const NODESystemContext* sys_ctx );
void InitContext( NODEInitContext* out, const NODESystemContext* sys_ctx );
void InitContext( NODETickContext* out, const NODESystemContext* sys_ctx );

struct NODEContainerImpl;
struct NODEContainer
{
    NODE* CreateNode( const char* node_name, NODE* parent = nullptr );
    NODE* CreateNode( const guid_t& guid, const char* node_name, NODE* parent = nullptr );
    void DestroyNode( NODE** node );

    void SetName( NODE* node, const char* name );
    void LinkNode( NODE* parent, NODE* node );
    void UnlinkNode( NODE* child );

    NODEComp* CreateComponent( NODE* parent, const char* type_name );
    template< typename T >
    T* CreateComponent( NODE* parent ) {  return CreateComponent( parent, T::TypeNameStatic() ); }
    
    void DestroyComponent( NODEComp** comp );

    NODE* FindParent( NODEComp* comp );
    NODE* FindNode( const NODEGuid& guid );
    NODE* GetNode( u32 runtime_index );
    NODE* GetRoot();

    void Tick( NODESystemContext* ctx, f32 dt );
    
    void Serialize( const char* filename );
    void Unserialize( const char* filename );

    static void StartUp( NODEContainer** system, BXIAllocator* allocator );
    static void ShutDown( NODEContainer** system, NODEInitContext* ctx );

    NODEContainerImpl* impl = nullptr;
};

struct NODE
{
    NODE();
    ~NODE();

    const guid_t&  Guid () const { return _guid; }
    u32            Index() const { return _index; }

    const char* Name  () const { return _name; }
          NODE* Parent()       { return _parent; }
    const NODE* Parent() const { return _parent; }

    NODESpan      Children  () const;
    NODECompSpan  Components() const;

private:
    using NODEArray = c_array_t<NODE*>;
    using COMPArray = c_array_t<NODEComp*>;

    NODEGuid   _guid;
    u32        _index;
    NODEFlags  _flags;
    char*      _name;
    NODE*      _parent;
    NODEArray* _children;
    COMPArray* _components;
    
    friend struct NODEContainer;
    friend struct NODEContainerImpl;
};

struct NODEComp
{
    virtual ~NODEComp() {}
    virtual void OnInitialize  ( NODE* node, NODEInitContext* ctx, NODEFlags* flags ) {}
    virtual void OnUninitialize( NODE* node, NODEInitContext* ctx ) {}
    virtual bool OnAttach      ( NODE* node, NODEAttachContext* ctx ) { return true; }
    virtual void OnDetach      ( NODE* node, NODEAttachContext* ctx ) {}
    virtual void OnTick        ( NODE* node, NODETickContext* ctx ) {}
    virtual void OnSerialize   ( NODE* node, NODETextSerializer* serializer ) {}

    virtual u64 TypeHashCode() const = 0;
    virtual const char* TypeName() const = 0;

    static constexpr u32 INVALID_SCENE_INDEX = UINT32_MAX;
    u32 _scene_index = INVALID_SCENE_INDEX;
};