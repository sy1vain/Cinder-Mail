//
//  Mailer.h
//  MailBlock
//
//  Created by Sylvain Vriens on 19/03/2013.
//
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Thread.h"
#include "cinder/Utilities.h"
#include "cinder/Function.h"


#include "Mail.h"
#include <queue>

#include <boost/asio.hpp>


namespace cinder {
    namespace mail {
        
        using namespace boost::asio;
        
        class Mailer;
        typedef std::shared_ptr<Mailer> MailerRef;
        
        typedef std::shared_ptr<class Message> MessageRef;
        
        typedef signals::signal<void(MessageRef,bool)> SentSignalType;
        
        class Mailer {
        public:
            
            enum LoginType {
                PLAIN,
                LOGIN
            };
            
            static MailerRef create(
                                    const std::string & server,
                                    int32_t port = MAIL_SMTP_PORT,
                                    const std::string & username="",
                                    const std::string & password="",
                                    LoginType type=PLAIN){
                return MailerRef(new Mailer(server, port, username, password, type));
            }
            
            SentSignalType& getSignalSent(){
                return mSignalSent;
            }
            template<typename T, typename Y>
            ci::signals::connection	connectSent( T fn, Y *inst ) { return getSignalSent().connect( std::bind( fn, inst, std::_1, std::_2 ) ); }
            
            void sendMessage(const MessageRef& msg){
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mMessages.push(msg);
                }
                
                run();
            }
            
            void setLogin(const std::string & username,
                          const std::string & password,
                          LoginType type=PLAIN){
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mUsername = username;
                    mPassword = password;
                    mLoginType = type;
                }
            }
            
            void setLogin(bool login=false){
                if(!login){
                    setLogin("","");
                }
            }
            
            ~Mailer(){
                if(mThread){
                    mThread->join();
                }
            }
           
            
        protected:
            
            Mailer(const std::string & server,
                   int32_t port,
                   const std::string & username,
                   const std::string & password,
                   LoginType type) : mThreadRunning(false), mServer(server), mPort(port), mUsername(username), mPassword(password), mLoginType(type){
            }
            
            struct Response {
                Response(std::string response) {
                    try {
                        mCode = fromString<int>(response.substr(0,3));
                        mResponse = response.substr(4);
                    }catch(...){
                        mResponse = response;
                        mCode = 0;
                    }
                };
                
                int getCode(){
                    return mCode;
                }
                
                const std::string& getResponse(){
                    return mResponse;
                }
                
                int mCode;
                std::string mResponse;
            };
            
            struct Responses : public std::vector<Response> {
                
                Responses() : std::vector<Response>(){}
                
                Responses(const std::string& str) : std::vector<Response>(){
                    push_back(Response(str));
                }
                
                Responses(const Response& response) : std::vector<Response>(){
                    push_back(response);
                }
                
                Responses(std::vector<std::string> responses) : std::vector<Response>(){
                    for(auto& response: responses){
                        if(response.size()){//skip empty lines
                            push_back(Response(response));
                        }
                    }
                }
                
                int getCode(){
                    if(empty()){
                        return 0;
                    }
                    return back().getCode();
                }
                
                std::string getResponse(){
                    if(empty()){
                        return "";
                    }
                    return back().getResponse();
                }
                
                operator int (){
                    return getCode();
                }
            };
            
            typedef std::shared_ptr<boost::asio::ip::tcp::socket> SocketRef;
            
            void run(bool threaded = true){
                if(!threaded){
                    threadedFunction();
                    return;
                }
                
                if(mThread){
                    //if there is a thread, join it
                    bool running;
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        running = mThreadRunning;
                    }
                    if(!running){
                        mThread->join();
                        mThread = std::shared_ptr<std::thread>();
                    }
                }
                
                if(!mThread){
                    mThread = std::shared_ptr<std::thread>(new std::thread(&Mailer::threadedFunction, this));
                }
            }
            
            void threadedFunction(){
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mThreadRunning = true;
                }
                
                //the loop
                //breaks on an empty message list
                while(true){
                    MessageRef message;
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        //break the loop if empty
                        if(mMessages.empty()) break;
                        
                        message = mMessages.front();
                        mMessages.pop();
                    }
                    
                    send(message);
                }
                
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mThreadRunning = false;
                }
            }
            
            void success(MessageRef msg){
                signal(msg,true);
            }
            
            void fail(MessageRef msg){
                signal(msg, false);
            }
            
            void signal(MessageRef msg, bool success){
                mSignalSent(msg, success);
            }
            
            //function that actually sends (and negotiates) the message
            void send(const MessageRef& msg){
                Responses reply;
                
                //get the socket by connecting
                SocketRef socket = connect();
                if(!socket){
                    ci::app::console() << "unable to connect" << std::endl;
                    fail(msg);
                    return;
                }
                
                
                //check if the server is indeed ready
                reply = readReply(socket);
                if(reply!=220){//220 is OK
                    disconnect(socket);
                    fail(msg);
                    return;
                }
                
                //authenticate, if set/needed (TBI)
                reply = authenticate(socket);
                if(reply!=250 && reply!=235){ //response should be ok or authentication succeeded
                    disconnect(socket);
                    fail(msg);
                    return;
                }
                
                //we are authenticate, let's initiate a message
                //by sending the headers of a message
                Message::Headers headers = msg->getHeaders();
                for(auto& header: headers){
                    reply = sendData(socket, header);
                    if(reply!=250){
                        disconnect(socket);
                        fail(msg);
                        return;
                    }
                }
                
                //Request the sending of data
                reply = sendData(socket, "DATA");
                if(reply!=354){ //data delimited with .
                    disconnect(socket);
                    fail(msg);
                    return;
                }
                
                reply = sendData(socket, msg->getData(),false);
                if(reply!=250){//OK
                    disconnect(socket);
                    fail(msg);
                    return;
                }
                
                disconnect(socket);
                
                success(msg);
            }
            
            //connects to the smtp server
            SocketRef connect(){
                SocketRef socket;
                
                std::string server;
                int32_t port;
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    server = mServer;
                    port = mPort;
                }
                
                if(!server.size()){
                    //no server set
                    return socket;
                }
                
                //static smtp for now
                boost::asio::ip::tcp::resolver resolver(ios);
                boost::asio::ip::tcp::resolver::query query(server, ci::toString(port));
                
                try {
                    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                    
                    socket = SocketRef(new boost::asio::ip::tcp::socket(ios));
                    boost::asio::connect(*socket, endpoint_iterator);
                }catch(...){
                    return SocketRef();
                }
                
                return socket;
                
            }
            
            //disconnect the socket
            Mailer::Responses disconnect(SocketRef socket){
                
                
                Responses reply = sendData(socket, "QUIT");
                try {
                    if(socket->is_open()){
                        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                        socket->close();
                    }
                }catch(...){
                    
                }
                
                return reply;
            }
            
            Mailer::Responses authenticate(SocketRef socket){
                Responses reply;
                
                //shake hands
                reply = sendData(socket, "EHLO cinder.local");
                if(reply!=250) return reply;
                
                //SSL stuff is done in the connection, no TLS upgrading supported
                
                //we have set a user name and password, so we should login
                if(mUsername.size() && mPassword.size()){
                    //detect the login type, should be found in the reply
                    
                    
                    if(mLoginType==PLAIN){//plain login
                        
                        //we create a identity/password combination
                        std::stringstream login;
                        //                    login << mUsername; //we can leave this out
                        login << '\0';
                        login << mUsername;
                        login << '\0';
                        login << mPassword;
                        
                        reply = sendData(socket, "AUTH PLAIN " + ci::toBase64(login.str()));
                        
                    }else if(mLoginType==LOGIN){//TBI
                        reply = sendData(socket, "AUTH LOGIN");
                        if(reply!=334 && reply.getResponse()!="VXNlcm5hbWU6"){ //it should return 334 and const base64 encoded string
                            return reply;
                        }
                        reply = sendData(socket, ci::toBase64(mUsername));
                        if(reply!=334 && reply.getResponse()!="UGFzc3dvcmQ6"){ //it should return 334 and const base64 encoded string
                            return reply;
                        }
                        
                        reply = sendData(socket, ci::toBase64(mPassword));
                    }
                    
                    
                }
                
                return reply;
            }
            
            //sends the data to the server and returns the responses
            Mailer::Responses sendData(SocketRef socket, const std::string& data, bool appendNL=true){
                
                try {
                    int bytesWritten = 0;
                    if(!appendNL){
                        bytesWritten = socket->write_some(boost::asio::buffer(data));
                    }else{
                        bytesWritten = socket->write_some(boost::asio::buffer(data+MAIL_SMTP_NEWLINE));
                    }
                    
                    if(bytesWritten==0){
                        return Responses();
                    }
                    
                    
                    return readReply(socket);
                }catch(...){
                    //error
                }
                
                return Responses();
            }
            
            //gets the respjnses from the server
            Mailer::Responses readReply(SocketRef socket){
                
                try{
                    boost::array<char, 8192> buf; //very large buff just in case the server blabs a lot
                    
                    int bytesRead = socket->read_some(boost::asio::buffer(buf));
                    if(bytesRead==0){
                        return Responses();
                    }
                    
                    std::string response(buf.data(),bytesRead);
                    
                    return Responses(ci::split(response, MAIL_SMTP_NEWLINE));
                }catch(...){
                    
                }
                
                return Responses();
            }
            
            //thread
            std::mutex                      mDataMutex;
            bool                            mThreadRunning;
            
            std::shared_ptr<std::thread>    mThread;
            std::queue<MessageRef>         mMessages;
            
            //server settings
            std::string mServer;
            int32_t mPort;
            std::string mUsername;
            std::string mPassword;
            LoginType mLoginType;
            bool mSSL;
            
            //notifications
            SentSignalType  mSignalSent;
            
            io_service ios;
            
        };
        
        
    }
}