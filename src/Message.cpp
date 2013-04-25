//
//  Message.cpp
//  FlatFunnyClient
//
//  Created by Sylvain Vriens on 25/04/2013.
//
//

#include "Message.h"

using namespace cinder::mail;

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
        data << "MIME-Version: 1.0" << MAIL_SMTP_NEWLINE << "Content-Type: multipart/mixed;" << MAIL_SMTP_NEWLINE << "\tboundary=\"" << MAIL_MSG_BOUNDARY << "\"" << MAIL_SMTP_NEWLINE;
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
        data << "--" << MAIL_MSG_BOUNDARY << MAIL_SMTP_NEWLINE;
        
        
        //the content part
        Headers headers = mContent->getHeaders();
        for(auto& header: headers){
            data << header << MAIL_SMTP_NEWLINE;
        }
        data << mContent->getData() << MAIL_SMTP_NEWLINE;
        
        //the attachemnets
        for(auto& attachment: mAttachments){
            data << MAIL_SMTP_NEWLINE << "--" << MAIL_MSG_BOUNDARY << MAIL_SMTP_NEWLINE;
            
            Headers headers = attachment->getHeaders();
            for(auto& header: headers){
                data << header << MAIL_SMTP_NEWLINE;
            }
            
            data << MAIL_SMTP_NEWLINE << attachment->getData() << MAIL_SMTP_NEWLINE;
        }
        
        data << MAIL_SMTP_NEWLINE << "--" << MAIL_MSG_BOUNDARY << "--" << MAIL_SMTP_NEWLINE;
        
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
    headers.push_back("\tboundary=\"" + ci::toString(MAIL_CONTENT_BOUNDARY) + "\"");
    
    return headers;
}

std::string Message::Content::getData() const{
    
    if(!isMultiPart()){
        return mText->getData();
    }
    
    std::stringstream data;
    
    data << MAIL_SMTP_NEWLINE << "--" << MAIL_CONTENT_BOUNDARY << MAIL_SMTP_NEWLINE;
    
    Headers headers = mText->getHeaders();
    for(auto& header: headers){
        data << header << MAIL_SMTP_NEWLINE;
    }
    data << MAIL_SMTP_NEWLINE <<mText->getData() << MAIL_SMTP_NEWLINE;
    
    data << MAIL_SMTP_NEWLINE<< "--" << MAIL_CONTENT_BOUNDARY << MAIL_SMTP_NEWLINE;
    
    headers = mHTML->getHeaders();
    for(auto& header: headers){
        data << header << MAIL_SMTP_NEWLINE;
    }
    data << MAIL_SMTP_NEWLINE <<mHTML->getData() << MAIL_SMTP_NEWLINE;
    
    data << MAIL_SMTP_NEWLINE << "--" << MAIL_CONTENT_BOUNDARY << "--" << MAIL_SMTP_NEWLINE;
    
    return data.str();
}


Message::Headers Message::Text::getHeaders() const {
    Message::Headers headers;
    
    headers.push_back("Content-type: text/plain; charset=ISO-8859-1");
    headers.push_back("Content-transfer-encoding: 7BIT");
    
    return headers;
}

std::string Message::Text::getData() const{
    return formatRFC(mContent);
}

std::string Message::Text::formatRFC(const std::string &data) const{
    
    //replace any wrong line-endings
    std::string replace = std::regex_replace(data, std::regex("\n\r|\r[^\n]|[^\r]\n"), MAIL_SMTP_NEWLINE);
    
    std::vector<std::string> lines = ci::split(data, MAIL_SMTP_NEWLINE);
    
    std::stringstream ss;
    for(std::vector<std::string>::iterator itr=lines.begin(); itr<lines.end(); ++itr){
        //if not the first push the line charactre first
        if(itr!=lines.begin()) ss << MAIL_SMTP_NEWLINE;
        
        std::string line = *itr;
        
        //if only a point, add it with a space
        if(line=="."){
            ss<<".."; //will show up as a single point
            continue;
        }
        
        size_t found;
        while(line.size()>100 && (found=line.rfind(" ",100))!=std::string::npos){
            ss << line.substr(0,found) << MAIL_SMTP_NEWLINE;
            line = line.substr(found+1);
        }
        ss << line;
        
        
    }
    
    
    return ss.str();
}

Message::Headers Message::HTML::getHeaders() const {
    Message::Headers headers;
    
    if(isMultiPart()){
        headers.push_back("Content-type: multipart/related;");
        headers.push_back("\tboundary=\"" + ci::toString(MAIL_HTML_BOUNDARY) + "\"");
        headers.push_back("");
        headers.push_back("--" + ci::toString(MAIL_HTML_BOUNDARY));
    }
    
    headers.push_back("Content-type: text/html; charset=ISO-8859-1");
    headers.push_back("Content-transfer-encoding: 7BIT");
    
    return headers;
}

std::string Message::HTML::getData() const {
    std::stringstream data;
    
    //find and replace cid and make it max 100 chars per line
    data << formatRFC(findReplaceCID(mContent));
    
    
    if(isMultiPart()){
        data << MAIL_SMTP_NEWLINE;
        //the attachemnets
        for(auto& attachment: mAttachments){
            data << MAIL_SMTP_NEWLINE << "--" << MAIL_HTML_BOUNDARY << MAIL_SMTP_NEWLINE;
            
            Headers headers = attachment->getHeaders();
            for(auto& header: headers){
                data << header << MAIL_SMTP_NEWLINE;
            }
            
            data << MAIL_SMTP_NEWLINE << attachment->getData() << MAIL_SMTP_NEWLINE;
        }
        
        data << MAIL_SMTP_NEWLINE << "--" << MAIL_HTML_BOUNDARY << "--" << MAIL_SMTP_NEWLINE;
    }
    
    return data.str();
}

std::string Message::HTML::findReplaceCID(const std::string& data) const{
    
    if(mAttachments.empty()) return data;
    
    std::string ret = data;
    
    for(auto& attachment: mAttachments){
        std::regex regex("cid:" + attachment->getFileName());
        std::string replace("cid:" + attachment->getCID());
        ret = std::regex_replace(data, regex, replace);
    }
    
    
    return ret;
}

std::string Message::HTML::getText(){
    std::string text = mContent;
    
    //change p and br into new lines
    text = std::regex_replace(text, std::regex("<p>|<p/>|<p .*?>"), "\n\n");
    text = std::regex_replace(text, std::regex("<br>|<br/>"), "\n");
    
    //strip html
    text = std::regex_replace(text, std::regex("<.*?>"), "");
    
    return text;
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
