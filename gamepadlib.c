#include <proto/sensors.h>
#include <proto/exec.h>
#include <libraries/sensors.h>
#include <libraries/sensors_hid.h>
#include <string.h>

#include "gamepadlib.h"

struct internalSlot
{
	gmlibGamepad     _pad;
	gmlibGamepadData _data;
	APTR             _notify; // MUST be present if the gamepad is valid
};

struct internalHandle
{
	APTR _pool;
	struct Library *_sensorsBase;
	struct internalSlot _slots[gmlibSlotMax - 1];
};

static BOOL gmlibSetupGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor);
static BOOL gmlibSetupHIDGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor);
static void gmlibReleaseAll(struct internalHandle *ihandle);

#define GH(__x__) ((gmlibHandle *)__x__)
#define IH(__x__) ((struct internalHandle *)__x__)

gmlibHandle *gmlibInitialize(const char *gameID, ULONG flags)
{
	(void)gameID;
	(void)flags;

	APTR pool = CreatePool(MEMF_ANY, 4096, 2048);
	if (pool)
	{
		struct internalHandle *handle = (struct internalHandle *)AllocPooled(pool, sizeof(struct internalHandle));
		
		if (handle)
		{
			handle->_pool = pool;
			handle->_sensorsBase = OpenLibrary("sensors.library", 53);
			
			for (int i = 0; i < gmlibSlotMax; i++)
				handle->_slots[i]._notify = NULL;

			if (handle->_sensorsBase)			
			{
				gmlibRenumerate(GH(handle));
				return GH(handle);
			}
		}
		
		DeletePool(pool);
	}		

	return NULL;	
}

void gmlibShutdown(gmlibHandle *handle)
{
	if (NULL != handle)
	{
		struct internalHandle *ihandle = IH(handle);
		gmlibReleaseAll(ihandle);
		if (ihandle->_sensorsBase)
			CloseLibrary(ihandle->_sensorsBase);
		DeletePool(ihandle->_pool);
	}		
}

void gmlibUpdate(gmlibHandle *handle)
{
	// handle removed gamepads
	
	// handle added gamepads
	
	// poll present gamepads...
}

BOOL gmlibGetGamepad(gmlibHandle *handle, ULONG slot, gmlibGamepad *outGamepad)
{
	if (handle && outGamepad && slot >= gmlibSlotMin && slot <= gmlibSlotMax)
	{
		struct internalHandle *ihandle = IH(handle);		
		struct internalSlot *islot = &ihandle->_slots[slot - 1];
		if (islot->_notify)
		{
			memcpy(outGamepad, &islot->_pad, sizeof(*outGamepad));
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL gmlibSetupGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor)
{
	// todo
	return FALSE;
}

static BOOL gmlibSetupHIDGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor)
{
	// todo
	return FALSE;
}

static void gmlibReleaseAll(struct internalHandle *ihandle)
{
	for (int i = 0; i < gmlibSlotMax; i++)
	{
		struct internalSlot *islot = &ihandle->_slots[i];
		
		if (islot->_notify)
		{
			EndSensorNotify(islot->_notify, NULL);
			islot->_notify = NULL;
		}
	}
}

void gmlibRenumerate(gmlibHandle *handle)
{
	if (handle)
	{
		struct internalHandle *ihandle = IH(handle);
		struct Library *SensorsBase = ihandle->_sensorsBase;
		APTR sensors;
		LONG slots = gmlibSlotMax;

		struct TagItem gamepadListTags[] = {
			SENSORS_Class, SensorClass_HID,
			SENSORS_Type, SensorType_HID_Gamepad,
			TAG_DONE
		};

		struct TagItem hidListTags[] = {
			SENSORS_Class, SensorClass_HID,
			SENSORS_Type, SensorType_HID_Generic,
			TAG_DONE
		};
		
		// release all gamepads...
		gmlibReleaseAll(ihandle);

		// prefer actual gamepads to random hid devices
		if ((sensors = ObtainSensorsList(gamepadListTags)))
		{
			// setup the gamepad...
			APTR sensor = NULL;
			while ((sensor = NextSensor(sensor, sensors, NULL)) && slots > 0)
			{
				if (gmlibSetupGamepad(ihandle, slots - gmlibSlotMax, sensor))
					slots --;
			}
			
			ReleaseSensorsList(sensors, NULL);
		}
		
		if (slots > 0)
		{
			if ((sensors = ObtainSensorsList(hidListTags)))
			{
				APTR sensor = NULL;
				while ((sensor = NextSensor(sensor, sensors, NULL)) && slots > 0)
				{
					if (gmlibSetupHIDGamepad(ihandle, slots - gmlibSlotMax, sensor))
						slots --;
				}
				ReleaseSensorsList(sensors, NULL);
			}
		}
	}
}

void gmlibGetData(gmlibHandle *handle, ULONG slot, gmlibGamepadData *outData)
{
	if (handle && outData && slot >= gmlibSlotMin && slot <= gmlibSlotMax)
	{
		struct internalHandle *ihandle = IH(handle);		
		struct internalSlot *islot = &ihandle->_slots[slot - 1];
		memcpy(outData, &islot->_data, sizeof(*outData));
	}
}

void gmlibSetRumble(gmlibHandle *handle, ULONG slot, DOUBLE smallMotorPower, DOUBLE largeMotorPower, ULONG msDuration)
{
	// todo
}
