#ifndef DBOP_H
#define DBOP_H

#include <stdio.h>

// Enable debug prints by uncommenting the desired line below
#define DEBUG_INFO_DeskLink
#define DEBUG_INFO_Led
#define DEBUG_INFO_PushButton

#ifdef DEBUG_INFO_DeskLink
  // All debug prints for DeskLink come out in the same format
  #define C_DeskLink(msg) printf("Dbg-DeskLink: %s\n", msg)
#else
  // If DEBUG_INFO_DeskLink is not defined, C_DeskLink becomes an empty macro
  #define C_DeskLink(msg)
#endif

#ifdef DEBUG_INFO_Led
  // All debug prints for Led come out in the same format
  #define C_Led(msg) printf("Dbg-Led: %s\n", msg)
#else
  // If DEBUG_INFO_Led is not defined, C_Led becomes an empty macro
  #define C_Led(msg)
#endif

#ifdef DEBUG_INFO_PushButton
  // All debug prints for PushButton come out in the same format
  #define C_PushButton(msg) printf("Dbg-PushButton: %s\n", msg)
#else
  // If DEBUG_INFO_PushButton is not defined, C_PushButton becomes an empty macro
  #define C_PushButton(msg)
#endif

#endif // DBOP_H

