#ifndef COMMON_MATH_H_
#define COMMON_MATH_H_

static unsigned power_of_2_max_u(unsigned x)
{
	x -= 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}

#endif // !COMMON_MATH_H_

