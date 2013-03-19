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


#include "Mail.h"
#include "Message.h"
#include <queue>

#include <boost/asio.hpp>


namespace cinder {
    namespace mail {
        
        using namespace boost::asio;
        
        class Mailer;
        typedef std::shared_ptr<Mailer> MailerPtr;
        
        class Mailer {
        public:
            
            static MailerPtr create(
                                    const std::string & server,
                                    int32_t port = MAIL_SMTP_PORT,
                                    const std::string & username="",
                                    const std::string & password=""
                                    ){
                return MailerPtr(new Mailer(server, port, username, password));
            }
            
            void sendMessage(const MessagePtr& msg){
                {
                    std::lock_guard<std::mutex> lock(mObj->mDataMutex);
                    mObj->mMessages.push(msg);
                }
                
                mObj->run();
            }
            
        protected:
            
            struct Obj {
                
                typedef std::shared_ptr<ip::tcp::socket> SocketPtr;
                
                Obj(const std::string & server,
                    int32_t port,
                    const std::string & username,
                    const std::string & password) : mThreadRunning(false), mServer(server), mPort(port), mUsername(username), mPassword(password)
                {
                }
                
                ~Obj(){
                    if(mThread){
                        mThread->join();
                    }
                }
                
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
                        mThread = std::shared_ptr<std::thread>(new std::thread(&Obj::threadedFunction, this));
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
                        MessagePtr message;
                        {
                            std::lock_guard<std::mutex> lock(mDataMutex);
                            //break the loop if empty
                            if(mMessages.empty()) break;
                            
                            message = mMessages.front();
                            mMessages.pop();
                        }
                        
                        sendMessage(message);
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        mThreadRunning = false;
                    }
                }
                
                //implemented lower to save some space here
                void sendMessage(const MessagePtr& msg);
                SocketPtr connect();
                int disconnect(SocketPtr socket);
                int authenticate(SocketPtr socket);
                int sendData(SocketPtr socket, const std::string& str, bool appendNL=true);
                int readReply(SocketPtr socket);
                
                std::mutex                      mDataMutex;
                bool                            mThreadRunning;
                
                std::shared_ptr<std::thread>    mThread;
                std::queue<MessagePtr>         mMessages;
                
                //server settings
                std::string mServer;
                int32_t mPort;
                std::string mUsername;
                std::string mPassword;
                
                io_service ios;
                
                
            };
            std::shared_ptr<Obj> mObj;
            
            Mailer(const std::string & server,
                   int32_t port,
                   const std::string & username,
                   const std::string & password
                   ){
                mObj = std::shared_ptr<Obj>(new Obj(server, port, username, password));
            }
            
        };
        
        void Mailer::Obj::sendMessage(const MessagePtr& msg){
            int reply;
            
            //get the socket by connecting
            SocketPtr socket = connect();
            if(!socket){
                ci::app::console() << "unable to connect" << std::endl;
                //FAIL
                return;
            }
            
            
            //check if the server is indeed ready
            reply = readReply(socket);
            if(reply!=220){//220 is OK
                disconnect(socket);
                //FAIL
                return;
            }
            
            //authenticate, if set/needed (TBI)
            reply = authenticate(socket);
            if(reply!=250){
                disconnect(socket);
                //FAIL
                return;
            }
            
            //we are authenticate, let's initiate a message
            //by sending the headers of a message
            Message::Headers headers = msg->getHeaders();
            for(auto& header: headers){
                reply = sendData(socket, header);
                if(reply!=250){
                    disconnect(socket);
                    //FAIL
                    return;
                }
            }
            
            //Request the sending of data
            reply = sendData(socket, "DATA");
            if(reply!=354){ //data delimited with .
                disconnect(socket);
                //FAIL
                return;
            }
            
            reply = sendData(socket, msg->getData(),false);
            if(reply!=250){//OK
                disconnect(socket);
                //FAIL
                return;
            }
            
            
            ci::app::console() << "succes! disconnecting" << std::endl;
    
            disconnect(socket);
            
            ci::app::console() << "done" << std::endl;
    
        }
        
        Mailer::Obj::SocketPtr Mailer::Obj::connect(){
            SocketPtr socket;
            
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
            ip::tcp::resolver resolver(ios);
            ip::tcp::resolver::query query(server, ci::toString(port));
            
            try {
                ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
                
                socket = SocketPtr(new ip::tcp::socket(ios));
                
                boost::asio::connect(*socket, endpoint_iterator);
            }catch(...){
                return SocketPtr();
            }
            
            return socket;
            
        }
        
        int Mailer::Obj::disconnect(SocketPtr socket){
            
            
            int reply = sendData(socket, "QUIT");
            try {
                if(socket->is_open()){
                    socket->shutdown(ip::tcp::socket::shutdown_both);
                    socket->close();
                }
            }catch(...){
                
            }
            
            return reply;;
        }
        
        int Mailer::Obj::authenticate(SocketPtr socket){
            int reply;
            
            //shake hands
            reply = sendData(socket, "HELO cinder.local");
            if(reply!=250) return reply;
            
            //here we should also initiate SSL (if we ever support it)
            
            //we should authenticate here TBI
            
            return reply;
        }
        
        int Mailer::Obj::sendData(SocketPtr socket, const std::string& data, bool appendNL){
            
            try {
                int bytesWritten = 0;
                if(!appendNL){
                    bytesWritten = socket->write_some(boost::asio::buffer(data));
                }else{
                    bytesWritten = socket->write_some(boost::asio::buffer(data+MAIL_SMTP_NEWLINE));
                }
            
                if(bytesWritten==0){
                    return 0;
                }
                
                
                return readReply(socket);
            }catch(...){
                //error
            }
            
            return 0;
        }
        
        int Mailer::Obj::readReply(SocketPtr socket){
            
            try{
                boost::array<char, 128> buf;
                int bytesRead = socket->read_some(boost::asio::buffer(buf));
                if(bytesRead==0){
                    return 0;
                }
                
                std::string response(buf.data(),bytesRead);
                
                ci::app::console() << response;
                
                return fromString<int>(response.substr(0,3));
            }catch(...){
                
            }
            
            return 0;
        }
        
    }
}