#ifndef TRANSFORM_INSTANCE_DATA
#define TRANSFORM_INSTANCE_DATA

#define TRANSFORM_INSTANCE_DATA_SLOT 0

#define TRANSFORM_INSTANCE_WORLD_SLOT 0
#define TRANSFORM_INSTANCE_WORLD_IT_SLOT 1

struct TransformInstanceData
{
	uint offset;
	uint _padding[15];
};

#endif