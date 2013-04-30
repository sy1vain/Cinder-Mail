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
#include "cinder/app/App.h"
#include "cinder/DataSource.h"

#include "Mail.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <regex>

namespace cinder {
    namespace mail {
        
        typedef std::shared_ptr<class Message> MessageRef;
        
        class Message {
        public:
            enum recipient_type {
                TO,CC,BCC
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
            typedef std::shared_ptr<Attachment> AttachmentRef;
            
            //creator function
            static MessageRef create(){
                return MessageRef(new Message());
            }
            
            AttachmentRef addAttachment(const ci::DataSourceRef& dataSource, bool embed=false){
                return addAttachment(Attachment::create(dataSource, embed));
            }
            
            AttachmentRef addAttachment(const ci::fs::path &path, bool embed=false){
                return addAttachment(Attachment::create(path, embed));
            }
            
            AttachmentRef addAttachment(const std::string& path, bool embed=false){
                return addAttachment(Attachment::create(path, embed));
            }
            
            AttachmentRef addInlineAttachment(const ci::DataSourceRef& dataSource){
                return addAttachment(Attachment::create(dataSource, true), true);
            }
            
            AttachmentRef addInlineAttachment(const ci::fs::path &path){
                return addAttachment(Attachment::create(path, true), true);
            }
            
            AttachmentRef addInlineAttachment(const std::string& path){
                return addAttachment(Attachment::create(path, true), true);
            }
            
            AttachmentRef addAttachment(const AttachmentRef& attachment, bool embed=false){
                if(embed){
                    mContent->addAttachment(attachment);
                }else{
                    mAttachments.push_back(attachment);
                }
                return attachment;
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
                }else if(type==BCC){
                    mBCC.push_back(a);
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
            
            void loadMessage(const ci::fs::path& path){
                try{
                    setMessage(
                               ci::loadString(
                                              ci::loadFile(path)
                               )
                    );
                }catch(...){
                    ci::app::console() << "Failed to load message content" << std::endl;
                }
            }
            
            void setHTML(const std::string& html){
                mContent->setHTML(html);
            }
            
            void loadHTML(const ci::fs::path& path){
                try {
                    setHTML(
                            ci::loadString(
                                           ci::loadFile(path)
                            )
                            );
                }catch(...){
                    ci::app::console() << "Failed to load HTML message content" << std::endl;
                }
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
            typedef std::shared_ptr<Content> ContentRef;
            
            
            ContentRef                  mContent;
            std::vector<AttachmentRef>  mAttachments;
            
            Address                     mFrom;
            Address                     mReplyTo;
            std::vector<Address>        mTo;
            std::vector<Address>        mCC;
            std::vector<Address>        mBCC;
            std::string                 mSubject;
            
            //****************//
            // HELPER CLASSES //
            //****************//
        protected:
            class Text;
            class HTML;
            typedef std::shared_ptr<Text> TextRef;
            typedef std::shared_ptr<HTML> HTMLRef;
            
            class MailPart {
                virtual std::string getData() const{return "";}
            };
            
            class Text : public MailPart {
            public:
                
                static TextRef create(const std::string& content=""){
                    return TextRef(new Text(content));
                }
                
                virtual Headers getHeaders() const;
                virtual std::string getData() const;
                
                void setContent(const std::string& content){
                    mContent = content;
                }
                
                std::string getText(){
                    return mContent;
                }
            protected:
                Text(const std::string& content=""){
                    mContent = content;
                }
                
                //format for max 100 chars per line and not single '.' on a line
                std::string formatRFC(const std::string& data) const;
                
                std::string mContent;
            };
            
            class HTML : public Text {
            public:
                
                static HTMLRef create(const std::string& content=""){
                    return HTMLRef(new HTML(content));
                }
                
                void addAttachment(const AttachmentRef& attachment){
                    mAttachments.push_back(attachment);
                }
                
                bool isMultiPart() const{
                    return !mAttachments.empty();
                }
                
                //will strip HTML and return text
                std::string getText();
                
                virtual Headers getHeaders() const;
                std::string getData() const;
                
            protected:
                HTML(const std::string& content=""){
                    mContent = content;
                }
                
                std::string findReplaceCID(const std::string& data) const;
                
                std::vector<AttachmentRef> mAttachments;
                
            };
            
            class Content : public MailPart {
            public:
                static ContentRef create(){
                    return ContentRef(new Content());
                }
                
                bool isMultiPart() const{
                    if(mHTML){
                        return true;
                    }
                    return false;
                }
                
                Headers getHeaders() const;
                std::string getData() const;
                
                void addAttachment(const AttachmentRef& attachment){
                    if(!mHTML){
                        setHTML("");
                    }
                    mHTML->addAttachment(attachment);
                }
                
                void setMessage(const std::string& msg){
                    if(!mText) mText = Text::create("");
                    mText->setContent(msg);
                }
                
                void setHTML(const std::string& html){
                    if(!mHTML){
                        mHTML = HTML::create();
                    }
                    mHTML->setContent(html);
                    
                    //if no text or the content length is 0
                    if(!mText || mText->getText().size()==0){
                        mText = Text::create(mHTML->getText());
                    }
                }
            protected:
                Content(){
                }
                
                TextRef mText;
                HTMLRef mHTML;

            };
        public:
            class Attachment : public MailPart {
            public:
                
                static AttachmentRef create(const ci::DataSourceRef& datasource, bool embed=false){
                    return AttachmentRef(new Attachment(datasource, embed));
                }
                static AttachmentRef create(const ci::fs::path &path, bool embed=false){
                    return create(ci::DataSourcePath::create(path), embed);
                }
                static AttachmentRef create(const std::string &path, bool embed=false){
                    return create(ci::fs::path(path), embed);
                }
                
                std::string getCID() const{
                    std::string path(mDataSource->getFilePath().string());

                    size_t hash = std::hash<std::string>()(path);
                    std::string cid = ci::toBase64(ci::toString(hash));
                    
                    return cid;
                }
                
                std::string getFileName() const{
                    return mDataSource->getFilePath().filename().string();
                }
                
                Headers getHeaders() const;
                std::string getData() const;
                
            protected:
                Attachment(const ci::DataSourceRef& datasource, bool embedded=false){
                    mDataSource = datasource;
                    mEmbedded = embedded;
                }
                
                ci::DataSourceRef mDataSource;
                bool mEmbedded;
            };
            
        protected:
            
            
        };
        
        
        typedef Message::AttachmentRef AttachmentRef;
        

    }
}