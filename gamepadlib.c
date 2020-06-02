#define __NOLIBBASE__
#include <proto/sensors.h>
#undef __NOLIBBASE__
#include <proto/exec.h>
#include <proto/utility.h>
#include <libraries/sensors.h>
#include <libraries/sensors_hid.h>
#include <string.h>

#include "gamepadlib.h"

#ifdef DEBUG
#include <stdio.h>
#define D(x) x
#else
#define D(x)
#endif

typedef struct _gmlibGamepadDataInternal
{
	// Two analog joysticks - sensors (from childlist, don't free)
	APTR _leftStickSensor;
	APTR _rightStickSensor;
	// Two analog triggers - sensors (from childlist, don't free)
	APTR _leftTriggerSensor;
	APTR _rightTriggerSensor;
	// Buttons - notifies
	APTR _dpadLeftSensor;
	APTR _dpadRightSensor;
	APTR _dpadUpSensor;
	APTR _dpadDownSensor;
	APTR _backSensor;
	APTR _startSensor;
	APTR _leftStickButtonSensor;
	APTR _rightStickButtonSensor;
	APTR _xLeftSensor;
	APTR _yTopSensor;
	APTR _aBottomSensor;
	APTR _bRightSensor;
	APTR _shoulderLeftSensor;
	APTR _shoulderRightSensor;
} gmlibGamepadDataInternal;

struct internalSlot
{
	gmlibGamepad             _pad;
	gmlibGamepadData         _data;
	APTR                     _notify; // MUST be present if the gamepad is valid, removal notification
	APTR                     _childList;
	gmlibGamepadDataInternal _internal;
};

#define GET_SLOT(_x_) ((_x_ >> 24) - 1)
#define GET_INDEX(_x_) (_x_ & 0xFF)
#define SET_SLOT(_x_, _y_) _x_ |= (((_y_ + 1) << 24) & 0xF)
 
struct internalHandle
{
	APTR _pool;
	struct Library *_sensorsBase;
	struct MsgPort *_port;
	struct internalSlot _slots[gmlibSlotMax];
};

static BOOL gmlibSetupGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor);
static BOOL gmlibSetupHIDGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor);
static void gmlibReleaseAll(struct internalHandle *ihandle);
static void gmlibRealseSlot(struct internalHandle *ihandle, struct internalSlot *islot);

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
			memset(handle, 0, sizeof(struct internalHandle));

			handle->_pool = pool;
			handle->_sensorsBase = OpenLibrary("sensors.library", 53);
			handle->_port = CreateMsgPort();

			if (handle->_sensorsBase && handle->_port)
			{
				D(printf("%s: initialized\n", __PRETTY_FUNCTION__));
				gmlibRenumerate(GH(handle));
				return GH(handle);
			}
			
			if (handle->_port)
				DeleteMsgPort(handle->_port);
			if (handle->_sensorsBase)
				CloseLibrary(handle->_sensorsBase);
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
		struct SensorsNotificationMessage *s;

		D(printf("%s: bye\n", __PRETTY_FUNCTION__));

		gmlibReleaseAll(ihandle);
		while ((s = (struct SensorsNotificationMessage *)GetMsg(ihandle->_port)))
		{
			ReplyMsg(&s->Msg);
		}

		DeleteMsgPort(ihandle->_port);
		if (ihandle->_sensorsBase)
			CloseLibrary(ihandle->_sensorsBase);

		DeletePool(ihandle->_pool);
	}		
}

void gmlibUpdate(gmlibHandle *handle)
{
	struct internalHandle *ihandle = IH(handle);

	if (ihandle)
	{
		struct Library *SensorsBase = ihandle->_sensorsBase;
		struct SensorsNotificationMessage *s;

		// clear previous button states
		for (int i = 0; i < gmlibSlotMax; i++)
		{
			struct internalSlot *islot = &ihandle->_slots[i];
			if (islot->_notify)
			{
				islot->_data._buttons._all = 0;
			}
		}

		while ((s = (struct SensorsNotificationMessage *)GetMsg(ihandle->_port)))
		{
			if (s->UserData)
			{
				ULONG idx = GET_INDEX((ULONG)s->UserData);
				ULONG slot = GET_SLOT((ULONG)s->UserData);

				D(printf("button idx %ld slot %ld\n", idx, slot));

				if (slot < gmlibSlotMax && idx < 32)
				{
					struct internalSlot *islot = &ihandle->_slots[slot];
					IPTR valAddr = GetTagData(SENSORS_HIDInput_Value, 0, s->Notifications);
					DOUBLE *val = (DOUBLE *)valAddr;

					if (val != NULL)
					{
						if (*val >= 1.0)
							islot->_data._buttons._all |= idx;
					}
				}
			}
			else
			{
				if (FindTagItem(SENSORS_Notification_Removed, s->Notifications))
				{
					for (int i = 0; i < gmlibSlotMax; i++)
					{
						struct internalSlot *islot = &ihandle->_slots[i];
						if (islot->_notify == s->Sensor)
						{
							gmlibRealseSlot(ihandle, islot);
							break;
						}
					}
				}
			}
			
			ReplyMsg(&s->Msg);
		}

		// For analog inputs we want to get the current reading at time of gmlibUpdate
		for (int i = 0; i < gmlibSlotMax; i++)
		{
			struct internalSlot *islot = &ihandle->_slots[i];
			if (islot->_notify)
			{
				if (islot->_internal._leftStickSensor)
				{
					struct TagItem pollTags[] = {
						{ SENSORS_HIDInput_NS_Value, (IPTR)&islot->_data._leftStick._northSouth },
						{ SENSORS_HIDInput_EW_Value, (IPTR)&islot->_data._leftStick._eastWest },
						{ TAG_DONE }
					};
					
					GetSensorAttr(islot->_internal._leftStickSensor, pollTags);
				}
				
				if (islot->_internal._rightStickSensor)
				{
					struct TagItem pollTags[] = {
						{ SENSORS_HIDInput_NS_Value, (IPTR)&islot->_data._rightStick._northSouth },
						{ SENSORS_HIDInput_EW_Value, (IPTR)&islot->_data._rightStick._eastWest },
						{ TAG_DONE }
					};
					
					GetSensorAttr(islot->_internal._rightStickSensor, pollTags);
				}
				
				if (islot->_internal._leftTriggerSensor)
				{
					struct TagItem pollTags[] = {
						{ SENSORS_HIDInput_Value, (IPTR)&islot->_data._leftTrigger },
						{ TAG_DONE }
					};
					
					GetSensorAttr(islot->_internal._leftTriggerSensor, pollTags);
				}

				if (islot->_internal._rightTriggerSensor)
				{
					struct TagItem pollTags[] = {
						{ SENSORS_HIDInput_Value, (IPTR)&islot->_data._rightTrigger },
						{ TAG_DONE }
					};
					
					GetSensorAttr(islot->_internal._rightTriggerSensor, pollTags);
				}
			}
		}
	}
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

static BOOL gmlibSetupGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR parent)
{
	struct Library *SensorsBase = ihandle->_sensorsBase;
	struct TagItem tags[] = {
		{SENSORS_Parent, (IPTR)parent},
		{SENSORS_Class, SensorClass_HID},
		{TAG_DONE},
	};

	APTR sensors = ObtainSensorsList(tags);

	if (sensors)
	{
		struct internalSlot *islot = &ihandle->_slots[slotidx];
		APTR sensor = NULL;

		while ((sensor = NextSensor(sensor, sensors, NULL)) != NULL)
		{
			ULONG type = 0, id = 0, limb = Sensor_HIDInput_Limb_Unknown;
			STRPTR name = NULL;

			struct TagItem qt[] = {
				{SENSORS_Type, (IPTR)&type},
				{SENSORS_HIDInput_Name, (IPTR)&name},
				{SENSORS_HIDInput_ID, (IPTR)&id},
				{SENSORS_HIDInput_Limb, (IPTR)&limb},
				{TAG_DONE}
			};

			if (GetSensorAttr(sensor, qt) > 0)
			{
				switch (type)
				{
				case SensorType_HIDInput_Trigger:
					{
						struct TagItem tags[] = {
							{SENSORS_Notification_UserData, 0},
							{SENSORS_Notification_Destination, (IPTR)ihandle->_port},
							{SENSORS_Notification_SendInitialValue, TRUE},
							{SENSORS_HIDInput_Value, 1},
							{TAG_DONE}
						};

						// need to map the buttons to their functions
						// not ideal but there's currently no other way to reliably do that
						if (0 == strcmp(name, "Shoulder Button Left"))
						{
							tags[0].ti_Data = 12; // bit number
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._shoulderLeftSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Shoulder Button Right"))
						{
							tags[0].ti_Data = 13;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._shoulderRightSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "A Button")) || (0 == strcmp(name, "Cross Button")))
						{
							tags[0].ti_Data = 10;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._aBottomSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "B Button")) || (0 == strcmp(name, "Circle Button")))
						{
							tags[0].ti_Data = 11;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._bRightSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "X Button")) || (0 == strcmp(name, "Square Button")))
						{
							tags[0].ti_Data = 8;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._xLeftSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "Y Button")) || (0 == strcmp(name, "Triangle Button")))
						{
							tags[0].ti_Data = 9;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._yTopSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Digital Stick Up"))
						{
							tags[0].ti_Data = 2;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._dpadUpSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Digital Stick Down"))
						{
							tags[0].ti_Data = 3;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._dpadDownSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Digital Stick Left"))
						{
							tags[0].ti_Data = 0;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._dpadLeftSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Digital Stick Right"))
						{
							tags[0].ti_Data = 1;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._dpadRightSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Left Analog Joystick Push Button"))
						{
							tags[0].ti_Data = 6;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._leftStickButtonSensor = StartSensorNotify(sensor, tags);
						}
						else if (0 == strcmp(name, "Right Analog Joystick Push Button"))
						{
							tags[0].ti_Data = 7;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._rightStickButtonSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "Menu Button")) || (0 == strcmp(name, "Share Button")) || (0 == strcmp(name, "Start Button")))
						{
							tags[0].ti_Data = 5;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._startSensor = StartSensorNotify(sensor, tags);
						}
						else if ((0 == strcmp(name, "View Button")) || (0 == strcmp(name, "Options Button")) || (0 == strcmp(name, "Back Button")))
						{
							tags[0].ti_Data = 4;
							SET_SLOT(tags[0].ti_Data, slotidx);
							islot->_internal._backSensor = StartSensorNotify(sensor, tags);
						}
					}
					break;
				case SensorType_HIDInput_Analog:
					if (limb == Sensor_HIDInput_Limb_LeftHand)
					{
						if (NULL == islot->_internal._leftTriggerSensor)
							islot->_internal._leftTriggerSensor = sensor;
					}
					else if (limb == Sensor_HIDInput_Limb_RightHand)
					{
						if (NULL == islot->_internal._rightTriggerSensor)
							islot->_internal._rightTriggerSensor = sensor;
					}
					break;
				case SensorType_HIDInput_AnalogStick:
					if (limb == Sensor_HIDInput_Limb_LeftHand)
					{
						if (NULL == islot->_internal._leftStickSensor)
							islot->_internal._leftStickSensor = sensor;
					}
					else if (limb == Sensor_HIDInput_Limb_RightHand)
					{
						if (NULL == islot->_internal._rightStickSensor)
							islot->_internal._rightStickSensor = sensor;
					}
					break;
				}
			}
		}

		struct TagItem nt[] = 
		{
			{SENSORS_Notification_Destination, (IPTR)ihandle->_port},
			{SENSORS_Notification_Removed, TRUE},
			{TAG_DONE}
		};

		islot->_childList = sensors;
		islot->_notify = StartSensorNotify(sensor, nt);
		return TRUE;
	}
	
	return FALSE;
}

static BOOL gmlibSetupHIDGamepad(struct internalHandle *ihandle, ULONG slotidx, APTR sensor)
{
	// todo
	return FALSE;
}

static void gmlibRealseSlot(struct internalHandle *ihandle, struct internalSlot *islot)
{
	struct Library *SensorsBase = ihandle->_sensorsBase;

	if (islot->_childList)
	{
		ReleaseSensorsList(islot->_childList, NULL);
		islot->_childList = NULL;
	}
	
	if (islot->_notify)
	{
		EndSensorNotify(islot->_notify, NULL);
		islot->_notify = NULL;
	}

	if (islot->_internal._dpadDownSensor)
	{
		EndSensorNotify(islot->_internal._dpadDownSensor, NULL);
		islot->_internal._dpadDownSensor = NULL;
	}
	
	if (islot->_internal._dpadLeftSensor)
	{
		EndSensorNotify(islot->_internal._dpadLeftSensor, NULL);
		islot->_internal._dpadLeftSensor = NULL;
	}

	if (islot->_internal._dpadRightSensor)
	{
		EndSensorNotify(islot->_internal._dpadRightSensor, NULL);
		islot->_internal._dpadRightSensor = NULL;
	}

	if (islot->_internal._dpadUpSensor)
	{
		EndSensorNotify(islot->_internal._dpadUpSensor, NULL);
		islot->_internal._dpadUpSensor = NULL;
	}

	if (islot->_internal._backSensor)
	{
		EndSensorNotify(islot->_internal._backSensor, NULL);
		islot->_internal._backSensor = NULL;
	}

	if (islot->_internal._startSensor)
	{
		EndSensorNotify(islot->_internal._startSensor, NULL);
		islot->_internal._startSensor = NULL;
	}

	if (islot->_internal._leftStickButtonSensor)
	{
		EndSensorNotify(islot->_internal._leftStickButtonSensor, NULL);
		islot->_internal._leftStickButtonSensor = NULL;
	}

	if (islot->_internal._rightStickButtonSensor)
	{
		EndSensorNotify(islot->_internal._rightStickButtonSensor, NULL);
		islot->_internal._rightStickButtonSensor = NULL;
	}

	if (islot->_internal._xLeftSensor)
	{
		EndSensorNotify(islot->_internal._xLeftSensor, NULL);
		islot->_internal._xLeftSensor = NULL;
	}

	if (islot->_internal._yTopSensor)
	{
		EndSensorNotify(islot->_internal._yTopSensor, NULL);
		islot->_internal._yTopSensor = NULL;
	}

	if (islot->_internal._aBottomSensor)
	{
		EndSensorNotify(islot->_internal._aBottomSensor, NULL);
		islot->_internal._aBottomSensor = NULL;
	}

	if (islot->_internal._bRightSensor)
	{
		EndSensorNotify(islot->_internal._bRightSensor, NULL);
		islot->_internal._bRightSensor = NULL;
	}

	if (islot->_internal._shoulderLeftSensor)
	{
		EndSensorNotify(islot->_internal._shoulderLeftSensor, NULL);
		islot->_internal._shoulderLeftSensor = NULL;
	}

	if (islot->_internal._shoulderRightSensor)
	{
		EndSensorNotify(islot->_internal._shoulderRightSensor, NULL);
		islot->_internal._shoulderRightSensor = NULL;
	}
}

static void gmlibReleaseAll(struct internalHandle *ihandle)
{
	for (int i = 0; i < gmlibSlotMax; i++)
	{
		struct internalSlot *islot = &ihandle->_slots[i];
		gmlibRealseSlot(ihandle, islot);
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
			{ SENSORS_Class, SensorClass_HID },
			{ SENSORS_Type, SensorType_HID_Gamepad },
			{ TAG_DONE }
		};

		struct TagItem hidListTags[] = {
			{ SENSORS_Class, SensorClass_HID },
			{ SENSORS_Type, SensorType_HID_Generic },
			{ TAG_DONE }
		};
		
		D(printf("%s: releasing gamepads...\n", __PRETTY_FUNCTION__));

		// release all gamepads...
		gmlibReleaseAll(ihandle);

		D(printf("%s: scanning x360 compatibles...\n", __PRETTY_FUNCTION__));

		// prefer actual gamepads to random hid devices
		if ((sensors = ObtainSensorsList(gamepadListTags)))
		{
			// setup the gamepad...
			APTR sensor = NULL;

			while (NULL != (sensor = NextSensor(sensor, sensors, NULL)) && (slots > 0))
			{
				if (gmlibSetupGamepad(ihandle, slots - gmlibSlotMax, sensor))
					slots --;
			}
			
			ReleaseSensorsList(sensors, NULL);
		}
		
		D(printf("%s: slots left %lu\n", __PRETTY_FUNCTION__, slots));
		
		if (slots > 0)
		{
			D(printf("%s: scanning hid compatibles...\n", __PRETTY_FUNCTION__));

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
