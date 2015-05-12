
#include "Util.h"

bool isLittleEndian()
{
	int n = 1;
	return (*(char *)&n == 1);
}
