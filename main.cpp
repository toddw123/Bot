#include "shared.h"

#define SLEEP( x ) usleep( x * 1000 )
string currentDateTime();

uint8 spell_counter = 0;

void *send_thread( void *arg );
void *recv_thread( void *arg );
void *main_thread( void *arg );
void *key_thread( void *arg );
void handleUpdatePacket(uint8* data, int len);
uint16 genMoveChecksum(uint16 id, uint16 x, uint16 y, uint8 type);
void Walk( uint16 p_id, uint16 x, uint16 y );
void Say( uint16 p_id, char *chat, int len );
void clickUnit( uint16 p_id, uint16 target_id, uint16 y );

queue<Packet> send_queue;

// Players account number (filled from settings file)
uint32 xen_account = 0;
// Players password (filled from settings file)
unsigned char xen_pass[25];
// Version of xenimus (required for login packet)
uint8 xen_version = 0;

bool ping_success = false, login_success = false;
uint16 player_id;
FILE *out;
uint16 player_x, player_y;
uint8 player_health = 255;
bool exit_bot = false;

bool loggedin = false;

QuestLog lastQL;

class Unit {
public:
	Unit(){ dead = true; }
	Unit(uint16 uid){ id = uid; dead = true; }
	uint16 id;
	UpdatePacketUnitMovement movement;
	UpdatePacketUnitAuras auras;
	UpdatePacketUnitModels models;
	UpdatePacketUnitAnim anim;
	UpdatePacketUnitSpellEffect spelleffect;
	bool inrange;
	bool dead;
};

unordered_map<uint16, Unit> um_units;

bool finished_quest = false;
bool missing_quest  = false;
bool have_quest     = false;

bool CheckRadius(int px, int py, int mx, int my, int r) {
	if(((px - mx) * (px - mx)) + ((py - my) * (py - my)) <= (r * r))
		return true;
	return false;
}

bool aliveUnitsRadius( int r )
{
	for( auto it = um_units.begin(); it != um_units.end(); ++it )
	{
		if( !it->second.dead && CheckRadius( it->second.movement.positionX, it->second.movement.positionY, player_x, player_y, r )) {
			return true;
		}
	}

	return false;
}

void logout( uint16 p_id ) {
	Packet logout( 0x0C, 14 );
	logout << p_id;
	logout << (uint16)0x00;
	logout << (uint8)0x06;

	send_queue.push( logout );
}

void port(uint16 p_id, uint8 mark) {
	Packet port( 0x08, 14 );
	port << p_id;
	port << (uint8)0;
	port << spell_counter;
	port << (uint16)( 0x0030 + mark );
	port << (uint16)0x0032;
	port << p_id;
	port << player_y;

	send_queue.push( port );

	if( spell_counter == 0xff ) {
		spell_counter = 0;
	} else {
		spell_counter++;
	}
}

void requestLog( uint16 p_id ) {
	Packet qlog( 0x25, 14 );
	qlog << p_id;
	qlog << (uint16)0;
	qlog << (uint16)0x0004;

	send_queue.push( qlog );
}

void createSkelly( uint16 p_id, int x, int y ) {
	Packet skelly( 0x08, 14 );
	skelly << p_id;
	skelly << (uint8)0;
	skelly << spell_counter;
	skelly << (uint16)0x004c;
	skelly << (uint16)0x0023;
	skelly << (uint16)( x * 20 + rand()%11 + 5 );
	skelly << (uint16)( y * 20 + rand()%11 + 5 );

	send_queue.push( skelly );

	if( spell_counter == 0xff ) {
		spell_counter = 0;
	} else {
		spell_counter++;
	}
}

void trans( uint16 p_id, uint16 x, uint16 y ) {
	Packet trans( 0x08, 14 );
	trans << p_id;
	trans << (uint8)0;
	trans << spell_counter;
	trans << (uint16)0x004c;
	trans << (uint16)0x0026;
	trans << x;
	trans << y;

	send_queue.push( trans );

	if( spell_counter == 0xff ) {
		spell_counter = 0;
	} else {
		spell_counter++;
	}
}

void rotFlesh( uint16 p_id ) {
	Packet multi( 0x08, 14 );
	multi << p_id;
	multi << (uint8)0;
	multi << spell_counter;
	multi << (uint16)0x004e;
	multi << (uint16)0x0029;
	multi << p_id;
	multi << player_y;

	send_queue.push( multi );

	if( spell_counter == 0xff ) {
		spell_counter = 0;
	} else {
		spell_counter++;
	}
}

void transXY( uint16 p_id, int x, int y ) {
	uint16 realx = x * 20 + ( rand()%11 + 5 );
	uint16 realy = y * 20 + ( rand()%11 + 5 );

	trans( p_id, realx, realy );
}

void setAllOoR() {
	for( auto& x: um_units ) {
		x.second.inrange = false;
	}
}

void setInRange( uint16 id ) {
	if( um_units.count( id ) == 0 ) {
		// Insert
		Unit tmp( id );
		um_units[ id ] = tmp;

		um_units[ id ].inrange = true;
	} else {
		um_units[ id ].inrange = true;
	}
}

void clearOoR() {
	uint16 keys[100];
	int i = 0;
	for( auto& x: um_units ) {
		if( !x.second.inrange ) {
			keys[i] = x.first;
			i++;
			//um_units.erase( x.first );
		}
	}
	for( int j = 0; j < i; j++ ) {
		um_units.erase( keys[ j ] );
	}
}

int main() {
// Random seed
	srand (time( NULL ));

// Load settings file
	XMLNode xMainNode = XMLNode::openFileHelper( "settings.xml", "Settings" );
// Attempt to get player account number
	XMLNode xNode = xMainNode.getChildNode( "Account" );
	char *acc = (char*)xNode.getText();
	xen_account = atoi( acc );
// Attempt to get player password
	xNode = xMainNode.getChildNode( "Password" );
	unsigned char *pass = (unsigned char*)xNode.getText();
	strcpy( (char*)xen_pass, (char*)pass );
// Attempt to get xen version from setting file
	xNode = xMainNode.getChildNode( "XenVersion" );
	char *ver = (char*)xNode.getText();
	xen_version = atoi( ver );

// Clean up
	delete acc;
	delete pass;
	delete ver;

// Defines
	int sock;
	pthread_t thread1, thread2, thread3, thread4;
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
// Create our keypress handler thread
	if( pthread_create( &thread4, NULL, key_thread, NULL ) < 0 ) {
		perror( "could not create thread\n" );
		return 1;
	}

	//out = fopen( "info.txt", "a+" );

	//Packet test( 0x25, 14 );
	//send_queue.push( test );
	//printf( "added to queue\n" );

// Wait for the threads to end (never)
	pthread_join( thread1, NULL );
	pthread_join( thread2, NULL );
	pthread_join( thread3, NULL );
	pthread_join( thread4, NULL );

	return 0;
}

uint16 locateUnit( uint16 model, uint16 weapon, uint8 shield, uint8 helmet, uint16 colorbits ) {
	for( auto it = um_units.begin(); it != um_units.end(); ++it )
	{
		if( it->second.models.model == model && it->second.models.weapon == weapon && it->second.models.shield == shield && it->second.models.helmet == helmet && it->second.models.colorbits == colorbits )
		{
			return it->first;
		}
	}

	return 0;
}

void dumpUnitMap()
{
	FILE *fout = fopen( "unitmap.txt", "a+" );

	for( auto& x: um_units ) {
		fprintf(  fout, "Player: %i | X,Y: %i,%i | ModelInfo - Model: %i, Weapon: %i, Shield: %i, Helmet: %i, Color: %04X\n", 
			x.second.id, x.second.movement.positionX, x.second.movement.positionY, x.second.models.model, x.second.models.weapon, x.second.models.shield, x.second.models.helmet, x.second.models.colorbits );
	}
	fprintf( fout, "\n" );
	fclose( fout );
}

void login() {
	Packet login( 0x0F, 38 );
	login << xen_account;
	login << (uint32)0;
	login << xen_pass;
	login[31] = xen_version;

	printf( "[%s] Sending login\n", currentDateTime().c_str() );
	send_queue.push( login );
}

void enterWorld( uint16 p_id ) {
	Packet enter_word( 0x10, 38 );
	enter_word << p_id;
	enter_word << (uint32)0;
	enter_word << (uint16)0;
	enter_word << xen_pass;

	printf( "[%s] Sending enter world.\n", currentDateTime().c_str() );
	send_queue.push( enter_word );
}

void doLogin() {
	// Make sure the server is online by pinging it
	while( !ping_success ) {
		Packet ping( 0x25, 14 );
		send_queue.push( ping );
		SLEEP( 1000 );
	}

	while( !login_success ) {
		login();
		SLEEP( 2000 );
	}

	enterWorld( player_id );
	SLEEP( 2000 );
}

// This is our botting thread
void *main_thread( void *arg ) {
	
// NPC details
// ModelInfo - Model: 496, Weapon: 85, Shield: 0, Helmet: 0, Color: 0199
// ModelInfo - Model: 496, Weapon: 85, Shield: 0, Helmet: 0, Color: 0344
	uint16 quest_giver = 0;
	uint16 other_guard = 0;


	int xs[] = { 1343, 1338, 1343, 1348, 1354, 1358, 1354, 1354, 1354, 1358, 1354, 1348, 1343, 1338, 1343, 1343 };
	int ys[] = {  888,  884,  888,  888,  888,  884,  888,  893,  899,  904,  899,  899,  899,  904,  899,  893 };

	int total = 0;

	while( !exit_bot ) {
		// Make sure we are logged in before we continue
		while( !loggedin ) {
			SLEEP( 1000 );
		}

		have_quest = false;

		requestLog( player_id );
		SLEEP( 5000 );

		if( !have_quest && lastQL.curquest != 0x57 ) {
			port( player_id, 2 );

			SLEEP( 2000 );

			transXY( player_id, ( 758 + rand()%2 ), ( 2333 + rand()%2 ));

			SLEEP( 1500 );

			/*while( 1 ) {
				for( auto& x: um_units ) {
					printf(  "Player: %i | X,Y: %i,%i | ModelInfo - Model: %i, Weapon: %i, Shield: %i, Helmet: %i, Color: %04X\n", 
						x.second.id, x.second.movement.positionX, x.second.movement.positionY, x.second.models.model, x.second.models.weapon, x.second.models.shield, x.second.models.helmet, x.second.models.colorbits );
				}
				fprintf( out, "\n" );
				SLEEP( 2000 );
			}*/

			other_guard = 0;
			while( other_guard == 0 ) {
				other_guard = locateUnit( 496, 85, 0, 0, 0x0244 );
				SLEEP( 1000 );
			}

			while( um_units.count( other_guard ) == 0 ) {
				SLEEP( 1000 );
			}

			clickUnit( player_id, other_guard, um_units[ other_guard ].movement.positionY );
			SLEEP( 2000 );

			Say( player_id, (char*)"Pix Animus Task", 15 );
			SLEEP( 2000 );
			Say( player_id, (char*)"OK", 2 );
			SLEEP( 2000 );
			Say( player_id, (char*)"-quitquest", 10 );
			SLEEP( 2000 );

			transXY( player_id, ( 762 + rand()%3 ), 2329 );
			SLEEP( 2000 );

			quest_giver = 0;
			while( quest_giver == 0 ) {
				quest_giver = locateUnit( 496, 85, 0, 0, 0x0099 );
				SLEEP( 1000 );
			}

			while( um_units.count( quest_giver ) == 0 ) {
				SLEEP( 1000 );
			}

			clickUnit( player_id, quest_giver, um_units[ quest_giver ].movement.positionY );
			SLEEP( 2000 );

			Say( player_id, (char*)"warlock task", 12 );
			SLEEP( 2000 );

			Say( player_id, (char*)"OK", 2 );
			SLEEP( 2000 );
		} else {
			port( player_id, 8 );
			SLEEP( 2000 );
		}

		port( player_id, 7 );
		SLEEP( 2000 );
		

		finished_quest = false;
		missing_quest  = false;

		createSkelly( player_id, 1346, 896 );
		SLEEP( 1500 );
		createSkelly( player_id, 1348, 896 );
		SLEEP( 1500 );

		Say( player_id, (char*)"-battle", 7 );
		SLEEP( 1500 );

		int index = 0;
		while( !finished_quest ) {
			if( aliveUnitsRadius( 200 )) {
				rotFlesh( player_id );
				SLEEP( 1000 );
			}
			transXY( player_id, xs[ index ], ys[ index ] );
			SLEEP( 1000 );
			if( !CheckRadius(( xs[ index ] * 20 ), ( ys[ index ] * 20 ), player_x, player_y, 200 )) {
				printf( "[%s] Radius check failed. Restart everything: %i/%i %i/%i\n", currentDateTime().c_str(), ( xs[ index ] * 20 ), ( ys[ index ] * 20 ), player_x, player_y );
				Say( player_id, (char*)"-quitquest", 10 );
				SLEEP( 1000 );
				finished_quest = true;
				missing_quest  = true;
			}
			if( index == 15 ) {
				index = 0;
				if(( rand()%5 ) == 3 ) {
					Say( player_id, (char*)"-battle", 7 );
					SLEEP( 500 );
				}

				requestLog( player_id );
				SLEEP( 500 );
			} else {
				index++;
			}
		}

		if( !missing_quest ) {
			Say( player_id, (char*)"-destroy", 8 );
			SLEEP( 2000 );

			port( player_id, 2 );
			SLEEP( 2000 );

			quest_giver = 0;
			while( quest_giver == 0 ) {
				quest_giver = locateUnit( 496, 85, 0, 0, 0x0099 );
				SLEEP( 1000 );
			}

			while( um_units.count( quest_giver ) == 0 ) {
				SLEEP( 1000 );
			}

			clickUnit( player_id, quest_giver, um_units[ quest_giver ].movement.positionY );
			SLEEP( 2000 );

			port( player_id, 8 );
			SLEEP( 10000 );


			total++;

			printf( "[%s] Quest Done. #%i\n", currentDateTime().c_str(), total );
		}
	}

	//SLEEP( 10000 );

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
		printf( "inet_aton() failed\n" );
		return 0;
	}

	Crypto crypto;

// Make sure we are always running
	while( !exit_bot ) {
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

	while( !exit_bot ) {
		ret = recvfrom( s, buffer, 512, 0, (struct sockaddr*)&si_other, &slen );
		if( ret == -1 ) {
			printf( "Recvfrom error\n" );
			return 0;
		}
		//printf( "Length of packet: %i\n", ret );
		//printf( "Received packet from %s:%d\n", inet_ntoa( si_other.sin_addr ), ntohs( si_other.sin_port ));
		crypto.Decrypt( buffer, ret );

		//printf( "recveived packet: " );
		//for( int i = 0; i < ret; i++ ) {
		//	printf( "%02X ", buffer[ i ] );
		//}
		//printf( "\n" );

		if( buffer[0] == 0x25 ) { // Ping Response
			ping_success = true;
		} else if( buffer[0] == 0x0F ) {
			login_success = true;
		// Get the player id for the first player
			player_id = *(uint16*)&buffer[41]; //1 = first, 41 = second
			printf( "[%s] Player ID: %i\n", currentDateTime().c_str(), player_id );
		} else if( buffer[0] == 0x1F ) { // Enter world response
			InitialLoginData ild = *(InitialLoginData*)&buffer[1];

			printf( "X/Y: %i, %i | MapID: %i | Player ID: %i\n ", ild.positionX, ild.positionY, ild.mapId, ild.serverId );

			loggedin = true;
		} else if( buffer[0] == 0x03 ) { // Update packet
			handleUpdatePacket( buffer, ret );
		} else if( buffer[0] == 0x1e ) { // Quest Log
			QuestLog q = *(QuestLog*)&buffer[1];

			if( q.curkills == q.reqkills || q.curkills == 100 ) {
				finished_quest = true;
			}
			if( q.curquest != 87 ) {
				finished_quest = true;
				missing_quest = true;
			} else {
				have_quest = true;
			}

			lastQL = q;

			printf( "[%s] QuestLog Requested: %i kills of %i\n", currentDateTime().c_str(), q.curkills, q.reqkills );
		} else if( buffer[0] == 0x0C ) { // Logout and misc?
			if( buffer[1] == 0x15 ) { // Logout
				login_success = false;
				loggedin = false;
				finished_quest = true;
				missing_quest = true;

				printf( "[%s] Player has logged out\n", currentDateTime().c_str());
			} else {
				//printf( "[%s] Recived 0x0C Packet Type\n", currentDateTime().c_str());
				//FILE *o = fopen( "in-packets.log", "a+" );
				//for( int i = 0; i < ret; i++ ) {
				//	fprintf( o, "%02X ", buffer[ i ] );
				//}
				//fprintf( o, "\n" );
				//fclose( o );
			}
		}
	}

	return 0;
}

void Walk( uint16 p_id, uint16 x, uint16 y ) {
	Packet move( 0x01, 18 );
	move << p_id;
	move << (uint16)0;
	move << x;
	move << (uint16)0;
	move << y;
	move << (uint16)0;
	move << genMoveChecksum( p_id, x, y, 0x4D );

   send_queue.push( move );
}

void Say(uint16 p_id, char *chat, int len)
{
	Packet msg(0x0A, 70);
	msg << p_id;
	msg << (uint16)0;
	for(int i = 0; i < len; i++)
		msg << (uint8)chat[i];

	send_queue.push( msg );
}

void clickUnit( uint16 p_id, uint16 target_id, uint16 y ) {
	Packet move( 0x01, 18 );
	move << p_id;
	move << (uint16)0;
	move << target_id;
	move << (uint16)0;
	move << y;
	move << (uint16)0;
	move << genMoveChecksum( p_id, target_id, y, 0x41 );

	send_queue.push( move );
}

uint16 genMoveChecksum( uint16 id, uint16 x, uint16 y, uint8 type ) {
	uint16 checksum = 0;
	checksum += id;
	checksum += x;
	checksum += y;
	checksum += type;
	checksum = type | (checksum << 8);

	return checksum;
}


void handleUpdatePacket(uint8* data, int len)
{

	UpdatePacketSelf update = *(UpdatePacketSelf*)&data[1];

	//if( player_x != update.positionX || player_y != update.positionY ) {
		//printf( "X/Y: %i, %i\n", update.positionX, update.positionY );
	//}
	player_x = update.positionX;
	player_y = update.positionY;
	player_health = update.currentHPPct;

	if( player_health <= 25 ) {
		printf( "[%s] Health (%i) < 25. Exiting!\n", currentDateTime().c_str(), player_health );
		logout( player_id );
		SLEEP( 100 );
		exit_bot       = true;
		finished_quest = true;
		missing_quest  = true;
	}

	/*for( int j = 0; j < len; j++ ) {
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
	fprintf( out, "\n\n" );*/

	int offset = 23;

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

	setAllOoR();

	if( update.numInRangeUnits > 0 )
	{
		for( int i = 0; i < update.numInRangeUnits; i++ )
		{
			uint16 id = ( data[offset+1] << 8 ) + data[offset];
			uint16 trueid = id & 0x7FFF;
			
			// Attempt to find unit in our unitmap
			setInRange( trueid );

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
					um_units.at( trueid ).movement = movement;
					//fprintf( out, "Player: %i | Movement | X/Y: %i, %i\n", trueid, movement.positionX, movement.positionY );
					//UnitMap::UpdateUnitsMovement(trueid, *(UpdatePacketUnitMovement*)&data[offset]); 
					offset += 5;
				}
				if(updateflag & 0x02)
				{
					UpdatePacketUnitAuras auras = *(UpdatePacketUnitAuras*)&data[offset];
					um_units.at( trueid ).auras = auras;
					//UnitMap::UpdateUnitsAuras(trueid, *(UpdatePacketUnitAuras*)&data[offset]);
					offset += 3;
				}
				if(updateflag & 0x04)
				{
					UpdatePacketUnitModels models = *(UpdatePacketUnitModels*)&data[offset];
					um_units.at( trueid ).models = models;
					//fprintf( out, "Player: %i | ModelInfo | Model: %i, Weapon: %i, Shield: %i, Helmet: %i\n", trueid, models.model, models.weapon, models.shield, models.helmet );
					//UnitMap::UpdateUnitsModel(trueid, *(UpdatePacketUnitModels*)&data[offset]);
					offset += 8;
				}
				if(updateflag & 0x08)
				{
					UpdatePacketUnitAnim anim = *(UpdatePacketUnitAnim*)&data[offset];
					//UnitMap::UpdateUnitsAnim(trueid, *(UpdatePacketUnitAnim*)&data[offset]);
					um_units.at( trueid ).anim = anim; 

					if( anim.anim == 21 || anim.anim == 45 )
					{
						um_units.at( trueid ).dead = true;
					} else {
						um_units.at( trueid ).dead = false;
					}

					offset += 1;
				}
				if(updateflag & 0x10)
				{
					UpdatePacketUnitSpellEffect spellfx = *(UpdatePacketUnitSpellEffect*)&data[offset];
					//UnitMap::UpdateUnitsSpellEffect(trueid, *(UpdatePacketUnitSpellEffect*)&data[offset]);
					um_units.at( trueid ).spelleffect = spellfx;
					offset += 1;
				}
				if(updateflag & 0x20)
				{
					offset += 1 + data[offset];
				}
			}
		}
	}

	clearOoR();

	if( update.flags & 0x01 ) {
		if( data[offset] == 0x31 && data[offset+1] == 0x30 && data[offset+2] == 0x30 ) {
			finished_quest = true;
		}
	}

	//fprintf( out, "\n\n" );
}

string currentDateTime() {
    time_t rawtime;
    struct tm * ptm;

    time ( &rawtime );
    ptm = gmtime ( &rawtime );

    char buf[8];
    sprintf( buf, "%2d:%02d:%02d", (ptm->tm_hour-7)%24, ptm->tm_min, ptm->tm_sec );
    return buf;
}

void *key_thread( void *arg ) {
	struct termios t;

	tcgetattr(STDIN_FILENO, &t); //get the current terminal I/O structure
	t.c_lflag &= ~ICANON; //Manipulate the flag bits to do what you want it to do
	tcsetattr(STDIN_FILENO, TCSANOW, &t); //Apply the new settings

	char key = ' ';
	while( !exit_bot ) {
		key = getchar();
		switch( key ) {
			case 49: // '1'
				finished_quest = true;
				missing_quest  = true;
				printf( "[%s] Restarting loop as requested\n", currentDateTime().c_str());
				break;
			case 50: // '2'
				printf( "[%s] X/Y: %i,%i - HP: %i - Current Quest: %i - Kills/Req: %i of %i\n", currentDateTime().c_str(), player_x, player_y, player_health, lastQL.curquest, lastQL.curkills, lastQL.reqkills );
				break;
			case 51: // '3'
				requestLog( player_id );
				break;
			case 52: // '4'
				printf( "[%s] Dumping Unit Map to File\n", currentDateTime().c_str());
				dumpUnitMap();
				break;
			case 53: // '5'
				printf( "[%s] Sending logout\n", currentDateTime().c_str());
				finished_quest = true;
				missing_quest  = true;
				logout( player_id );
				break;
			case 54: // '6'
				ping_success = false;
				doLogin();
				break;
			case 55: // '7'
				have_quest = false;
				finished_quest = true;
				missing_quest  = true;
				Say( player_id, (char*)"-quitquest", 10 );
				printf( "[%s] Forcing player to abandon quest and start over\n", currentDateTime().c_str());
				break;
			case 48:
				printf( "[%s] Logging off and exiting\n", currentDateTime().c_str());
				finished_quest = true;
				missing_quest  = true;
				logout( player_id );
				SLEEP( 200 );
				exit_bot = true;
				break;
		}
	}
}
