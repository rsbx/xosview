//
//  Copyright (c) 2013 by Raymond S Brand
//
//  This file may be distributed under the terms of (any) BSD license or equivalent.
//

// Format a value for humans.


#include <stdio.h>
#include "formatnum.h"


#ifndef __cplusplus
#define bool int
#define false !!0
#define true !false
#endif


#define SUFFIXES_UP	" KMGTPEZY"
#define SUFFIXES_DOWN	" munpfazy"


// Format a value into 4 chars for humans.
char *FormatNum4(char *buffer, size_t size, enum FormatNum_Formats format_type, double value)
	{
	char *suffix = " ";
	char *format;
	bool do_up = false;
	bool do_down = false;
	double scale_order;
	double scale_thres;

	if (!buffer || !size)
		{
		return buffer;			// GIGO
		}

	if (value < 0.0)
		{
		snprintf(buffer, size, "-");
		return buffer;
		}

	switch(format_type)
		{
		case FormatNum_Percent:
			{
			suffix = "%";
			break;
			}
		case FormatNum_Scale_1000_Up:
			{
			do_up = true;
			scale_order = 1000;
			scale_thres = 1000;
			break;
			}
		case FormatNum_Scale_1024_Up:
			{
			do_up = true;
			scale_order = 1024;
			scale_thres = 1024;	// set to 1000 to NOT spread the error...
			break;
			}
		case FormatNum_Scale_1000:
			{
			do_up = true;
			do_down = true;
			scale_order = 1000;
			scale_thres = 1000;
			break;
			}
		case FormatNum_Scale_1024:
			{
			do_up = true;
			do_down = true;
			scale_order = 1024;
			scale_thres = 1024;	// set to 1000 to NOT spread the error...
			break;
			}
		default:
			{
			*buffer = '\0';
			return buffer;
			}
		}

	if (do_up && value+0.5 >= scale_thres)
		{
		suffix = SUFFIXES_UP;
		while (*suffix && value+0.5 >= scale_thres)
			{
			value /= scale_order;
			suffix++;
			}
		if (*suffix == '\0')
			{
			snprintf(buffer, size, "****");
			return buffer;
			}
		value = value*1000.0/scale_thres;	// spread the error...
		}
	else if (do_down && value != 0.0 && value*scale_order+0.5 <= scale_thres)
		{
		suffix = SUFFIXES_DOWN;
		while (*suffix && value*scale_order+0.5 <= scale_thres)
			{
			value *= scale_order;
			suffix++;
			}
		if (*suffix == '\0')
			{
			snprintf(buffer, size, "....");
			return buffer;
			}
		value = value*1000.0/scale_thres;	// spread the error...
		}

	if (*suffix == ' ')
		{
		if (value+0.05 >= 100)
			{
			format = "%.0f";	// 9999
			}
		else if (value+0.005 >= 10)
			{
			format = "%.1f";	// 99.9
			}
		else if (value+0.005 >= 0.01)
			{
			format = "%.2f";	// 9.99
			}
		else
			{
			format = "%.0f";	// 0
			}
		snprintf(buffer, size, format, value);
		}
	else
		{
		if (value+0.05 >=10)
			{
			format = "%.0f%c";	// 999X
			}
		else if (value+0.005 >= 0.1)
			{
			format = "%.1f%c";	// 9.9X
			}
		else
			{
			format = "%.0f%c";	// 0X
			}
		snprintf(buffer, size, format, value, *suffix);
		}

	return buffer;
	}


#ifdef TEST


#include <stdlib.h>


int main(int argc, const char **argv)
	{
	char buffer[1024];
	double val;

	if (argc != 2)
		{
		fprintf(stderr, "Usage: %s <number>\n", argv[0], argv[0]);
		exit(1);
		}

	val = strtod(argv[1], NULL);

	printf("\t\"%4s\"", FormatNum4(buffer, 1024, FormatNum_Percent,       val));
	printf("\t\"%4s\"", FormatNum4(buffer, 1024, FormatNum_Scale_1000_Up, val));
	printf("\t\"%4s\"", FormatNum4(buffer, 1024, FormatNum_Scale_1024_Up, val));
	printf("\t\"%4s\"", FormatNum4(buffer, 1024, FormatNum_Scale_1000,    val));
	printf("\t\"%4s\"", FormatNum4(buffer, 1024, FormatNum_Scale_1024,    val));
	printf("\n");
	}


#endif
