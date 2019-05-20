/*@H
* File: ray.h
* Author: Jesse Calvert
* Created: October 22, 2017, 16:20
* Last modified: November 2, 2017, 18:13
*/

#pragma once

#include <stdint.h>
#include <limits.h>
#include <float.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8 s8;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef uintptr_t uintptr;
typedef intptr_t intptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

typedef real32 r32;
typedef real64 r64;

typedef r32 f32;
typedef r64 f64;

typedef uintptr_t umm;

#define Real32Maximum FLT_MAX
#define Real32Minimum -FLT_MAX

#ifndef internal
    #define internal static
#endif
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

#if RAY_DEBUG
#define Assert(Expression) if(!(Expression)) *(int *)0 = 0
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

// TODO: should these all use 64-bit?
#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

#define U32FromPointer(Pointer) ((u32)(memory_index)(Pointer))
#define PointerFromU32(type, Value) (type *)((memory_index)Value)

#define OffsetOf(type, Member) (umm)&(((type *)0)->Member)

#define Swap(A, B, type) type TEMP_DONT_REDEFINE_THIS_##type = A; A = B; B = TEMP_DONT_REDEFINE_THIS_##type

#include "ray_intrinsics.h"
#include "ray_math.h"

#define LittleEndianTag(A, B, C, D) (((u32) A << 24) | (((u32) B) << 16) | (((u32) C) << 8) | (((u32) D) << 0))

#pragma pack(push, 1)
struct bitmap_file_header
{
	u16 MagicValue;
	u32 Size;
	u16 Reserved0;
	u16 Reserved1;
	u32 ImageDataOffset;
};

struct bitmap_information_header
{
	u32 Size;
	u32 Width;
	u32 Height;
	u16 ColorPlanes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 ImageSize;
	u32 HorizontalResolution;
	u32 VerticalResolution;
	u32 ColorsUsed;
	u32 ColorsImportant;

	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
	u32 AlphaMask;

	u32 ColorSpaceTag;
	u32 Ignored[9];
	u32 RedGamma;
	u32 GreenGamma;
	u32 BlueGamma;
};
#pragma pack(pop)

struct image
{
	u32 Width;
	u32 Height;
	u32 PixelCount;
	u32 PixelsSize;
	u32 *Pixels;
};

struct material
{
	v3 ReflectionColor;
	v3 EmitColor;
	f32 Specularity;
	b32 Transparent;
	f32 RefractionIndex;
};

enum object_type
{
	Object_None,
	Object_Plane,
	Object_Sphere,
};
struct object
{
	object_type Type;
	union
	{
		sphere Sphere;
		plane Plane;
	};

	material Material;
};

struct tile_work_order
{
	image *Image;
	struct world *World;

	u32 MinX;
	u32 MinY;
	u32 OnePastMaxX;
	u32 OnePastMaxY;

	u32 RaysPerPixel;
	u32 MaxBounces;
};

struct work_queue
{
	u32 TileQueueSize;
	tile_work_order TileWorkQueue[256];

	volatile u32 NextWorkIndex;
	volatile u32 TilesCompleted;
	volatile u32 RaysCast;
};

struct world
{
	v3 CameraP;
	v3 CameraX;
	v3 CameraY;
	v3 CameraZ;
	v3 FilmP;
	f32 HalfFilmW;
	f32 HalfFilmH;

	material NullMaterial;
	v3 LightDirection;
	v3 LightColor;

	u32 ObjectCount;
	object Objects[64];
};
