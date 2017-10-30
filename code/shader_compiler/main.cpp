#include "shader_compiler.h"
#include <foundation/memory/memory.h>

int main( int argc, const char** argv )
{
	BXIAllocator* allocator = nullptr;
	BXMemoryStartUp( &allocator );
	
	const char input_file[] = "x:/dev/BitBox_v2/code/shader/debug.hlsl";
	const char output_dir[] = "x:/dev/assets/shader/";

	bx::tool::ShaderCompilerCompile( input_file, output_dir, allocator );
	
	BXMemoryShutDown( &allocator );
	return 0;
}
