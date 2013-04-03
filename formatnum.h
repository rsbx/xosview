//
//  Copyright (c) 2013 by Raymond S Brand
//
//  This file may be distributed under the terms of (any) BSD license or equivalent.
//

#ifndef _FORMATNUM_H_
#define _FORMATNUM_H_

#ifdef __cplusplus
extern "C" {
#endif


enum FormatNum_Formats
	{
	FormatNum_Percent,
	FormatNum_Scale_1000,
	FormatNum_Scale_1024,
	FormatNum_Scale_1000_Up,
	FormatNum_Scale_1024_Up
	};


char *FormatNum4(char *buffer, size_t size, enum FormatNum_Formats format_type, double value);


#ifdef __cplusplus
}
#endif

#endif
