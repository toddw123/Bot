#include "shared.h"

#define SLEEP( x ) usleep( x * 1000 )

void *send_thread( void *arg );
void *recv_thread( void *arg );
void *main_thread( void *arg );
void handleUpdatePacket(uint8* data, int len);

queue<Packet> send_queue;
bool ping_success = false, login_success = false;
uint16 player_id;
FILE *out;

int main() {
// Random seed
	srand (time( NULL ));

// Defines
	int sock;
	pthread_t thread1, thread2, thread3;
// Create the socket
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( sock == -1 ) {
		printf( "failed to create socket\n" );
		return -1;
	}
// Pass the socket to the recv_thread
	if( pthread_create( &thread1, NULL,  recv_thread, (void*) &sock ) < 0 ) {
		perror( "could not create thread\n" );
		return 1;
	}
// Pass the socket to the send_thread
	if( pthread_create( &thread2, NULL,  send_thread, (void*) &sock ) < 0 ) {
		perror( "could not create thread\n" );
		return 1;
	}
// Create our main bot thread
	if( pthread_create( &thread3, NULL, main_thread, NULL ) < 0 ) {
		perror( "could not create thread\n" );
		return 1;
	}

	out = fopen( "info.txt", "a+" );

	//Packet test( 0x25, 14 );
	//send_queue.push( test );
	//printf( "added to queue\n" );

// Wait for the threads to end (never)
	pthread_join( thread1, NULL );
	pthread_join( thread2, NULL );
	pthread_join( thread3, NULL );

	return 0;
}

// This is our botting thread
void *main_thread( void *arg ) {
// Make sure the server is online by pinging it
	while( !ping_success ) {
		Packet ping( 0x25, 14 );
		send_queue.push( ping );
		SLEEP( 1000 );
	}

// Account details
	unsigned char password[] = "***PASSWORD-GOES-HERE***";
	uint32 account = ***ACCOUNT-NUMBER-HERE***;
// Attempt to login (wait for the 0F response from server)
	while( !login_success ) {
		Packet login( 0x0F, 38 );
		login << account;
		login << (uint32)0;
		login << password;
		login[31] = 0xF2;

		printf( "Sending login\n" );
		send_queue.push( login );

		SLEEP( 2000 );
	}

	Packet enter_word( 0x10, 38 );
	enter_word << player_id;
	enter_word << (uint32)0;
	enter_word << (uint16)0;
	enter_word << password;

	printf( "Sending enter world.\n" );
	send_queue.push( enter_word );

	SLEEP( 2000 );

}

// Our thread to handle sending packets
void *send_thread( void *arg ) {
// Set up misc variables and the xenimus server info
	int s = *(int*) arg;
	int ret;
	struct sockaddr_in si_xen;
	memset((char *) &si_xen, 0, sizeof(si_xen));
	si_xen.sin_family = AF_INET;
	si_xen.sin_port = htons( 5050 );
	if( inet_aton( "64.34.163.8" , &si_xen.sin_addr ) == 0 ) {
		fprintf( stderr, "inet_aton() failed\n" );
		return 0;
	}

	Crypto crypto;

// Make sure we are always running
	while( true ) {
	// Check if there are any packets waiting in the queue
		if( !send_queue.empty() ) {
		// Grab the top packet
			Packet tmp = send_queue.front();

			//printf( "sending packet: " );
			//for( int i = 0; i < tmp.length(); i++ ) {
			//	printf( "%02X ", tmp[ i ] );
			//}
			//printf( "\n" );
		// Encrypt it for sending
			crypto.Encrypt( tmp, tmp.length() );

		// Send the packet
			ret = sendto( s, tmp, tmp.length(), 0, (struct sockaddr*)&si_xen, sizeof( si_xen ));
			if( ret == -1 ) {
				printf( "failed to send last packet\n" );
			}
		// Remove the top packet from queue
			send_queue.pop();
		}
	// Take a quick breather
		SLEEP( 10 );
	}

	return 0;
}

// Our thread to handle incoming packets
void *recv_thread( void *arg ) {
	int s = *(int*) arg;
	struct sockaddr_in si_other;
	socklen_t slen = sizeof( si_other );
	int ret;
	unsigned char buffer[512];

	Crypto crypto;

	while( true ) {
		ret = recvfrom( s, buffer, 512, 0, (struct sockaddr*)&si_other, &slen );
		if( ret == -1 ) {
			printf( "Recvfrom error\n" );
			return 0;
		}
		printf( "Length of packet: %i\n", ret );
		printf( "Received packet from %s:%d\n", inet_ntoa( si_other.sin_addr ), ntohs( si_other.sin_port ));
		crypto.Decrypt( buffer, ret );

		printf( "recveived packet: " );
		for( int i = 0; i < ret; i++ ) {
			printf( "%02X ", buffer[ i ] );
		}
		printf( "\n" );

	// Ping Response
		if( buffer[0] == 0x25 ) {
			ping_success = true;
		}
		else if( buffer[0] == 0x0F ) {
			login_success = true;
		// Get the player id for the first player
			player_id = *(uint16*)&buffer[1];
		}
	// Enter world response
		else if( buffer[0] == 0x1F ) {
			InitialLoginData *ild = (InitialLoginData*)&buffer[1];

			printf( "X/Y: %i, %i | MapID: %i | Player ID: %i\n ", ild->positionX, ild->positionY, ild->mapId, ild->serverId );
		}
		else if( buffer[0] == 0x03 ) {
			handleUpdatePacket( buffer, ret );
		}
	}

	return 0;
}


void handleUpdatePacket(uint8* data, int len)
{

	UpdatePacketSelf update = *(UpdatePacketSelf*)&data[1];
	for( int j = 0; j < len; j++ ) {
		fprintf( out, "%02X ", data[j] );
	}
	fprintf( out, "\n" );

	fprintf( out, "Update positionX: %i\n", update.positionX );
	fprintf( out, "Update positionY: %i\n", update.positionY );
	fprintf( out, "Update spellflags: %i\n", update.spellflags );
	fprintf( out, "Update colorbits: %i\n", update.colorbits );
	fprintf( out, "Update numInRangeDynamicObjects: %i\n", update.numInRangeDynamicObjects );
	fprintf( out, "Update numInRangeUnits: %i\n", update.numInRangeUnits );
	fprintf( out, "Update unklol: %i\n", update.unklol );
	fprintf( out, "Update flags: %i\n", update.flags );
	fprintf( out, "Update rotation: %i\n", update.rotation );
	fprintf( out, "Update animation: %i\n", update.animation );
	fprintf( out, "Update spellEffect: %i\n", update.spellEffect );
	fprintf( out, "Update numInRangeSpellEffects: %i\n", update.numInRangeSpellEffects );
	fprintf( out, "Update unklol2: %i\n", update.unklol2 );
	fprintf( out, "Update unklol3: %i\n", update.unklol3 );
	fprintf( out, "Update currentHPPct: %i\n", update.currentHPPct );
	fprintf( out, "Update currentMPPct: %i\n", update.currentMPPct );
	fprintf( out, "Update currentExpPct: %i\n", update.currentExpPct );
	fprintf( out, "\n\n" );

	int offset = 24;

	if( update.numInRangeDynamicObjects > 0 )
	{
		for( int i = 0; i < update.numInRangeDynamicObjects; i++ )
		{
			if( data[offset] == 0x01 ) offset += 8;
			else if( data[offset] == 0x02 ) offset += 12;
			else if( data[offset] == 0x03 ) offset += 6;
		}
	}

	if( update.numInRangeSpellEffects > 0 )
	{
		for( int x = 0; x < update.numInRangeSpellEffects; x++ )
		{
			if( data[offset] >= 240 ) offset += 12;
			else offset += 8;
		}
	}

	if( update.numInRangeUnits > 0 )
	{
		for( int i = 0; i < update.numInRangeUnits; i++ )
		{
			uint16 id = ( data[offset+1] << 8 ) + data[offset];
			uint16 trueid = id & 0x7FFF;

			offset += 2;

			if( id & 0x8000 )
				continue;
			else
			{
				uint8 updateflag = data[offset];
				offset += 1;

				if(updateflag & 0x01)
				{
					UpdatePacketUnitMovement movement = *(UpdatePacketUnitMovement*)&data[offset];
					printf( "Player: %i | Movement | X/Y: %i, %i\n", trueid, movement.positionX, movement.positionY );
					//UnitMap::UpdateUnitsMovement(trueid, *(UpdatePacketUnitMovement*)&data[offset]); 
					offset += 5;
				}
				if(updateflag & 0x02)
				{
					UpdatePacketUnitAuras auras = *(UpdatePacketUnitAuras*)&data[offset];
					//UnitMap::UpdateUnitsAuras(trueid, *(UpdatePacketUnitAuras*)&data[offset]);
					offset += 3;
				}
				if(updateflag & 0x04)
				{
					UpdatePacketUnitModels models = *(UpdatePacketUnitModels*)&data[offset];
					printf( "Player: %i | ModelInfo | Model: %i, Weapon: %i, Shield: %i, Helmet: %i\n", trueid, models.model, models.weapon, models.shield, models.helmet );
					//UnitMap::UpdateUnitsModel(trueid, *(UpdatePacketUnitModels*)&data[offset]);
					offset += 8;
				}
				if(updateflag & 0x08)
				{
					//UnitMap::UpdateUnitsAnim(trueid, *(UpdatePacketUnitAnim*)&data[offset]);
					offset += 1;
				}
				if(updateflag & 0x10)
				{
					//UnitMap::UpdateUnitsSpellEffect(trueid, *(UpdatePacketUnitSpellEffect*)&data[offset]);
					offset += 1;
				}
				if(updateflag & 0x20)
				{
					offset += 1 + data[offset];
				}
			}
		}
	}
}