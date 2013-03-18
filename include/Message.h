//
//  MailMessage.h
//  MailBlock
//
//  Created by Sylvain Vriens on 18/03/2013.
//
//

#pragma once

#include "Mail.h"
#include "cinder/Cinder.h"
#include "cinder/Utilities.h"
#include "cinder/app/AppBasic.h"
#include "cinder/Base64.h"

#include <boost/asio.hpp>


namespace cinder {
    namespace mail {
        
        using namespace boost::asio::ip;
        
        enum AuthType {
            LOGIN = 1, PLAIN
        };
        
        class Message {
        public:
            
            // Con/de-structor
            Message(
                    const std::string & to = "",
                    const std::string & from = "",
                    const std::string & subject = "",
                    const std::string & message = "",
                    const std::string & server = "127.0.0.1",
                    int32_t port = MAIL_SMTP_PORT,
                    bool mxLookUp = true);
            ~Message();
            
            // Send the message
			bool send(){
                return mObj->send();
            }
            
			// Reset
			void reset(){
                mObj = std::shared_ptr<Obj>(new Obj());
            }
            
			// Get server response
			const std::string getResponse(){
                return mObj->mServerResponse;
            }
            
			// Recipient
			bool addTo(const std::string & to){
                if(to.size()==0) return false;
                mObj->mTo.push_back(to);
                return true;
            }
			bool addTo(const std::vector<std::string> & to){
                for(auto& t : to){
                    if(!addTo(t)) return false;
                }
                return true;
            }
			void clearTo(){
                mObj->mTo.clear();
            }
			bool removeTo(const std::string & to){
                std::vector<std::string>::const_iterator itr = std::find(mObj->mTo.begin(), mObj->mTo.end(), to);
                //not found
                if(itr==mObj->mTo.end()){
                    return false;
                }
                
                mObj->mTo.erase(itr);
                
                return true;
            }
			bool removeTo(const std::vector<std::string> & to){
                for(auto& t : to){
                    if(!removeTo(t)) return false;
                }
                return true;
            }
            
			// Attachments
			bool addAttachment(const std::string & path){ return addAttachment(ci::fs::path(path));}
			bool addAttachments(const std::vector<std::string> & paths){
                for(auto& path: paths){
                    if(!addAttachment(path)){
                        return false;
                    }
                }
                
                return true;
            }
			bool addAttachment(const ci::fs::path & path){
                if(path.empty()) return false;
                if(!exists(path)) return false;
                if(!is_regular_file(path)) return false;
                mObj->mAttachments.push_back(path);
                return true;
            }
			bool addAttachments(const std::vector<ci::fs::path> & paths){
                for(auto& path: paths){
                    if(!addAttachment(path)){
                        return false;
                    }
                }
                
                return true;
            }
			void clearAttachments(){
                mObj->mAttachments.clear();
            }
			bool removeAttachment(const std::string & path){return removeAttachment(ci::fs::path(path));}
			bool removeAttachments(const std::vector<std::string> & paths){
                for(auto& path: paths){
                    if(!removeAttachment(path)){
                        return false;
                    }
                }
                
                return true;
            }
			bool removeAttachment(const ci::fs::path & path){
                std::vector<ci::fs::path>::const_iterator itr = std::find(mObj->mAttachments.begin(), mObj->mAttachments.end(), path);
                //not found
                if(itr==mObj->mAttachments.end()){
                    return false;
                }
                
                mObj->mAttachments.erase(itr);
                
                return true;
            }
			bool removeAttachments(const std::vector<ci::fs::path> & paths){
                for(auto& path: paths){
                    if(!removeAttachment(path)){
                        return false;
                    }
                }
                
                return true;
            }
            
			// Setters
			void setAuthentication(const AuthType & authType = AuthType::PLAIN, const std::string & username = "", const std::string & password = ""){
                mObj->mAuthType = authType;
                mObj->mUsername = username;
                mObj->mPassword = password;
            }
			bool setFrom(const std::string & from){
                if(from.size()==0) return false;
                mObj->mFrom = from;
                return true;
            }
			bool setMessage(const std::string & message, bool html = false){
                if(message.size()==0) return false;
                
                if(html){
                    mObj->mMessageHTML = message;
                }else{
                    mObj->mMessage = message;
                }
                
                return true;
            }
			bool setServer(const std::string & server, unsigned short port = MAIL_SMTP_PORT){
                if(server.size()==0) return false;
                mObj->mServer = server;
                mObj->mPort = port;
                return true;
            }
			bool setSubject(const std::string & subject){
                if(subject.size()==0) return false;
                mObj->mSubject = subject;
                return true;
            }
			bool setTo(const std::string & to){
                mObj->mTo.clear();
                return addTo(to);
            }
            
        protected:
            
            //we only use the OBJ because of better copying
            struct Obj{
                
                Obj() : mSocket(mIOService){
                    
                }
                
                //basic stuff
                std::vector<std::string>    mTo;
                std::string                 mFrom;
                std::string                 mSubject;
                
                //message stuff
                std::string                 mMessage;
                std::string                 mMessageHTML;
                
                //attachement stuff
                std::vector<ci::fs::path>   mAttachments;
                
                //auth stuff
                AuthType                    mAuthType;
                std::string                 mUsername;
                std::string                 mPassword;
                
                //response
                std::string                 mServerResponse;
                
                //server stuff
                std::string                 mServer;
                unsigned short              mPort;
                boost::asio::io_service     mIOService;
                tcp::socket                 mSocket;
                
                
                bool send(){
                    ci::app::console() << buildMessage();
//                    return false;
                    return operator()();
                }
                
                //worker function
                bool operator()(){
                    if(mTo.empty()){
                        mServerResponse = "451 Requested action aborted: local error who am I mailing";
                        return false;
                    }
                    
                    if(mFrom.size()==0){
                        mServerResponse = "451 Requested action aborted: local error who am I";
                        return false;
                    }
                    
                    if(connect()!=220){
                        mServerResponse = "554 Transaction failed: server connect response error.";
                        return false;
                    }
                    
                    
                    /** TODO: check nameserver **/
                    
                    
                    /** lot more checks here before we actualle begin **/
                    
                    
                    //Say hello
                    if(send("HELO cinder.local\r\n")!=250){
                        mServerResponse = "554 Transaction failed: EHLO send/receive";
                        return false;
                    }
                    
                    //we have a user name and password, so autjenticate!
                    //TBI
                    if(mUsername.length() && mPassword.length()){
                        
                    }
                    
                    //set the sender
                    if(send("MAIL FROM:<"+mFrom+">\r\n")!=250){
                        mServerResponse = "554 MAIL FROM sending error";
                        return false;
                    }
                    
                    //set the tos
                    if(send("RCPT TO: <" + mTo.front() + ">\r\n")!=250){
                        mServerResponse = "554 Transaction failed";
                        return false;
                    }
                    
                    //prepare sending data
                    if(send("DATA\r\n")!=354){
                        mServerResponse = "554 DATA error";
                        return false;
                    }
                    
                    
                    if(send(buildMessage())!=250){
                        mServerResponse = "554 DATA, server response error (actual send)";
                        return false;
                    }
                    
                    
                    if(send("QUIT\r\n")!=221){
                        //ignore
                    };
                    mSocket.close();
                    
                    return true;
                }
                
                int connect(){
                    try {
                        //set up the address resolving
                        tcp::resolver resolver(mIOService);
                        tcp::resolver::query query(mServer, ci::toString(mPort));
                        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query), end;
                        
                        //try to connect
                        boost::system::error_code error_code_connect;
                        boost::asio::connect(mSocket, endpoint_iterator, end, error_code_connect);
                        if(error_code_connect){
                            return 0;
                        }
                        
                        
                        return getResponse();
                    }catch(boost::system::system_error){
                        
                    }
                    
                    return 0;
                }
                
                int send(const std::string & data){
                    try {
                        int bytesWritten = mSocket.write_some(boost::asio::buffer(data));

                        if(bytesWritten==0){
                            return 0;
                        }
                        
                        return getResponse();
                    }catch(boost::system::system_error){
                        
                    }
                    return 0;
                }
                
                int getResponse(){
                    try{
                        boost::array<char, 128> buf;
                        int bytesRead = mSocket.read_some(boost::asio::buffer(buf));
                        if(bytesRead==0){
                            return 0;
                        }
                        
                        std::string response(buf.data(),bytesRead);
                        
                        ci::app::console() << "response: " << response << std::endl;
                        
                        return fromString<int>(response.substr(0,3));
                    }catch(...){
                    }
                    
                    return 0;
                }
                
            std::string buildMessage(){
                std::stringstream message;
                
                //From
                message << "From: " << mFrom << "\r\n";
                message << "Reply-To: " << mFrom << "\r\n";
                
                //To
                message << "To: ";
                for(auto& to : mTo){
                    if(&to!=&mTo.front()){
                        message << ",\r\n ";// must add space after comma
                    }
                    message << to;
                }
                message << "\r\n"; //here we remove the extra added ,\r\n , kind of sloppy for now
                //TBI cc && bcc
                
                const std::string boundary("b52e926e95ee553820da4dca6d2304dc");
                bool MIME = (mAttachments.size() || mMessageHTML.size());
                
                if(MIME){
                    message << "MIME-Version: 1.0\r\n";
                    message << "Content-Type: multipart/mixed;\r\n";
                    message << "\tboundary=\"";
                    message << boundary << "\"\r\n";
                }
                
                //add the date
                time_t t;
                time(&t);
                char timestring[128] = "";
                std::string timeformat = "Date: %d %b %y %H:%M:%S %Z\r\n";
                if(strftime(timestring, 127, timeformat.c_str(), localtime(&t))) { // got the date
                    message << timestring;
                }
                
                //add the subject
                message << "Subject: " << mSubject << "\r\n\r\n";//extra empty line marks start of body
                
                
                //we start the body here
                if(MIME){
                    message << "This is a MIME encapsulated message\r\n";
                    message << "--" << boundary << "\r\n";
                    
                    if(!mMessageHTML.size()){ //only plain message
                        
                        message << "Content-type: text/plain; charset=ISO-8859-1\r\n";
                        message << "Content-transfer-encoding: 7BIT\r\n\r\n";
                        
                        message << mMessage;    //TBI : RFC compliance
                        message << "\r\n\r\n--" << boundary << "\r\n";
                    }else{
                        const std::string innerboundary("a0665c5b2505ed854f869a6cde447781");
                        
                        //a multipart massage
                        message << "Content-Type: multipart/alternative;\r\n";
                        message << "\tboundary=\"" << innerboundary << "\"\r\n";
                        
                        //add the inner boundary
                        message << "\r\n\r\n--" << innerboundary << "\r\n";
                        
                        //add the plain message
                        message << "Content-type: text/plain; charset=iso-8859-1\r\n";
                        message << "Content-transfer-encoding: 7BIT\r\n\r\n";
                        message << mMessage;    //TBI : RFC compliance
                        message << "\r\n\r\n--" << innerboundary << "\r\n";
                        
                        //add the html message
                        message << "Content-type: text/html; charset=iso-8859-1\r\n";
                        message << "Content-Transfer-Encoding: BASE64\r\n\r\n";
                        message << ci::toBase64(mMessageHTML);
                        message << "\r\n\r\n--" << innerboundary << "--\r\n";
                        
                    }
                    
                    message << "\r\n--" << boundary;
                    if(!mAttachments.size()){
                        message << "--";
                    }else{//we should add attachments TBI
                        for(auto & path: mAttachments){
                            message << "\r\n";
                        
                            //the default
                            message << "Content-Type: application/X-other-1;\r\n";
                            message << "\tname=" << path.filename() << "\r\n";
                            message << "Content-Transfer-Encoding: base64\r\n";
                            message << "Content-Disposition: attachment; filename=" << path.filename() << "\r\n\r\n";
                            
                            message << ci::toBase64(ci::DataSourcePath::create(path)->getBuffer(), 76);
                            
                            message << "\r\n\r\n--" << boundary;
                        }
                        message << "--";
                    }
                    
                }else{
                    //no mime attachemnets, so just paste the message
                    message << mMessage; //TBI : RFC compliance
                }
                
                //finalize the message
                message << "\r\n.\r\n";
                
                return message.str();
            }
            
                
        
            };
            std::shared_ptr<Obj>            mObj;
        
        };
        
        
        /** IMPLEMENTATION **/
        Message::Message(const std::string & to, const std::string & from, const std::string & subject, const std::string & message, const std::string & server, int32_t port,bool mxLookUp){
            reset();
            setTo(to);
            setFrom(from);
            setSubject(subject);
            setMessage(message);
            setServer(server, port);
            //loopup
        }
        
        Message::~Message(){
        }
        
        
        
    }
}