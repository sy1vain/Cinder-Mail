Cinder-Mail
===========

SMTP from Cinder. This version is very sloppy and needs a lot of clean-up checks, etc etc. But it works for now for my project. When I find the time or need more functionality I will update the code.

SSL is not supported since the corresponding boost libraries are not built in the current (dev) distribution.

**Using the code**

	//create a message
	message = ci::mail::Message::create();
    message->setSender("[me@mysite.com]");
    message->addRecipient("you@yoursite.com");
    message->setSubject("Mail from Cinder!");
    message->setMessage("Hallo from cinder");
    message->setHTML("<html><body><center><h1>Mail from Cinder</h1><img src=\"cid:cinder_logo.png\"><br/>Happy coding!</body></html>");
    //add an inline attachment, in this case from the assets
    message->addInlineAttachment(loadAsset("cinder_logo.png"));
	
	//create a mailer object
    mailer = cinder::mail::Mailer::create("[my local ISP SMTP]");
    //send the message (on separate thread)
    mailer->sendMessage(message);

This will result in something like this:

![ScreenShot](https://raw.github.com/sy1vain/Cinder-Mail/master/screenshots/email.png)

Any `cid:filename` in the HTML will be replaced by the cid of a matching file.

**TODO (at the very least):**

* auto create plain text alternative from HTML
* management of recipients and attachments
* load html from file
* notifications of succes/failure
* SSL support -> currently Cinder does not contain the right boost build for this