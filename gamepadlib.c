#include <proto/sensors.h>
#include <libraries/sensors.h>
#include "gamepadlib.h"

gmlibHandle *gmlibInitialize(const char *gameID, ULONG flags)
{
	(void)gameID;
	(void)flags;
	

	return NULL;	
}

void gmlibShutdown(gmlibHandle *handle)
{
}

void gmlibUpdate(gmlibHandle *handle)
{
}

BOOL gmlibGetGamepad(gmlibHandle *handle, ULONG slot, gmlibGamepad *outGamepad)
{
	return FALSE;
}

void gmlibRenumerate(gmlibHandle *handle)
{
}

void gmlibGetData(gmlibHandle *handle, ULONG slot, gmlibGamepadData *outData)
{
}

void gmlibSetRumble(gmlibHandle *handle, ULONG slot, DOUBLE smallMotorPower, DOUBLE largeMotorPower, ULONG msDuration)
{
}
