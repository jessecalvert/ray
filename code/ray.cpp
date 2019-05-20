/*@H
* File: ray.cpp
* Author: Jesse Calvert
* Created: October 22, 2017, 16:18
* Last modified: November 2, 2017, 18:15
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ray.h"

#include "win32_ray.cpp"

internal image
AllocateImage(u32 Width, u32 Height)
{
	image Result = {};
	Result.Width = Width;
	Result.Height = Height;
	Result.PixelCount = Result.Width * Result.Height;
	Result.PixelsSize = sizeof(u32) * Result.PixelCount;
	Result.Pixels = (u32 *)calloc(Result.PixelsSize, 1);
	return Result;
}

internal void
WriteImage(image *Image, char *Filename)
{
	bitmap_file_header FileHeader = {};
	FileHeader.MagicValue = 0x4D42;
	FileHeader.Size = sizeof(bitmap_file_header) + sizeof(bitmap_information_header) + Image->PixelsSize;
	FileHeader.ImageDataOffset = sizeof(bitmap_file_header) + sizeof(bitmap_information_header);

	bitmap_information_header InfoHeader = {};
	InfoHeader.Size = sizeof(bitmap_information_header);
	InfoHeader.Width = Image->Width;
	InfoHeader.Height = Image->Height;
	InfoHeader.ColorPlanes = 1;
	InfoHeader.BitsPerPixel = 32;
	InfoHeader.Compression = 3;
	InfoHeader.ImageSize = Image->PixelsSize;
	InfoHeader.RedMask = 0x00FF0000;
	InfoHeader.GreenMask = 0x0000FF00;
	InfoHeader.BlueMask = 0x000000FF;
	InfoHeader.AlphaMask = 0xFF000000;
	InfoHeader.ColorSpaceTag = LittleEndianTag('s', 'R', 'G', 'B');

	FILE *OutFile = fopen(Filename, "wb");
	if(OutFile)
	{
		fwrite(&FileHeader, sizeof(bitmap_file_header), 1, OutFile);
		fwrite(&InfoHeader, sizeof(bitmap_information_header), 1, OutFile);
		fwrite(Image->Pixels, Image->PixelsSize, 1, OutFile);
		fclose(OutFile);
	}
}

inline f32
LinearToSRGB(f32 L)
{
	L = Clamp01(L);
	f32 Result = L < 0.0031308f ? L*12.92f : 1.055f*Pow(L, 1.0f / 2.4f) - 0.055f;
	return Result;
}

inline u32
PackLinear01ToSRGBU32(v3 Color)
{
	u32 R = RoundReal32ToUInt32(255.0f*LinearToSRGB(Color.r));
	u32 G = RoundReal32ToUInt32(255.0f*LinearToSRGB(Color.g));
	u32 B = RoundReal32ToUInt32(255.0f*LinearToSRGB(Color.b));
	u32 A = RoundReal32ToUInt32(255.0f);
	u32 Result = (A << 24) | (R << 16) | (G << 8) | (B << 0);
	return Result;
}

inline f32
RandomUnilateral()
{
	f32 Result = (f32)rand()/RAND_MAX;
	return Result;
}

inline f32
RandomBilateral()
{
	f32 Result = -1.0f + 2.0f*RandomUnilateral();
	return Result;
}

struct ray_cast_result
{
	f32 ClosestHit;
	material *MaterialHit;
	v3 HitNormal;
};

internal ray_cast_result
SingleRayCast(world *World, v3 RayOrigin, v3 RayDirection)
{
	Assert(LengthSq(RayDirection) != 0.0f);

	ray_cast_result Result = {};
	Result.ClosestHit = Real32Maximum;

	f32 Tolerance = 0.0001f;

	for(u32 ObjectIndex = 0;
		ObjectIndex < World->ObjectCount;
		++ObjectIndex)
	{
		object *Object = World->Objects + ObjectIndex;
		switch(Object->Type)
		{
			case Object_Plane:
			{
				plane *Plane = &Object->Plane;
				f32 Denom = Inner(Plane->Normal, RayDirection);
				if((Denom > Tolerance) || (Denom < -Tolerance))
				{
					f32 DistanceToHit = (Plane->Offset - Inner(Plane->Normal, RayOrigin))/Denom;
					if((DistanceToHit > Tolerance) && (DistanceToHit < Result.ClosestHit))
					{
						Result.ClosestHit = DistanceToHit;
						Result.MaterialHit = &Object->Material;
						Result.HitNormal = Plane->Normal;
					}
				}
			} break;

			case Object_Sphere:
			{
				sphere *Sphere = &Object->Sphere;

				v3 SphereRelativeCenter = RayOrigin - Sphere->Center;

				f32 a = Inner(RayDirection, RayDirection);
				f32 b = 2.0f*Inner(RayDirection, SphereRelativeCenter);
				f32 c = Inner(SphereRelativeCenter, SphereRelativeCenter) - Square(Sphere->Radius);
				f32 Determinant = Square(b) - 4.0f*a*c;
				if(Determinant > Tolerance)
				{
					f32 DistanceToHitPos = (-b + SquareRoot(Determinant)) / (2.0f*a);
					f32 DistanceToHitNeg = (-b - SquareRoot(Determinant)) / (2.0f*a);

					f32 DistanceToHit = DistanceToHitPos;
					if((DistanceToHitNeg > Tolerance) && (DistanceToHitNeg < DistanceToHitPos))
					{
						DistanceToHit = DistanceToHitNeg;
					}

					if((DistanceToHit > Tolerance) && (DistanceToHit < Result.ClosestHit))
					{
						Result.ClosestHit = DistanceToHit;
						Result.MaterialHit = &Object->Material;
						Result.HitNormal = NOZ(RayOrigin + Result.ClosestHit*RayDirection - Sphere->Center);
					}
				}
			} break;
		}
	}

	return Result;
}

internal v3
RayCast(world *World, v3 RayOrigin, v3 RayDirection, u32 MaxBounces, volatile u32 *RaysCast)
{
	v3 Result = {};
	v3 Attenuation = V3(1.0f, 1.0f, 1.0f);
	b32 InsideObject = false;
	u32 RayCount = 0;

	for(u32 BounceIndex = 0;
		BounceIndex < MaxBounces;
		++BounceIndex)
	{
		++RayCount;
		ray_cast_result RayCastResult = SingleRayCast(World, RayOrigin, RayDirection);

		f32 ClosestHit = RayCastResult.ClosestHit;
		material *MaterialHit = RayCastResult.MaterialHit;
		v3 HitNormal = RayCastResult.HitNormal;

		if(MaterialHit)
		{
			v3 NewRayOrigin = RayOrigin + ClosestHit*RayDirection;
			f32 CosIncidentAngle = Inner(-RayDirection, HitNormal);
			if(CosIncidentAngle < 0.0f) {CosIncidentAngle = -CosIncidentAngle;}
			v3 PureBounce = RayDirection + 2.0f*CosIncidentAngle*HitNormal;

			if(MaterialHit->Transparent)
			{
				f32 RefractionIndexRatio = InsideObject ? MaterialHit->RefractionIndex : 1.0f/MaterialHit->RefractionIndex;
				f32 Radical = 1.0f - Square(RefractionIndexRatio)*(1.0f - Square(CosIncidentAngle));
				if(Radical < 0.0f)
				{
					// NOTE: Total internal reflection;
					RayOrigin = NewRayOrigin;
					RayDirection = PureBounce;
				}
				else
				{
					v3 Refraction = RefractionIndexRatio*RayDirection +
						(RefractionIndexRatio*CosIncidentAngle - SquareRoot(Radical))*HitNormal;
					f32 CosRefractionAngle = Inner(-Refraction, HitNormal);
					f32 OldRefIndex = InsideObject ? MaterialHit->RefractionIndex : 1.0f;
					f32 NewRefIndex = InsideObject ? 1.0f : MaterialHit->RefractionIndex;
					f32 FresnelParallel = Square((NewRefIndex*CosIncidentAngle - OldRefIndex*CosRefractionAngle)/(NewRefIndex*CosIncidentAngle + OldRefIndex*CosRefractionAngle));
					f32 FresnelPerp = Square((OldRefIndex*CosRefractionAngle - NewRefIndex*CosIncidentAngle)/(OldRefIndex*CosRefractionAngle + NewRefIndex*CosIncidentAngle));

					f32 ReflectRatio = 0.5f*(FresnelParallel + FresnelPerp);
					b32 Reflected = (RandomUnilateral() < ReflectRatio);

					if(Reflected)
					{
						RayOrigin = NewRayOrigin;
						RayDirection = PureBounce;
					}
					else
					{
						RayOrigin = NewRayOrigin;
						RayDirection = Refraction;
						InsideObject = !InsideObject;
					}
				}
			}
			else
			{
				Attenuation = Hadamard(Attenuation, CosIncidentAngle*MaterialHit->ReflectionColor);

				// NOTE: Shadow ray
				++RayCount;
				v3 RandomDirection = NOZ(V3(RandomBilateral(), RandomBilateral(), RandomBilateral()));
				v3 LightDirection = NOZ(-World->LightDirection + 0.1f*RandomDirection);
				ray_cast_result ShadowRayCast = SingleRayCast(World, NewRayOrigin, LightDirection);
				if(!ShadowRayCast.MaterialHit)
				{
					Result += Hadamard(Attenuation, World->LightColor);
				}
				Result += Hadamard(Attenuation, MaterialHit->EmitColor);

				RayOrigin = NewRayOrigin;
				v3 RandomBounce = NOZ(V3(RandomBilateral(), RandomBilateral(), RandomBilateral()));
				if(Inner(RandomBounce, HitNormal) < 0)
				{
					RandomBounce = -RandomBounce;
				}
				RayDirection = NOZ(Lerp(RandomBounce, MaterialHit->Specularity, PureBounce));
			}
		}
		else
		{
			Result += Hadamard(Attenuation, World->NullMaterial.EmitColor);
			break;
		}
	}

	Clamp01(Result.r);
	Clamp01(Result.g);
	Clamp01(Result.b);

	LockedAddAndReturnPreviousValue(RaysCast, RayCount);

	return Result;
}

inline u32 *
GetPixelPointer(image *Image, u32 X, u32 Y)
{
	u32 *Result = Image->Pixels + Y*Image->Width + X;
	return Result;
}

internal void
RenderTile(work_queue *WorkQueue)
{
	u32 WorkOrderIndex = LockedAddAndReturnPreviousValue(&WorkQueue->NextWorkIndex, 1);
	if(WorkOrderIndex < WorkQueue->TileQueueSize)
	{
		tile_work_order *WorkOrder = WorkQueue->TileWorkQueue + WorkOrderIndex;
		world *World = WorkOrder->World;
		image *Image = WorkOrder->Image;
		u32 MinX = WorkOrder->MinX;
		u32 MinY = WorkOrder->MinY;
		u32 OnePastMaxX = WorkOrder->OnePastMaxX;
		u32 OnePastMaxY= WorkOrder->OnePastMaxY;
		u32 RaysPerPixel = WorkOrder->RaysPerPixel;

		for(u32 Y = MinY;
			Y < OnePastMaxY;
			++Y)
		{
			u32 *Dest = GetPixelPointer(Image, MinX, Y);

			for(u32 X = MinX;
				X < OnePastMaxX;
				++X)
			{
				v3 Color = {};
				f32 Contrib = 1.0f/RaysPerPixel;

				for(u32 RayIndex = 0;
					RayIndex < RaysPerPixel;
					++RayIndex)
				{
					f32 XRatio = -1.0f + 2.0f*((((f32)X) + 0.5f*RandomBilateral())/(f32)Image->Width);
					f32 YRatio = -1.0f + 2.0f*((((f32)Y) + 0.5f*RandomBilateral())/(f32)Image->Height);
					v3 FilmPoint = World->FilmP + XRatio*World->HalfFilmW*World->CameraX + YRatio*World->HalfFilmH*World->CameraY;

					v3 RayOrigin = World->CameraP;
					v3 RayDirection = NOZ(FilmPoint - RayOrigin);

					Color += Contrib*RayCast(World, RayOrigin, RayDirection, WorkOrder->MaxBounces, &WorkQueue->RaysCast);
				}

				*Dest++ = PackLinear01ToSRGBU32(Color);
			}
		}

		LockedAddAndReturnPreviousValue(&WorkQueue->TilesCompleted, 1);
	}
}

s32 main(s32 ArgumentCount, char **Arguments)
{
	printf("Raycasting...");
	fflush(stdout);

	// image Image = AllocateImage(640, 480);
	image Image = AllocateImage(1280, 720);
	u32 RaysPerPixel = 64;
	u32 MaxBounces = 8;

	world World = {};

	World.NullMaterial.EmitColor = V3(0.1f, 0.1f, 0.1f);
	World.LightDirection = NOZ(V3(1.0f, -1.0f, -1.0f));
	World.LightColor = V3(0.7f, 0.7f, 0.7f);

	object *Plane = World.Objects + World.ObjectCount++;
	Plane->Type = Object_Plane;
	Plane->Plane.Normal = V3(0.0f, 1.0f, 0.0f);
	Plane->Plane.Offset = 0.0f;
	Plane->Material.ReflectionColor = V3(0.1f, 0.1f, 0.1f);
	Plane->Material.Specularity = 0.05f;

	f32 Radius = 1.0f;
	v3 Start = V3(-4.0f, 1.0f, -4.0f);
	v3 End = V3(4.0f, 1.0f, 4.0f);
	u32 Rows = 3;
	u32 Columns = 4;
	u32 SphereCount = Rows*Columns;
	f32 dX = (End.x - Start.x) / (Columns - 1);
	f32 dZ = (End.z - Start.z) / (Rows - 1);

	u32 Index = 0;
	for(u32 Z = 0;
		Z < Rows;
		++Z)
	{
		for(u32 X = 0;
			X < Columns;
			++X)
		{
			f32 T = (f32)Index/(SphereCount - 1);

			object *Sphere = World.Objects + World.ObjectCount++;
			Sphere->Type = Object_Sphere;
			Sphere->Sphere.Center = Start + V3(X*dX, 0.0f, Z*dZ);
			Sphere->Sphere.Radius = Radius;
			Sphere->Material.ReflectionColor = Lerp(V3(0.9f, 0.1f, 0.1f), T, V3(0.5f, 0.5f, 0.1f));
			Sphere->Material.Specularity = Lerp(1.0f, T, 0.00f);

			if(Z == 1 && X == 2)
			{
				Sphere->Sphere.Center.y += 0.6f;
				Sphere->Material.ReflectionColor = V3(0.1f, 0.1f, 0.6f);
				Sphere->Material.Specularity = 1.0f;
				Sphere->Material.Transparent = true;
				Sphere->Material.RefractionIndex = 1.52f; // NOTE: Glass
			}
			if(Z == 1 && X == 1)
			{
				Sphere->Material.EmitColor = 2.0f*Sphere->Material.ReflectionColor;
			}

			++Index;
		}
	}

	World.CameraP = V3(0.0f, 6.0f, 10.0f);
	World.CameraZ = NOZ(World.CameraP);
	World.CameraX = NOZ(Cross(V3(0.0f, 1.0f, 0.0f), World.CameraZ));
	World.CameraY = NOZ(Cross(World.CameraZ, World.CameraX));

	f32 FilmD = 1.0f;
	World.FilmP = World.CameraP - FilmD*World.CameraZ;
	f32 FilmW = 1.0f;
	f32 FilmH = 1.0f;
	if(Image.Width > Image.Height)
	{
		FilmH = (f32)Image.Height / (f32)Image.Width;
	}

	else if(Image.Width < Image.Height)
	{
		FilmW = (f32)Image.Width / (f32)Image.Height;
	}
	World.HalfFilmW = 0.5f * FilmW;
	World.HalfFilmH = 0.5f * FilmH;

	u32 TileDim = 8;
	u32 TileWidth = (Image.Width + TileDim - 1) / TileDim;
	u32 TileHeight = (Image.Height + TileDim - 1) / TileDim;
	u32 TileCount = (TileDim*TileDim);

	work_queue WorkQueue = {};

	for(u32 TileY = 0;
		TileY < TileDim;
		++TileY)
	{
		for(u32 TileX = 0;
			TileX < TileDim;
			++TileX)
		{
			tile_work_order *Work = WorkQueue.TileWorkQueue + WorkQueue.TileQueueSize++;
			Work->Image = &Image;
			Work->World = &World;
			Work->MinX = TileX*TileWidth;
			Work->OnePastMaxX = Minimum(Work->MinX + TileWidth, Image.Width);
			Work->MinY = TileY*TileHeight;
			Work->OnePastMaxY = Minimum(Work->MinY + TileHeight, Image.Height);
			Work->RaysPerPixel = RaysPerPixel;
			Work->MaxBounces = MaxBounces;
		}
	}

	clock_t Tick = clock();

	// TODO: complete previous writes
	LockedAddAndReturnPreviousValue(&WorkQueue.NextWorkIndex, 0);

	u32 ThreadCount = 3;
	ThreadStart(&WorkQueue, ThreadCount);

	while(WorkQueue.TilesCompleted < TileCount)
	{
		RenderTile(&WorkQueue);

		u32 Progress = WorkQueue.TilesCompleted;
		printf("\rRaycasting... %d%%", (u32)((100.0f*(Progress)) / TileCount));
		fflush(stdout);
	}

	clock_t Tock = clock();
	f64 ElapsedMS = 1000.0*(f64)(Tock - Tick)/CLOCKS_PER_SEC;

	WriteImage(&Image, "test.bmp");

	printf("\rRaycasting... Done.\n");
	printf("Time: %f s\n", ElapsedMS/1000.0);
	printf("Rays: %d\n", WorkQueue.RaysCast);
	printf("ms/ray: %f ms\n", ElapsedMS/WorkQueue.RaysCast);

	return 0;
}
