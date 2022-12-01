#include <scoop_server.h>
#include <timemgr.h>

#define __version__		"0.1.2"

//=================================================
 ScoopClientInterface::ScoopClientInterface()
 :broker(NULL),verbose(SC_SILENT),rxBuf("")
 {

 }
void ScoopClientInterface::subscribe(const char *topic)
{
	topics.subscribe(topic);
}

//================================================
ScoopRemoteClient::ScoopRemoteClient()
{
}
void ScoopRemoteClient::write(const char *str)
{
	client.write(str);
}
bool ScoopRemoteClient::isConnected() // Returns true (local client is always connected)
{
	return client.connected() ;
}
void ScoopRemoteClient::receive()
{
	while(client.available()){
	  char c = client.read();
	  switch(c){
		  case '\n':
			if(broker) broker->parse(this,rxBuf);
			rxBuf = "" ;
			break ;
		 /*  case '\r':// Voir si on filtre
		  	break ;
		  case '\x1E': // Substitute to \n in payload
			rxBuf += '\n' ;
			break ;
			* */
		  default:
			rxBuf += c;// Accumulate in buffer
			break;
		}
	}
}
//============================================================================================
ScoopLocalClient::ScoopLocalClient()
:startTime(millis()/1000)		// Epoch de démarrage du client (en secondes)
,rate(1000)		// refresh rate (for external loop)
{
}
bool 	ScoopLocalClient::isConnected()  // Returns true if connected on broker (should be "const" but arduino WiFiClient has no const....)
{
	return true ;
}
void ScoopLocalClient::write(const char *str)
{
	txBuf = str ;
	parse(txBuf);
}
void	ScoopLocalClient::publish(const char *topic,const char *payload,Retain retain)
{
	rxBuf = retain ;
	rxBuf += topic ;
	if(payload){
		rxBuf += " " ;
		rxBuf += payload ;
	}
	rxBuf += "\n" ;
	broker->parse(this,rxBuf); // Emulates a packet reception
}

void	ScoopLocalClient::publish(const char *topic,bool payload,Retain retain)
{
	char str[132] ;
	snprintf(str,sizeof(str),"%c%s %d\n",retain,topic,payload?1:0);
	rxBuf = str ;
	broker->parse(this,rxBuf);
}

void	ScoopLocalClient::publish(const char *topic,int payload,Retain retain)
{
	char str[132] ;
	snprintf(str,sizeof(str),"%c%s %d\n",retain,topic,payload?1:0);
	rxBuf = str ;
	broker->parse(this,rxBuf);
}

void ScoopLocalClient::publish(const char *topic,double payload,int decimals,Retain retain)
{
	char str[132] ;
	snprintf(str,sizeof(str),"%c%s %f\n",retain,topic,payload);//TODO => add optionnal decimals
	rxBuf = str ;
	broker->parse(this,rxBuf);
}

void ScoopLocalClient::subscribe(const char *topic,void ( *callback)(const char *topic,const char *payload))
{
	topics.subscribe(topic,callback);
}

void ScoopLocalClient::subscribe(const char *topic,void ( *callback)(const char *topic,long value))
{
	topics.subscribe(topic,callback);
}

void ScoopLocalClient::subscribe(const char *topic,void ( *callback)(const char *topic,double value))
{
	topics.subscribe(topic,callback);
}
// System command
bool ScoopLocalClient::systemCommand(char *topic,char *payload)
{
	if(verbose) Serial.printf("scoop system command: \"$%s\"\n",topic);
	if(!strcmp(topic,"now") && payload){
		// When I write this library, we use 32 bits microcontroler... so we have to manage the epoch
		// This code is provided up to January 19, 2038, at 3:14 a.m. UT
		// I will be 83 years old at that time and this robots will be in operation for 17 years...
		// If so, you will have to manage the problem, just add a hook to manage last years and change the epoch origin
		// for instance if you edit this file in 2035
		// if (epoch < 2051222400) => Change epoch root to January 19, 2038, at 3:14 a.m. UT
		// else ==> keep epoch root to January 1, 1970, at 0:0 a.m. UT
		//payload[strlen(payload)-3] = 0 ;// Drop ms to get a 32 bits compatible integer
		startTime = strtoul(payload,NULL,10) ;// Do not forget the unsigned!
		setTime(startTime);// Adjust internal timers
		bTimeReceived = true ;
		if(verbose) {
			Serial.print("Start time: ");
			Serial.print(startTime);
			Serial.print(" (");
			Serial.print(datetag());
			Serial.println(")");
		}
	} else if(!strcmp(topic,"version") && payload){
		publish(topic,__version__);
	} else if(!strcmp(topic,"broker") && payload){
		brokerName = payload ;
		if(verbose) Serial.printf("connected to broker %s\n",payload);
	} else if(!strcmp(topic,"reset")){
		publish(topic,"goodbye");
		if(verbose)  Serial.printf("Reset command Cause: %s\n",payload ? payload : "no explanation");
		delay(1000);// Let time to print
		ESP_RESET;
	} else if(!strcmp(topic,"log") && payload){
		if(verbose)  {
			Serial.printf("== %s == ",topic);
			Serial.println(payload);
		}
		publish(topic,"done");
	} else if(!strcmp(topic,"rate")){
		time_t x = strtoul(payload,NULL,10) ;
		if(x>50 && x<5000) rate = x ;

		/*
    } else if(!strcmp(topic,"ls")){
		publish(topic,listDir("/").c_str());
    } else if(!strcmp(topic,"options")){
		publish(topic,options.list().c_str());
    } else if(!strcmp(topic,"option") && payload){
		publish(topic,options.get(payload).c_str());
    } else if(!strcmp(topic,"on") && payload){
		publish(topic,__version__);// TODO store published "on" payload
    } else if(!strcmp(topic,"read") && payload){
		String str  = fileRead(payload);
		publish(topic,str.length() ? str.c_str() : "" );
    } else if(!strcmp(topic,"write") && payload){
		const char *path = strtok(payload,"\t ");
		const char *value = strtok(NULL,"");
		if(value) fileWrite(path,value);
		publish(topic,path);
		* */
	} else {
		if(verbose) Serial.printf("command not found: \"$%s\" payload:%s\n",topic,payload?payload:"no payload");
		return false ;
	}
	return true ;
}
void	ScoopLocalClient::parse(String &line) // scoop protocol parse
{
	char tag ;
	char *topic,*payload = NULL;
	char * raw = (char *) line.c_str();// Récupère la ligne en vue d'un traitement
	char * ptr ;// ptr pour strtok_r
	if(line.length() <1) return ;// empty lines rejected
	tag = raw[0] ;
	topic = strtok_r(raw+1," \n",&ptr);
	if(topic) payload = strtok_r(NULL,"\n",&ptr);
	switch(tag){
		default:
			topic-- ;// We go back to first character because tag has been omitted
		case '$':
			Serial.println("command received");// DEBUG
			systemCommand(topic,payload) ;
			break ;
		case '&':
		case ' ':
			topics.dispatch(topic,payload);
			break ;
	}

}
//====================================================================
ScoopBroker::ScoopBroker(ScoopClient &bridge,ScoopLocalClient &localClient,int port)
:RtosTask("broker",10)
,port(port),server(port)
,bridge(bridge),localClient(localClient)
{
	int i ;
	localClient.broker = this ;// Create a link to local client
	// Clients table initialisation including local client at end
	for(i = 0;i<MAX_SCOOP_CLIENTS;i++) clients[i] = remoteClient + i ;
	clients[i] = &localClient ;
}
void ScoopBroker::parse(ScoopClientInterface *pClient,String &line)
{
	char *buf = (char *) line.c_str() ;// Buffer for parsing
	char *ptr;// utility pointer, storage for strtok_r etc...
	char tag = buf[0] ;
	String topic ;
	topic.reserve(MAX_TOPIC_LENGTH);
	if(strchr(SCOOP_TAGS,tag)){// Here we have a tag
		buf++ ;// We skip the tag
	} else {  // Here the tag has been omitted
		tag = ' ';// Take the space as tag, do not skip tag
	}
	// The challenge is to get the topic without modifing received frame
	ptr = strchr(buf,' ');// separator for payload?
	if(!ptr) topic = buf ;// No payload
	else strncpy((char *)topic.c_str(),buf,ptr-buf);
	Serial.printf("Topic extract: %s, payload: %s\n",topic,ptr ? ptr+1 : "no-payload");
	switch(tag){
		case '$':	// Client answer (bidirectionnal initiated by broker)
			break ;
		case '&':	// Client command (bidirectionnal initiated by client)
			break ;
		default:  // Dispatchable topic (normal, retain ...)
			// we will look for every active client if he has susbscribe to this tag
			for(int i=0;i<MAX_SCOOP_CLIENTS;i++){
				ScoopClientInterface *pOther = clients[i] ;
				if(pOther->topics.match(topic.c_str())) pOther->write(buf);
			}
			break ;
	}

}
// note that this method should be "const" but as in arduino
// "isConnected()" is not declared "const" it is not possible to do so
int ScoopBroker::nbClients()
{
	int result = 0 ;
	for(int i=0;i<MAX_SCOOP_CLIENTS+1;i++){
		if(clients[i]->isConnected())  result++ ;
	}
	return result ;
}
void ScoopBroker::setup()
{
}
void ScoopBroker::loop()
{
	if(server.hasClient()){ // A connexion occurs
		// find a free client and connect
		bool full = true ;// Indicates that client array is full
		for(int i=0;i<MAX_SCOOP_CLIENTS;i++){ // Note that we do not tet last one wich is the local client
			ScoopRemoteClient *pClient = remoteClient + i ;
			if(pClient->isConnected())  continue ;
			pClient->client = server.available() ;
			pClient->verbose = verbose ;
			pClient->broker = this ;
			if(verbose){
				Serial.print("Connection accepted from ");
				Serial.print(pClient->client.remoteIP());
				Serial.println(" ...[ok]");
			}
			full = false ;// A place has been found
			break ;
		}
		if(full){// The connexion requesst is rejected, the client array is full
			IPAddress addr =  server.available().remoteIP() ;
			server.available().stop();
			if(verbose){
				Serial.print("connexion rejected from ");
				Serial.print(addr);
				Serial.println(" more than ");
				Serial.print(MAX_SCOOP_CLIENTS) ;
				Serial.println(" clients connected, ");
			}
		}
	}
	// CLients receive loop
	for(int i=0;i<MAX_SCOOP_CLIENTS;i++){
		ScoopRemoteClient *pClient = remoteClient + i ;
		if(pClient->isConnected() && pClient->client.available())  pClient->receive() ;
	}
}
