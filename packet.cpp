#include "shared.h"

Packet::Packet(uint8 opcode, uint16 length)
{
   index = 1;
   op = opcode;
   len = length;
   data = new uint8[len];
   memset(data, 0x00, len);
   data[0] = opcode;
}

int Packet::length() {
   return len;
}

Packet Packet::operator<<(uint8 byte)
{
   data[index] = byte;
   index++;
   return *this;
}
Packet Packet::operator<<(uint8 bytes[] )
{
   int size = strlen( (char*)bytes );
   for( int x = 0; x < size; x++ ) {
      data[index] = bytes[x];
      index++;
   }
   return *this;
}

Packet Packet::operator<<(uint16 bytes)
{
   data[index] = (uint8)bytes;
   index++;
   data[index] = (uint8)(bytes >> 8);
   index++;
   return *this;
}
Packet Packet::operator<<(uint32 bytes)
{
   data[index] = (uint8)bytes;
   index++;
   data[index] = (uint8)(bytes >> 8);
   index++;
   data[index] = (uint8)(bytes >> 16);
   index++;
   data[index] = (uint8)(bytes >> 24);
   index++;
   return *this;
}
uint8 & Packet::operator[](int i)
{
   return data[i];
}


Packet::operator uint8*()
{
   return data;
}
Packet::operator int()
{
   return len;
}