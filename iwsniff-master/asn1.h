/*
  asn1.h

  ASN.1 routines.
 
  Copyright (c) 2000 Dug Song <dugsong@monkey.org>
  
  $Id: asn1.h,v 1.1.1.1 2004-07-30 11:36:18 cmn Exp $
*/

#ifndef ASN1_H
#define ASN1_H

#define ASN1_INTEGER	2
#define ASN1_STRING	4
#define ASN1_SEQUENCE	16

int	asn1_type(buf_t buf);
int	asn1_len(buf_t buf);

#endif /* ASN1_H */

