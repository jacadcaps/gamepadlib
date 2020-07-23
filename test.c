#include "gamepadlib.h"
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
	gmlibHandle *gm = gmlibInitialize("test", 0);

	if (gm)
	{
		int padCount = 0;
		BOOL noisyList = TRUE;

		for (;;)
		{
			Delay(3);

			gmlibUpdate(gm);
			
			int pads = 0;
			
			for (int i = gmlibSlotMin; i <= gmlibSlotMax; i++)
			{
				gmlibGamepad pad;
				if (gmlibGetGamepad(gm, i, noisyList ? &pad : NULL))
				{
					gmlibGamepadData data;
					gmlibGetData(gm, i, &data);
					if (noisyList)
					{
						Printf("Pad %s (%lx/%lx) hasBattery %ld (%ld) hasRumble %ld\n", pad._name, pad._vid, pad._pid, pad._hasBattery, (int)(data._battery * 100), pad._hasRumble);
					}
					pads ++;
				}
			}

			noisyList = FALSE;
			
			if (pads != padCount)
			{
				Printf(">> Pad count %ld\n", pads);
				padCount = pads;
				noisyList = TRUE;
			}
			
			if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
				break;
			
			if (SetSignal(0L, SIGBREAKF_CTRL_E) & SIGBREAKF_CTRL_E)
				gmlibRenumerate(gm); 

			if (SetSignal(0L, SIGBREAKF_CTRL_D) & SIGBREAKF_CTRL_D)
				gmlibSetRumble(gm, gmlibSlotMin, 1.0, 1.0, 1000);
		}

		gmlibShutdown(gm);
	}
};
