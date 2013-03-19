//
//  MailParts.h
//  MailBlock
//
//  Created by Sylvain Vriens on 18/03/2013.
//
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"
#include "cinder/Text.h"
#include "cinder/Base64.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace cinder {
    namespace mail {
        
        //these two are public for reference
        class Message;
        typedef std::shared_ptr<Message> MessagePtr;
        
        
        class Message {
        public:
            enum recipient_type {
                TO,CC
            };
            
            typedef std::vector<std::string> Headers;
            
        protected:
            
            class Address {
            public:
                Address(){}
                Address(const std::string &address, const std::string &name=""){
                    mName = name;
                    mAddress = address;
                }
                bool isValid() const{
                    if(mAddress.size()==0) return false;
                    return true;
                }
                std::string getAddress() const{
                    return mAddress;
                }
                std::string getName() const{
                    return mName;
                }
                std::string getFullAddress(bool includeName=true) const{
                    std::stringstream ret;
                    if(includeName && mName.size()){
                        ret << "mNAme" << " ";
                    }
                    ret << "<" << mAddress << ">";
                    return ret.str();
                }
            protected:
                std::string mAddress;
                std::string mName;
            };
            
        public:
            
            class Attachment;
            typedef std::shared_ptr<Attachment> AttachmentPtr;
            
            //creator function
            static MessagePtr create(){
                return MessagePtr(new Message());
            }
            
            void addAttachment(const ci::DataSourceRef& dataSource, bool embed=false){
                addAttachment(Attachment::create(dataSource, embed));
            }
            
            void addAttachment(const ci::fs::path &path, bool embed=false){
                addAttachment(Attachment::create(path, embed));
            }
            
            void addAttachment(const std::string& path, bool embed=false){
                addAttachment(Attachment::create(path, embed));
            }
            
            void addInlineAttachment(const ci::DataSourceRef& dataSource){
                addAttachment(Attachment::create(dataSource, true));
            }
            
            void addInlineAttachment(const ci::fs::path &path){
                addAttachment(Attachment::create(path, true));
            }
            
            void addInlineAttachment(const std::string& path){
                addAttachment(Attachment::create(path, true));
            }
            
            void addAttachment(const AttachmentPtr& attachment){
                if(attachment->isEmbedded()){
                    mContent->addAttachment(attachment);
                }else{
                    mAttachments.push_back(attachment);
                }
            }
            
            void setSender(const std::string& address, const std::string& name=""){
                mFrom = Address(address, name);
            }
            
            void addRecipient(const std::string& address, const std::string& name="", recipient_type type=TO){
                Address a(address, name);
                if(!a.isValid()) return;
                
                if(type==TO){
                    mTo.push_back(a);
                }else if(type==CC){
                    mCC.push_back(a);
                }
            }
            
            void addRecipient(const std::string& address, recipient_type type){
                addRecipient(address, "", type);
            }
            
            void setSubject(const std::string& subject){
                mSubject = subject;
            }
            
            void setMessage(const std::string& msg){
                mContent->setMessage(msg);
            }
            
            void setHTML(const std::string& html){
                mContent->setHTML(html);
            }
            
            Headers getHeaders();
            std::string getData() const;
            
            
        protected:
            Message(){
                mContent = Content::create();
            }
            
            bool isMultiPart() const{
                return !mAttachments.empty() || mContent->isMultiPart();
            }
            
            //some define from helpe classes later on
            class Content;
            typedef std::shared_ptr<Content> ContentPtr;
            
            
            ContentPtr                  mContent;
            std::vector<AttachmentPtr>  mAttachments;
            
            Address                     mFrom;
            Address                     mReplyTo;
            std::vector<Address>        mTo;
            std::vector<Address>        mCC;
            std::string                 mSubject;
            
            const std::string mBoundary = "=585ac769fba6306f9982300a8af93da7=";
            
            //****************//
            // HELPER CLASSES //
            //****************//
        protected:
            class Text;
            class HTML;
            typedef std::shared_ptr<Text> TextPtr;
            typedef std::shared_ptr<HTML> HTMLPtr;
            
            class MailPart {
                virtual std::string getData() const{return "";}
            };
            
            class Text : public MailPart {
            public:
                
                static TextPtr create(const std::string& content=""){
                    return TextPtr(new Text(content));
                }
                
                virtual Headers getHeaders() const;
                virtual std::string getData() const;
                
                void setContent(const std::string& content){
                    mContent = content;
                }
            protected:
                Text(const std::string& content=""){
                    mContent = content;
                }
                
                std::string mContent;
            };
            
            class HTML : public Text {
            public:
                
                static HTMLPtr create(const std::string& content=""){
                    return HTMLPtr(new HTML(content));
                }
                
                void addAttachment(const AttachmentPtr& attachment){
                    mAttachments.push_back(attachment);
                }
                
                bool isMultiPart() const{
                    return !mAttachments.empty();
                }
                
                virtual Headers getHeaders() const;
                std::string getData() const;
                
            protected:
                HTML(const std::string& content=""){
                    mContent = content;
                }
                
                std::vector<AttachmentPtr> mAttachments;
                
                const std::string mBoundary = "=bd91c9aaf15895bc2251fedfa9d433b6=";
            };
            
            class Content : public MailPart {
            public:
                static ContentPtr create(){
                    return ContentPtr(new Content());
                }
                
                bool isMultiPart() const{
                    if(mHTML){
                        return true;
                    }
                    return false;
                }
                
                Headers getHeaders() const;
                std::string getData() const;
                
                void addAttachment(const AttachmentPtr& attachment){
                    if(!mHTML){
                        mHTML = HTML::create();
                    }
                    mHTML->addAttachment(attachment);
                }
                
                void setMessage(const std::string& msg){
                    mText->setContent(msg);
                }
                
                void setHTML(const std::string& html){
                    if(!mHTML){
                        mHTML = HTML::create();
                    }
                    mHTML->setContent(html);
                }
            protected:
                Content(){
                    mText = Text::create("");
                }
                
                TextPtr mText;
                HTMLPtr mHTML;
                
                const std::string mBoundary = "=3ebb25d3e279a83ea483bb8daa1f5588=";
            };
        public:
            class Attachment : public MailPart {
            public:
                
                static AttachmentPtr create(const ci::DataSourceRef& datasource, bool embed=false){
                    return AttachmentPtr(new Attachment(datasource, embed));
                }
                static AttachmentPtr create(const ci::fs::path &path, bool embed=false){
                    return AttachmentPtr(new Attachment(ci::DataSourcePath::create(path), embed));
                }
                static AttachmentPtr create(const std::string &path, bool embed=false){
                    return create(ci::fs::path(path), embed);
                }
                
                bool isEmbedded(){
                    return mEmbedded;
                }
                
                std::string getCID() const{
                    return mDataSource->getFilePath().filename().string();
                }
                
                Headers getHeaders() const;
                std::string getData() const;
                
            protected:
                Attachment(const ci::DataSourceRef& datasource, bool embed=false){
                    mDataSource = datasource;
                    mEmbedded = embed;
                }
                
                ci::DataSourceRef mDataSource;
                bool mEmbedded;
            };
            
        protected:
            
            
        };
        
        
        typedef Message::AttachmentPtr AttachmentPtr;
        
        
        
        //****************************//
        // getData implementations //
        //****************************//
        Message::Headers Message::getHeaders(){
            Message::Headers headers;
            
            //check complete here first
            
            headers.push_back("MAIL FROM:" + mFrom.getFullAddress(false));
            if(mTo.size()){//to prevent an error here, if it is empty the server will most likely reject it
                headers.push_back("RCPT TO:" + mTo.front().getFullAddress(false));
            }
            
            return headers;
        }
        
        std::string Message::getData() const {
            std::stringstream data;
            
            
            //check complete here first
            
            //sender
            data << "From: " << mFrom.getFullAddress() << MAIL_SMTP_NEWLINE;
            data << "Reply-to: ";
            if(mReplyTo.isValid()){
                data << mReplyTo.getAddress() << MAIL_SMTP_NEWLINE;
            }else{
                data << mFrom.getAddress() << MAIL_SMTP_NEWLINE;
            }
            
            //the TO
            std::vector<Address>::const_iterator itr;
            for(itr = mTo.begin(); itr<mTo.end(); ++itr){
                if(itr==mTo.begin()){
                    data << "To: ";
                }
                data << (*itr).getFullAddress();
                
                if((itr+1)<mTo.end()){
                    data << "," << MAIL_SMTP_NEWLINE << " "; //we add a space because we need to (duh!)
                }else{
                    data << MAIL_SMTP_NEWLINE;
                }
            }
            
            //the CC
            for(itr = mCC.begin(); itr<mCC.end(); ++itr){//reusing the itr form the to
                if(itr==mCC.begin()){
                    data << "Cc: ";
                }
                data << (*itr).getFullAddress();
                
                if((itr+1)<mCC.end()){
                    data << "," << MAIL_SMTP_NEWLINE << " "; //we add a space because we need to (duh!)
                }else{
                    data << MAIL_SMTP_NEWLINE;
                }
            }
            
            //add a multipart header in case we have a multipart message
            if(isMultiPart()){
                data << "MIME-Version: 1.0" << MAIL_SMTP_NEWLINE << "Content-Type: multipart/mixed;" << MAIL_SMTP_NEWLINE << "\tboundary=\"" << mBoundary << "\"" << MAIL_SMTP_NEWLINE;
            }
            
            //add the current local data
            time_t t;
            time(&t);
            char timestring[128] = "";
            std::string timeformat = "Date: %d %b %y %H:%M:%S %Z";
            if(strftime(timestring, 127, timeformat.c_str(), localtime(&t))) { // got the date
                data << timestring << MAIL_SMTP_NEWLINE;
            }
            
            //add the subject line
            data << "Subject: " << mSubject << MAIL_SMTP_NEWLINE << MAIL_SMTP_NEWLINE;
            
            if(isMultiPart()){
                data << "This is a MIME encapsulated message" << MAIL_SMTP_NEWLINE;
                data << "--" << mBoundary << MAIL_SMTP_NEWLINE;
                
                
                //the content part
                Headers headers = mContent->getHeaders();
                for(auto& header: headers){
                    data << header << MAIL_SMTP_NEWLINE;
                }
                data << mContent->getData() << MAIL_SMTP_NEWLINE;
                
                //the attachemnets
                for(auto& attachment: mAttachments){
                    data << MAIL_SMTP_NEWLINE << "--" << mBoundary << MAIL_SMTP_NEWLINE;
                    
                    Headers headers = attachment->getHeaders();
                    for(auto& header: headers){
                        data << header << MAIL_SMTP_NEWLINE;
                    }
                    
                    data << MAIL_SMTP_NEWLINE << attachment->getData() << MAIL_SMTP_NEWLINE;
                }

                data << MAIL_SMTP_NEWLINE << "--" << mBoundary << "--" << MAIL_SMTP_NEWLINE;
                
            }else{
                //it has not alternative parts or attachents, so just the data
                data << mContent->getData();
            }
            
            //terminate the message
            data << MAIL_SMTP_NEWLINE << "." << MAIL_SMTP_NEWLINE;
            
            return data.str();
        }
        
        Message::Headers Message::Content::getHeaders() const {
            Message::Headers headers;
            if(!isMultiPart()){ //no parts so anempty header
                return headers;
            }
            headers.push_back("Content-Type: multipart/alternative;");
            headers.push_back("\tboundary=\"" + mBoundary + "\"");
            
            return headers;
        }
        
        std::string Message::Content::getData() const{
            if(!isMultiPart()){
                return mText->getData();
            }
            
            std::stringstream data;
            
            data << MAIL_SMTP_NEWLINE << "--" << mBoundary << MAIL_SMTP_NEWLINE;
            
            Headers headers = mText->getHeaders();
            for(auto& header: headers){
                data << header << MAIL_SMTP_NEWLINE;
            }
            data << MAIL_SMTP_NEWLINE <<mText->getData() << MAIL_SMTP_NEWLINE;
            
            data << MAIL_SMTP_NEWLINE<< "--" << mBoundary << MAIL_SMTP_NEWLINE;
            
            headers = mHTML->getHeaders();
            for(auto& header: headers){
                data << header << MAIL_SMTP_NEWLINE;
            }
            data << MAIL_SMTP_NEWLINE <<mHTML->getData() << MAIL_SMTP_NEWLINE;
            
            data << MAIL_SMTP_NEWLINE << "--" << mBoundary << "--" << MAIL_SMTP_NEWLINE;
            
            return data.str();
        }
        
        
        Message::Headers Message::Text::getHeaders() const {
            Message::Headers headers;
            
            headers.push_back("Content-type: text/plain; charset=ISO-8859-1");
            headers.push_back("Content-transfer-encoding: 7BIT");
            
            return headers;
        }
        
        std::string Message::Text::getData() const{
            return mContent;
        }
        
        Message::Headers Message::HTML::getHeaders() const {
            Message::Headers headers;
            
            if(isMultiPart()){
                headers.push_back("Content-type: multipart/related;");
                headers.push_back("\tboundary=\"" + mBoundary + "\"");
                headers.push_back("");
                headers.push_back("--" + mBoundary);
            }
            
            headers.push_back("Content-type: text/html; charset=ISO-8859-1");
            headers.push_back("Content-transfer-encoding: 7BIT");
            
            return headers;
        }
        
        std::string Message::HTML::getData() const {
            std::stringstream data;
            
            data << mContent;
            
            if(isMultiPart()){
                data << MAIL_SMTP_NEWLINE;
                //the attachemnets
                for(auto& attachment: mAttachments){
                    data << MAIL_SMTP_NEWLINE << "--" << mBoundary << MAIL_SMTP_NEWLINE;
                    
                    Headers headers = attachment->getHeaders();
                    for(auto& header: headers){
                        data << header << MAIL_SMTP_NEWLINE;
                    }
                    
                    data << MAIL_SMTP_NEWLINE << attachment->getData() << MAIL_SMTP_NEWLINE;
                }
                
                data << MAIL_SMTP_NEWLINE << "--" << mBoundary << "--" << MAIL_SMTP_NEWLINE;
            }
            
            return data.str();
        }
        
        
        Message::Headers Message::Attachment::getHeaders() const {
            Message::Headers headers;
            
            ci::fs::path path = mDataSource->getFilePath();
            std::string filename = path.filename().string();
            std::string extension = path.extension().string();
            boost::algorithm::to_lower(extension);
            
            
            if(extension==".gif"){
                headers.push_back("Content-Type: image/gif;");
            }else if(extension==".jpg" || extension==".jpeg"){
                headers.push_back("Content-Type: image/jpg;");
            }else if(extension==".txt"){
                headers.push_back("Content-Type: plain/txt;");
            }else if(extension==".bmp"){
                headers.push_back("Content-Type: image/bmp;");
            }else if(extension==".html" || extension==".htm"){
                headers.push_back("Content-Type: plain/htm;");
            }else if(extension==".png"){
                headers.push_back("Content-Type: image/png;");
            }else if(extension==".exe"){
                headers.push_back("Content-Type: application/X-exectype-1;");
            }else{
                headers.push_back("Content-Type: application/X-other-1;");
            }
            
            headers.push_back("\tname=\"" + filename + "\"");
            headers.push_back("Content-Transfer-Encoding: base64");
            
            if(mEmbedded){
                headers.push_back("Content-Id: <" + getCID() + ">"); //cid is currently the filename, this is forward thinking!
            }else{
                headers.push_back("Content-Disposition: attachment; filename=\"" + filename + "\"");
            }
            
            return headers;
        }
        
        std::string Message::Attachment::getData() const{
            return ci::toBase64(mDataSource->getBuffer(), MAIL_SMTP_BASE64_LINE_WIDTH);
        }
        
        
    }
}