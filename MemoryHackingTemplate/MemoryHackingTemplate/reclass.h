// Created with ReClass.NET 1.2 by KN4CK3R
#include "math.h"

class Player
{
public:
	char pad_0000[84]; //0x0000
	Vector3 Coordinates; //0x0054
	char pad_0060[472]; //0x0060
	float Health; //0x0238
	char pad_023C[8]; //0x023C
	float Mana; //0x0244
	char pad_0248[8]; //0x0248
	float Stamina; //0x0250
	char pad_0254[3572]; //0x0254
}; //Size: 0x1048