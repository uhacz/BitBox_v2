#pragma once

#include "../foundation/type.h"
#include "../foundation/containers.h"
#include "../util/guid.h"

#include "../rtti/rtti.h"

struct BXIAllocator;

struct NODEFlags
{
    union Offline
    {
        u16 all;
        struct  
        {
            u16 read_only : 1;
            u16 tick : 1;
        };
    } offline;

    struct Online
    {
        u16 all;
        struct  
        {
            u32 initializing : 1;
            u32 uninitializing : 1;
            u32 initialized : 1;
            u32 attaching : 1;
            u32 detaching : 1;
            u32 attached : 1;
        };
    }online;
};

struct NODESystem;
struct GFX;
struct NODESystemContext
{
    GFX* gfx;
};

struct NODEInitContext : NODESystemContext
{

};
struct NODEAttachContext : NODESystemContext
{
};

struct NODETickContext : NODESystemContext
{
    f32 dt;
};

void InitContext( NODEAttachContext* out, const NODESystemContext* sys_ctx );
void InitContext( NODEInitContext* out, const NODESystemContext* sys_ctx );
void InitContext( NODETickContext* out, const NODESystemContext* sys_ctx );

struct NODETextSerializer;
struct NODE;
struct NODEComp;

using NODEGuid = guid_t;
using NODESpan = array_span_t<NODE*>;
using NODECompSpan = array_span_t<NODEComp*>;

struct NODEComp
{
    RTTI_DECLARE_TYPE( NODEComp );

    virtual ~NODEComp() {}
    virtual void OnInitialize  ( NODE* node, NODEInitContext* ctx, NODEFlags* flags ) {}
    virtual void OnUninitialize( NODE* node, NODEInitContext* ctx ) {}
    virtual bool OnAttach      ( NODE* node, NODEAttachContext* ctx ) { return true; }
    virtual void OnDetach      ( NODE* node, NODEAttachContext* ctx ) {}
    virtual void OnTick        ( NODE* node, NODETickContext* ctx ) {}
    virtual void OnSerialize   ( NODE* node, NODETextSerializer* serializer ) {}
};

struct NODESystemImpl;
struct NODESystem
{
    NODE* CreateNode( const char* node_name, NODE* parent = nullptr );
    void DestroyNode( NODE** node );

    NODEComp* CreateComponent( const char* type_name );
    void DestroyComponent( NODEComp** comp );

    void SetName( NODE* node, const char* name );
    void LinkNode( NODE* parent, NODE* node );
    void UnlinkNode( NODE* child );

    void LinkComponent( NODE* parent, NODEComp* comp );
    void UnlinkComponent( NODE* parent, NODEComp* comp );

    NODE* FindParent( NODEComp* comp );
    NODE* FindNode( guid_t guid );
    NODE* GetNode( u32 runtime_index );

    void Tick( NODESystemContext* ctx, f32 dt );

    static void StartUp( NODESystem** system, BXIAllocator* allocator );
    static void ShutDown( NODESystem** system, NODEInitContext* ctx );

    NODESystemImpl* impl = nullptr;
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

    friend struct NODESystem;
    friend struct NODESystemImpl;
};

