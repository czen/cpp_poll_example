#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <pthread.h>
#include <map>
#include <list>

using namespace std;

void handleError(string msg)
{
  cerr << msg << " error code " << errno << " (" << strerror(errno) << ")\n";
  exit(1);
}
vector<string> split(const string text, const string delims)
{
    vector<string> tokens;
    size_t start = text.find_first_not_of(delims), end = 0;
    while((end = text.find_first_of(delims, start)) != string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(delims, end);
    }
    if(start != string::npos) tokens.push_back(text.substr(start));
    return tokens;
}
string toString(int a)
{
  stringstream ss;
  ss << a;
  return ss.str();
}

int main(int argc, char* argv[])
{
  int port = 28563, concurrentClientCount = 0;
  struct sockaddr_in serverAddress, clientAddress;
  int listenSocket;  // РІРїСѓСЃРєР°СЋС‰РёР№ СЃРѕРєРµС‚
  vector<pollfd> allSockets; //С…СЂР°РЅРёР»РёС‰Рµ РІСЃРµС… СЃРѕРєРµС‚РѕРІ (РІРїСѓСЃРєР°СЋС‰РёР№ + РєР»РёРµРЅС‚СЃРєРёРµ) РґР»СЏ С„СѓРЅРєС†РёРё poll
  map<int,string> dataForProcessing; //РЅРµРїРѕР»РЅС‹Рµ Р·Р°РїСЂРѕСЃС‹ РѕС‚ РєР»РёРµРЅС‚РѕРІ (РїСЂРѕРґРѕР»Р¶РµРЅРёРµ РєРѕС‚РѕСЂС‹С… РµС‰Рµ РЅРµ РґРѕСЃС‚Р°РІРёР»РѕСЃСЊ РїРѕ СЃРµС‚Рё)

  //СЃРѕР·РґР°РµРј РќР•Р‘Р›РћРљРР РЈР®Р©РР™ СЃРѕРєРµС‚ РґР»СЏ РїСЂРёРµРјР° СЃРѕРµРґРёРЅРµРЅРёР№ РІСЃРµС… РєР»РёРµРЅС‚РѕРІ
  listenSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

  //РґРѕР±Р°РІР»СЏРµРј РµРіРѕ РІ РѕР±С‰РёР№ РјР°СЃСЃРёРІ РґР»СЏ С„СѓРЅРєС†РёРё poll
  pollfd tmpPollfd; tmpPollfd.fd = listenSocket; tmpPollfd.events = POLLIN;
  allSockets.push_back(tmpPollfd);

  //СЂР°Р·СЂРµС€Р°РµРј РїРѕРІС‚РѕСЂРЅРѕ РёСЃРїРѕР»СЊР·РѕРІР°С‚СЊ С‚РѕС‚ Р¶Рµ РїРѕСЂС‚ СЃРµСЂРІРµСЂСѓ РїРѕСЃР»Рµ РїРµСЂРµР·Р°РїСѓСЃРєР° (РЅСѓР¶РЅРѕ, РµСЃР»Рё СЃРµСЂРІРµСЂ СѓРїР°Р» РІРѕ РІСЂРµРјСЏ РѕС‚РєСЂС‹С‚РѕРіРѕ СЃРѕРµРґРёРЅРµРЅРёСЏ)
  int turnOn = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &turnOn, sizeof(turnOn)) == -1)
    handleError("setsockopt failed:");

  // Setup the TCP listening socket
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = inet_addr("0.0.0.0");
  serverAddress.sin_port = htons(port);

  if (bind( listenSocket, (sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
    handleError("bind failed:");

  if (listen(listenSocket, 1000) == -1) handleError("listen failed:");

  string recvBuffer(10000, '=');  //Р±СѓС„РµСЂ РїСЂРёРµРјР°
  //С€Р°Р±Р»РѕРЅ РѕС‚РІРµС‚Р°
  string answer = "HTTP/1.1 200 OK\r\nServer: super fast mexmat server\r\nContent-Type: text/html\r\nContent-Length: 1\r\n\r\nA";
  while (true)
  {
    list<int> disconnectedClients;
    //Р¶РґРµРј РїРѕРґРєР»СЋС‡РµРЅРёСЏ РЅРѕРІРѕРіРѕ РєР»РёРµРЅС‚Р° РёР»Рё РїСЂРёС…РѕРґР° Р·Р°РїСЂРѕСЃР° РѕС‚ РєР°РєРѕРіРѕ-РЅРёР±СѓРґСЊ РёР· СѓР¶Рµ РїРѕРґРєР»СЋС‡РµРЅРЅС‹С… РєР»РёРµРЅС‚РѕРІ
    if ( poll(&allSockets[0], allSockets.size(), -1) < 0 ) handleError("poll failed:");

    //РІС‹СЏСЃРЅСЏРµРј РЅР° РєР°РєРѕР№ СЃРѕРµРєС‚ РїСЂРёС€Р»Рё РґР°РЅРЅС‹Рµ
    for (int i = 0; i < allSockets.size(); ++i)
    {
      if (allSockets[i].revents == 0) continue;
      if (allSockets[i].revents != POLLIN)
      {
        cerr << "Error: wrong event type occured, when we waiting only for POLLIN\n";
        exit(1);
      }
      if (i == 0)  //РїСЂРѕРёР·РѕС€Р»Рѕ РїРѕРґРєР»СЋС‡РµРЅРёРµ РЅРѕРІРѕРіРѕ РєР»РёРµРЅС‚Р°
      {
        int clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket < 0)
        {
          if (errno != EWOULDBLOCK) handleError("accept failed: ");
          else //РІ СЃРїРµС†РёС„РёС‡РµСЃРєРёС… СЃР»СѓС‡Р°СЏС… Р±С‹РІР°РµС‚, С‡С‚Рѕ accept РЅРµ РїСЂРёРЅРёРјР°РµС‚ РєР»РёРµРЅС‚Р° РїРѕСЃР»Рµ СЃСЂР°Р±Р°С‚С‹РІР°РЅРёСЏ poll
            continue;
        }
        //РґРѕР±Р°РІР»СЏРµРј РЅРѕРІРѕРіРѕ РєР»РёРµРЅС‚Р°
        tmpPollfd.fd = clientSocket;
        allSockets.push_back(tmpPollfd);
        cout << ++concurrentClientCount << " concurrent clients are connected\n";
      }
      else //РїСЂРѕРёР·РѕС€РµР» РїСЂРёРµРј РґР°РЅРЅС‹С… РѕС‚ РєР»РёРµРЅС‚Р°
      {
        int clientSocket = allSockets[i].fd;
        int readBytesCount = 1, err;
        //РїРѕР»СѓС‡Р°РµРј РґР°РЅРЅС‹Рµ
        err = recv(clientSocket, &recvBuffer[0], 10000, 0);
        if (err < 0)
        {
          if (errno != EWOULDBLOCK) handleError("recv failed:");
          else //РґР°РЅРЅС‹Рµ РЅРµ РїРѕР»СѓС‡РµРЅС‹ (С‚Р°РєРѕРµ Р±С‹РІР°РµС‚, РЅРѕ РѕС‡РµРЅСЊ СЂРµРґРєРѕ)
            continue;
        }
        if (err == 0) //РєР»РёРµРЅС‚ СЂР°Р·РѕСЂРІР°Р» СЃРѕРµРґРёРЅРµРЅРёРµ
        {
          disconnectedClients.push_back(i);
          continue;
        }
        //РѕР±СЂР°Р±Р°С‚С‹РІР°РµРј РІСЃРµ РїРѕСЃС‚СѓРїРёРІС€РёРµ РѕС‚ РєР»РёРµРЅС‚Р° Р·Р°РїСЂРѕСЃС‹
        string data = recvBuffer.substr(0,err);
        cout << "Data received: " << data << "\n";
        string queries = dataForProcessing[clientSocket] + data;
        int border1 = 0, border2 = queries.find("\r\n");
        while (border2 != string::npos)
        {
          string query = queries.substr(border1, border2-border1);
          vector<string> words = split(query, " ");
          string answer = words[1] == "+" ? toString(atoi(words[2].c_str()) + atoi(words[3].c_str())) : "ERR Unknown operator";
          cout << "Answer: " << answer << "\n";
          answer += "\r\n";
          send( clientSocket,  &answer[0], answer.size(), 0 );
          border1 = border2+2;
          border2 = queries.find("\r\n", border1);
        }
        //РєР»Р°РґРµРј РѕСЃС‚Р°С‚РѕРє РІ dataForProcessing[clientSocket]
        dataForProcessing[clientSocket] = queries.substr(border1);
      }
    }
    //СѓРґР°Р»СЏРµРј РѕС‚РєР»СЋС‡РёРІС€РёС…СЃСЏ РєР»РёРµРЅС‚РѕРІ РёР· РІСЃРµС… СЃС‚СЂСѓРєС‚СѓСЂ
    for (list<int>::reverse_iterator it = disconnectedClients.rbegin(); it != disconnectedClients.rend(); ++it)
    {
      dataForProcessing.erase(allSockets[*it].fd);
      allSockets.erase(*it+allSockets.begin());
      concurrentClientCount--;
    }
  }
}
