#include "shader_compiler.h"
#include <foundation/memory/memory.h>
#include <3rd_party/AnyOption/anyoption.h>

int main( int argc, char** argv )
{
	BXIAllocator* allocator = nullptr;
	BXMemoryStartUp( &allocator );

#if TEST == 1
    const char input_file[] = "x:/dev/BitBox_v2/code/shader/debug.hlsl";
	const char output_dir[] = "x:/dev/assets/shader/";
#else
    AnyOption opt;
    opt.addUsage( "" );
    opt.addUsage( "Usage:" );
    opt.addUsage( "" );
    opt.addUsage( "--input-file     input file (absolute path)" );
    opt.addUsage( "--output-dir     output directory (absolute path)" );

    opt.setOption( "input-file" );
    opt.setOption( "output-dir" );

    opt.processCommandArgs( argc, argv );
    if( !opt.hasOptions() )
    {
        opt.printUsage();
        return -1;
    }
    
    const char* input_file = opt.getValue( "input-file" );
    const char* output_dir = opt.getValue( "output-dir" );

    if( !input_file || !output_dir )
    {
        opt.printUsage();
        return -2;
    }

#endif

	bx::tool::ShaderCompilerCompile( input_file, output_dir, allocator );
	
	BXMemoryShutDown( &allocator );
	return 0;
}
