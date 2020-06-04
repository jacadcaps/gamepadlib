#include "gamepadlib.h"
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
	gmlibHandle *gm = gmlibInitialize("test", 0);

	if (gm)
	{
		int padCount = 0;

		for (;;)
		{
			Delay(3);

			gmlibUpdate(gm);
			
			int pads = 0;
			
			for (int i = gmlibSlotMin; i <= gmlibSlotMax; i++)
			{
				if (gmlibGetGamepad(gm, i, NULL))
				{
					gmlibGamepadData data;
					gmlibGetData(gm, i, &data);
					pads ++;
				}
			}

			if (pads != padCount)
			{
				Printf("Pad count %ld\n", pads);
				padCount = pads;
			}
			
			if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
				break;
		}

		gmlibShutdown(gm);
	}
};
