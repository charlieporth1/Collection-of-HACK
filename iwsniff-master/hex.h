/*
  hex.h

  Hexadecimal conversion routines.
  
  Copyright (c) 2000 Dug Song <dugsong@monkey.org>

  $Id: hex.h,v 1.1.1.1 2004-07-30 11:36:18 cmn Exp $
*/

#ifndef HEX_H
#define HEX_H

int	hex_decode(char *src, int srclen, u_char *buf, int len);

void	hex_print(const u_char *buf, int len, int offset);
	       
#endif /* HEX_H */

