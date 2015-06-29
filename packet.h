#ifndef __PACKET_H__
#define __PACKET_H__

class Packet{
private:
   uint8 op;
   uint8* data;
   uint16 index;
   uint16 len;
public:
   Packet(uint8 opcode, uint16 length);

   int length();

   Packet operator<<(uint8 byte);
   Packet operator<<(uint8 bytes[]);
   Packet operator<<(uint16 bytes);
   Packet operator<<(uint32 bytes);
   
   uint8 & operator[](int i);

   operator uint8*();
   operator int();
};


#endif