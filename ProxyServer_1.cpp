// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //
//                                                                                           //
//                      Proxy Server by Yogitha Mahadasu                                     //
//     Handles:                                                                              //
//  1. Works as a basic Proxy for any client request                                         //
//  2. Caching has been implemented - The proxy caches the pages to a specified time out     //
//  3. Proxy Server also does the Link Pre-fetching whenever a web page is requested         //
//                                                                                           //
// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //

#include "md5.h"
#include<iostream>
#include<sstream>
#include<fstream>
#include<string>
#include<cstring>
#include<cstdlib>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>
#include<pthread.h>
#include<tr1/unordered_map>
#include<ctime>
#include<errno.h>
#include<vector>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<dirent.h>
#include<sys/stat.h>
#include<stdio.h>
#include <stdlib.h>
#include <algorithm>
using namespace std;

#define	QLEN		  30
#define TIMEOUT      "120"
void *handleRequest(void *);

int main(int argNum, char* argV[])
{
    if (argNum != 2)
        {
            cerr << "Incorrect number of arguments. Please provide an input of the format: ./<filename> <port number on which you want the proxy server to run>" << endl;
            return -1;
        }
    int proxyServerPort = atoi(argV[1]);

    //creating a logical entity - a socket
    int proxyServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (proxyServerSocket < 0)
        {
            perror("Socket creation failed");
            exit(-1);
        }

    //setting the properties of server
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(proxyServerPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    //binding the socket with the server properties
    if((bind(proxyServerSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)))<0)
    {
        perror("Socket binding failed");
        exit(-1);
    }

    //setting the socket to listen
    if(listen(proxyServerSocket, QLEN)<0)
    {
        perror("Listening on the socket failed");
        exit(-1);
    }

    cout<<"Proxy Server is running on "<<proxyServerPort<<endl<<endl;

    //Accepting connections
    while(true)
    {
        struct sockaddr_in clientAddress;
        unsigned int length = sizeof(clientAddress);
        int browserClientSocket = accept(proxyServerSocket, (struct sockaddr *)&clientAddress, &length);

        if(browserClientSocket < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_t threadID;
        //creating thread to handle the client connection
        if(pthread_create(&threadID, NULL, handleRequest, (void*) &browserClientSocket) < 0)
        {
            perror("Thread creation failed");
            close(browserClientSocket);
            pthread_exit(NULL);

        }
        pthread_join(threadID , NULL);
    }
    return 0;
}

string getRequestFromBrowserClient(int browserClientSocket)
{
    char buffer[2000];
    char* bufferPointer = buffer;
    memset(bufferPointer, '\0', 2000);
    int sizeRead, totalMessageLength;
    stringstream inputStream;

    while (true)
        {
         if((sizeRead=recv(browserClientSocket,(void *)bufferPointer,2000,0))<0)
         {
            perror("Reading from Browser Client Socket failed");
            close(browserClientSocket);
            pthread_exit(NULL);
         }

         totalMessageLength = totalMessageLength + sizeRead;
         for(int i = 0; i < sizeRead; i ++)
         {
            inputStream << bufferPointer[i];
         }

         //checking condition to exit the loop - when we encounter a \r \n \r \n
         if(totalMessageLength > 4)
         {
            string temp = inputStream.str();
            if (temp[temp.length()-4] == '\r' && temp[temp.length()-3] == '\n' && temp[temp.length()-2] == '\r' && temp[temp.length()-1] == '\n')
                {
                    break;
                }
         }
        }

    //now inputStream will only be populated with the first line of the request received from the browser
    string requestHeader = inputStream.str();
    if (requestHeader.find("Connection: keep-alive") != string::npos)
        requestHeader.replace(requestHeader.find("Connection: keep-alive"), 24, "");

    //cout<< "Content sent from Browser to proxy "<<requestHeader<<endl;
    return requestHeader;
}

//Images were not loading as host was not being found certain times and the thread was exiting
//This idea has been taken from location: stack over flow

string sendDummyData() {

  stringstream sendstream;
  sendstream << "HTTP/1.0 408 Request Timeout\r\n";
  sendstream << "Content-Type: text/html; charset=UTF-8\r\n";
  sendstream << "\r\n";
  sendstream << "<html>";
  sendstream << "<meta name=viewport content=\"initial-scale=1, minimum-scale=1, width=device-width\">\n";
  sendstream << "<title>Error 404 (Elements Missing)!!</title>\n";
  sendstream << "<p>Elements have not loaded correctly.</p> <p> Host name not found.</p>";
  sendstream << "</html>";

  return sendstream.str();
}

string filenotFound() {

  stringstream sendstream;
  sendstream << "HTTP/1.0 404 File not found\r\n";
  sendstream << "Content-Type: text/html; charset=UTF-8\r\n";
  sendstream << "\r\n";
  sendstream << "<html>";
  sendstream << "<meta name=viewport content=\"initial-scale=1, minimum-scale=1, width=device-width\">\n";
  sendstream << "<title>Error 404 (File Not Found Error)!!</title>\n";
  sendstream << "<p>The requested file is not found.</p> <p> Host name not found.</p>";
  sendstream << "</html>";

  return sendstream.str();
}

string invalidRequest() {
 stringstream sendstream;
  sendstream << "HTTP/1.0 500 Request Timeout\r\n";
  sendstream << "Content-Type: text/html; charset=UTF-8\r\n";
  sendstream << "\r\n";
  sendstream << "<html>";
  sendstream << "<meta name=viewport content=\"initial-scale=1, minimum-scale=1, width=device-width\">\n";
  sendstream << "<title>Error 500 (Internal Server Error)!!</title>\n";
  sendstream << "<p>Only the GET requests are served</p> <p> Host name not found.</p>";
  sendstream << "</html>";

  return sendstream.str();

}
string parseforHostname(int soc)
{
    string hostname = "localhost";
    struct sockaddr_in sa;
    socklen_t sa_len;

    sa_len = sizeof sa;
    if(getpeername(soc,(struct sockaddr *)&sa,&sa_len) == -1)
    {
     cout<<"Host name is not retrieved "<<endl;
     return hostname;
    }
    hostname = inet_ntoa(sa.sin_addr);
    return hostname;
}

int parseforPortNumber(int soc)
{
    int portNumber = 80;

    struct sockaddr_in sa;
    socklen_t len_inet;

    len_inet = sizeof sa;
    if(getpeername(soc, (struct sockaddr *)&sa, &len_inet)  == -1)
    {
     cout<<"Port number is not retrieved "<<endl;
     return portNumber;
    }
    portNumber = (int) ntohs(sa.sin_port);

    return portNumber;
}

string getResponseFromWebServer(string requestHeaders, string hostname, int portNumber)
{
    //we need to create a socket to establish and communicate with the back-end Web server
    struct hostent *webserver;
    int proxyClientSocket;

    cout<<"Request headers in get response method "<< requestHeaders<<endl;
    portNumber = 80;

    if(requestHeaders.find("Host: ") != string::npos)
    {
        hostname = requestHeaders.substr(requestHeaders.find("Host: ") + 6,requestHeaders.find("\n", requestHeaders.find("Host: ")) -requestHeaders.find("Host: ")-7);
    }
    hostname = hostname.substr(0,hostname.find(":"));
    cout<<"Host name is "<<hostname<<endl;

    if((webserver = gethostbyname(hostname.c_str()))==NULL)
    {
        cerr << "Host does not exist here" << endl;
        return sendDummyData();
    }

    //now that we have hostname and port number we can establish a connection to the back-end web server
    struct sockaddr_in webserverAddr;
    bzero((char *) &webserverAddr, sizeof(webserverAddr));
    webserverAddr.sin_family = AF_INET;
    webserverAddr.sin_port = htons(portNumber);
    bcopy((char *) webserver -> h_addr, (char *) &webserverAddr.sin_addr.s_addr, webserver -> h_length);

    proxyClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(connect(proxyClientSocket, (struct sockaddr *) &webserverAddr, sizeof(webserverAddr))<0)
    {
        cerr << "Connection to the Back-End Web server failed" << endl;
        return sendDummyData();
    }

    //now that we have a connection established to the back end web server, we are ready to send the request headers
    int totalRequestLength = requestHeaders.length();
    int sizeSent = 0;
    char requestToSend[totalRequestLength];

    memcpy(requestToSend, requestHeaders.c_str(), totalRequestLength);

    while(sizeSent!=totalRequestLength)
    {
        sizeSent = send(proxyClientSocket,(void*) requestToSend, totalRequestLength, 0);
    }

    //request has been sent to the web server, now we need to receive the response from the back-end web server
    char buffer[2000];
    char* bufferPointer = buffer;
    memset(bufferPointer, '\0', 2000);
    int sizeRead, totalMessageLength;
    stringstream inputStream;

    cout<<"Reading from the Back-end Web server socket "<<endl<<endl;
    while(true)
        {
            sizeRead=recv(proxyClientSocket,(void *)bufferPointer,2000,0);
            if(sizeRead < 0)
            {
                perror("Reading from Web Server Socket failed");
                close(proxyClientSocket);
                pthread_exit(NULL);
            }

            if(sizeRead == 0)
                break;

         totalMessageLength = totalMessageLength + sizeRead;
         for(int i = 0; i < sizeRead; i ++)
         {
            inputStream << bufferPointer[i];
         }

         //checking condition to exit the loop - when we encounter a \r \n \r \n
         if(totalMessageLength > 4)
         {
            string temp = inputStream.str();
            if (temp[temp.length()-4] == '\r' && temp[temp.length()-3] == '\n' && temp[temp.length()-2] == '\r' && temp[temp.length()-1] == '\n')
                {
                    break;
                }
         }
        }

    string responseContent = inputStream.str();
    //cout<< "Content sent from Backend Web server is "<<responseContent<<endl;

    close(proxyClientSocket);
    return responseContent;
}

bool sendResponseFromWebServertoClientBrowser(int browserClientSocket, string responseContent)
{
    int totalResponseLength = responseContent.length();
    int sizeSent = 0;
    char responseBuffer[totalResponseLength];

    memcpy(responseBuffer, responseContent.c_str(), totalResponseLength);

    //cout<<"response length is "<<totalResponseLength<<endl;

    while(sizeSent!=totalResponseLength)
    {
        sizeSent = send(browserClientSocket,(void*) responseBuffer, totalResponseLength, 0);
        //cout<<"still in this loop"<<endl;
    }
    return true;
}


int getFilesfromDirectory(string dir, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if(string(dirp->d_name).find(".txt")!=std::string::npos)
        files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

void getFileName(string filestartswith, vector<string> &files_relavant)
{
    string dir = "Cache/";
    vector<string> files = vector<string>();
    getFilesfromDirectory(dir,files);
     for (int i = 0;i < files.size();i++)
            {
                if(files[i].find(filestartswith)!=std::string::npos)
                    files_relavant.push_back(files[i]);
            }
}

bool checkforValidity(string filename)
{
    double seconds;
    time_t now;
    time(&now);

    double created_at = atoi(filename.substr(filename.find("_")+1, filename.find_last_of("_",filename.length()) - filename.find("_")-1).c_str());
    double timeout = atoi(filename.substr(filename.find_last_of("_",filename.length())+1,filename.find(".txt")-filename.find_last_of("_",filename.length())-1).c_str());
    //cout<<"time now is "<<now<<endl;
    seconds = difftime(now,created_at);
    if(timeout < seconds)
    {
        return false;
    }
    return true;
}

string checkCache(string requestURL)
{
    string content = "";
    string md5hashValue = md5(requestURL);
    vector<string> files_relavant = vector<string>();
    getFileName(md5hashValue,files_relavant);
    int numberofFiles = files_relavant.size();

    bool fileExists = false;
    if(numberofFiles > 0)
        fileExists = true;
    for(int k = 0; k<numberofFiles; k ++)
    {
     string filename = files_relavant[k];
     //cout<<"filename "<<filename<<endl;
    filename = "Cache/"+filename;

    bool valid = checkforValidity(filename);

    //cout<<"valid "<<valid<<endl;
       if(valid)
       {
           streampos size;
           char * memblock;
           ifstream file (filename.c_str(), ios::binary);
           ostringstream output;
           if (file.is_open())
            {
                cout<<"Retrieved the file from cache :- "<<filename<<endl;
                output << file.rdbuf();
                content = output.str();
           }
           else
            cout << "Unable to open file";
        }
      else
        {
            if(fileExists)
            {
                if(remove(filename.c_str()) != 0 )
                perror( "Error deleting file" );
                else
                cout<<"File successfully deleted as cache time out has expired"<<endl;
            }
        }
    //cout<<"content from cache is "<<content<<endl;
    }
    return content;
}

void storeResponseinCache(string responseContent, string requestURL)
{
    //cout<<"Writing the response from server to cache "<<responseContent<<endl;

    string md5hashValue = md5(requestURL);
    //cout<<"Request url is "<<requestURL<<" Md5 value is "<<md5hashValue<<endl;

    cout<<"The filename in Cache is"<<md5hashValue<<endl<<endl;
    time_t now;
    time(&now);

    string time_string;
    stringstream temp;
    temp << now;
    time_string = temp.str();

    string filename = "Cache/"+md5hashValue+"_"+time_string+"_"+TIMEOUT+".txt";
    ofstream backupFile;
    backupFile.open(filename.c_str(),ios::out | ios::binary);
    backupFile << responseContent;
    backupFile.close();
}

int getCountOfLinks(string responseContent)
{
    int numberOfLinks = 0;
    string::size_type start = 0;

    while ((start = responseContent.find("href", start)) != string::npos)
    {
    numberOfLinks++;
    start += sizeof("href")-1;
    }
    return numberOfLinks;
}

string stripanyQuotes(string s)
{
   s.erase(remove( s.begin(), s.end(), '\"' ), s.end());
    return s;
}
void populateLinks(string responseContent, vector<string> &links, string requestURL)
{
    string::size_type start = 0;
    requestURL = requestURL.substr(0,requestURL.find_last_of("/"));

    while((start = responseContent.find("href=",start))!=string::npos)
    {
        string link = responseContent.substr(responseContent.find("href=",start)+5,responseContent.find(">",responseContent.find("href=",start))-responseContent.find("href=",start)-5);

        link = stripanyQuotes(link);

        if(link.find("http")==string::npos)
        {
            link = requestURL+"/"+link;
        }
        //cout<<"Link is "<<link<<endl;
        links.push_back(link);
        start +=sizeof("href=")-1;
    }
}
string getRequestURL(string requestHeaders)
{
    string requestURL = "";
    requestURL = requestHeaders.substr(requestHeaders.find("http:"),requestHeaders.find("HTTP")-5);
    return requestURL;
}

string constructRequestHeaders(string link, string requestHeaders)
{
    string replacedRequestHeaders = "";
    string part1 = requestHeaders.substr(requestHeaders.find("GET "),requestHeaders.find("http:/"));
    string part2 = requestHeaders.substr(requestHeaders.find(" HTTP/"));
    replacedRequestHeaders = part1 + link + part2;
    return replacedRequestHeaders;
}

void prefetchtheLinks(string link,string requestHeaders,int soc)
{
    //cout<<"Link is "<<link<<endl;
    string responseContent;

    requestHeaders = constructRequestHeaders(link,requestHeaders);
    //cout<<"Reconstructed Request headers are "<<requestHeaders<<endl;
    string hostname = parseforHostname(soc);
    int portNumber = parseforPortNumber(soc);
    responseContent = getResponseFromWebServer(requestHeaders,hostname,portNumber);

    cout<<"Prefetch completed, storing the file "<<link<<"into the cache"<<endl<<endl;
    //cout<<"Response is "<<responseContent<<endl;
    storeResponseinCache(responseContent,link);
}
void *handleRequest(void *clientSocket)
{
    int browserClientSocket = *(int*)clientSocket;

    //we are detaching the thread in order to free the resources held by this thread on termination
    pthread_detach(pthread_self());

    string requestHeaders = getRequestFromBrowserClient(browserClientSocket);
    string requestURL = getRequestURL(requestHeaders);
    cout<<"1: Request Headers "<<requestHeaders<<endl;

    string responseContent ="";

    responseContent = checkCache(requestURL);

    if(responseContent == "")
    {
        cout<<"Not found in Cache, request is sent to the back-end web server "<<endl;
        cout<<"The request sent to backend web server is {"<<requestURL<<"}"<<endl;

        string hostname = parseforHostname(browserClientSocket);
        int portNumber = parseforPortNumber(browserClientSocket);

        cout<<"host name is "<< hostname<<endl;
        cout<<"Port number is "<<portNumber<<endl;

        responseContent = getResponseFromWebServer(requestHeaders,hostname,portNumber);

        if(requestHeaders.find("GET")==string::npos)
        {
            responseContent = invalidRequest();
        }
        if(responseContent.find("404")!=string::npos)
        {
            responseContent = filenotFound();
        }

        cout<<"Received the response from Back-end Webserver for the url {"<<requestURL<<"} now storing it into the cache "<<endl;
        storeResponseinCache(responseContent,requestURL);
        int numberofLinks = 0;

        numberofLinks = getCountOfLinks(responseContent);

        if(numberofLinks!= 0)
        {
            cout<<"Pre-fetching the links on the web page requested"<<endl;
            vector <string> links = vector<string>();
            populateLinks(responseContent,links, requestURL);
            for(int i=0;i<numberofLinks;i++)
            {
                prefetchtheLinks(links[i],requestHeaders,browserClientSocket);
            }
        }
    }
    //cout<<"2: Response Content "<<responseContent<<endl;

    if(!sendResponseFromWebServertoClientBrowser(browserClientSocket,responseContent))
    {
        cerr << "Error in sending the back-end webserver's response to the client browser"<<endl;
    }
    //closing the socket and exiting the thread after processing is complete
    close(browserClientSocket);
    pthread_exit(NULL);
}
