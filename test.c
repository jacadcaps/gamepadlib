#include "gamepadlib.h"
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
	gmlibHandle *gm = gmlibInitialize("test", 0);

	if (gm)
	{
		for (;;)
		{
			Delay(3);

			gmlibUpdate(gm);
			
			if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
				break;
		}

		gmlibShutdown(gm);
	}
};
