#ifndef __SHARED_H
#define __SHARED_H

#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <time.h>
#include <queue>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
using namespace std;

#include "xmlParser.h"

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;
typedef signed int 		int32;

#include "crypto.h"
#include "packet.h"
#include "structs.h"

	
#endif