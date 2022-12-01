#ifndef	SCOOP_SERVER_H
#define	SCOOP_SERVER_H
#include <arduino.h>
#include <WiFi.h>
#include <scoop.h>
#include <rtostask.h>

#define	MAX_SCOOP_CLIENTS 20
#define	MAX_TOPIC_LENGTH 80 // Longueur maximum d'un topic

#ifndef ESP32
	#error "This library is written for ESP32 only"
#endif

typedef  bool (*TopicCallback)(const char *topic,const char *payload)  ;

// Pure Virtual class for Common broker interface for local and remote client
//-----------------------
//===============================================
class ScoopClientInterface {
	friend class ScoopBroker ;
public:
					ScoopClientInterface();
	void 			subscribe(const char *topic);
	virtual bool 	isConnected() = 0 ;// Returns true if connected on broker (should be "const" but arduino WiFiClient has no const....)
	virtual void 	write(const char *buf) = 0 ;// Data to proceed from broker
protected:
	class ScoopBroker *broker;// Backlink to broker
	ScoopTopic 		topics	;// Topics subscribed
	String			rxBuf ;
	TaskVerbosity 	verbose	;// Verbosity level
};
// Local broker client
//-----------------------
//===============================================
class ScoopLocalClient : public ScoopClientInterface {
public:
	ScoopLocalClient() ;
	virtual void write(const char *buf);// Data to proceed from broker
	virtual bool isConnected() 		;// Returns true (local client is always connected)
	time_t 	getRate() const{return rate;} ;// Result in ms
	time_t 	getStartTime() const {return startTime;} ;// Result in ms
	time_t 	missionTime() const {return (millis()-startTime)/1000;};// Result in s
	void	publish(const char *topic,const char *payload=NULL,Retain retain=SC_NO_RETAIN);
	void	publish(const char *topic,bool payload,Retain retain=SC_NO_RETAIN);
	void	publish(const char *topic,int payload,Retain retain=SC_NO_RETAIN);
	void	publish(const char *topic,double payload,int decimals=2,Retain retain=SC_NO_RETAIN);
	void	subscribe(const char *topic,void ( *callback)(const char *topic,const char *payload));
	void	subscribe(const char *topic,void ( *callback)(const char *topic,long value));
	void	subscribe(const char *topic,void ( *callback)(const char *topic,double value));
public: 		// Variables accessible to user
protected:
	String		brokerName;
	time_t		startTime	;// Epoch de démarrage du client (en secondes)
	time_t		rate		;// refresh rate (for external loop)
	ScoopTopic	topics 		;// Generic topics
	bool		bTimeReceived	;// true : local time has been set by broker and is valid
	String 		txBuf		;// Pending of rxBuf for broker to client data (avoid critical courses on rxBuf by separating the two streams)
private:
	bool 	systemCommand(char *topic,char *payload);// system command ($ prefix) execution false => unknown
	void	parse(String &line);// scoop protocol parse
};
//---------------------
// Proxy of clients in broker
class ScoopRemoteClient : public ScoopClientInterface {
public:
	ScoopRemoteClient();
	virtual void write(const char *buf);// Data to proceed from broker
	virtual bool isConnected() ;// Returns true (local client is always connected)
public:
	void			receive();// Process received data
	WiFiClient 		client ;// Associated wifi client (TOSEE if protected and access function?)
protected:
};

////---------------------
class ScoopBroker : public RtosTask{
	friend class ScoopLocalClient ;
public:
	ScoopBroker(class ScoopClient &bridge,ScoopLocalClient &client,int port=SCOOP_PORT);
	int  	nbClients()  ;
	void	parse(ScoopClientInterface *client,String &line) ;// To be called to parse and dispatch received data
	ScoopLocalClient & 	getLocalClient() {return localClient ;}	;// On a un seul local client
protected:
	void 			setup() ;// see if a client try to connect
	void 			loop() 	;// see if a client try to connect
protected:
	WiFiServer				server 	;// Embeeded  wifi server
	int						port	;// Server port
	ScoopClient				&bridge	;// Bridge to upper level scoop broker
	ScoopLocalClient		&localClient	;// On a un seul local client
	ScoopClientInterface	*clients[MAX_SCOOP_CLIENTS+1] ;// All my clients (local + remote)
	ScoopRemoteClient 		remoteClient[MAX_SCOOP_CLIENTS] ;// Server clients
};

#endif //	SCOOP_SERVER_H
