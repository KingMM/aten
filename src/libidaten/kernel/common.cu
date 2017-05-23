#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "kernel/light.cuh"
#include "kernel/material.cuh"
#include "kernel/intersect.cuh"

__global__ void onAddFuncs()
{
	addLighFuncs();
	addMaterialFuncs();
	addIntersectFuncs();
}

void addFuncs()
{
	onAddFuncs << <1, 1 >> > ();
}