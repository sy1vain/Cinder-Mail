Cinder-Mail
===========

SMTP from Cinder. This version is very sloppy and needs a lot of clean-up checks, etc etc. But it works for now for my project. When I find the time or need more functionality I will update the code.

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
<<<<<<< HEAD

=======
>>>>>>> 6da7f38c8ba443994b72e558ccc54ff3e054bbe0
![ScreenShot](https://raw.github.com/sy1vain/Cinder-Mail/master/screenshots/email.png)

**TODO (at the very least):**

* Authentication
* Auto replacement of embedded images (with better cid's)
* auto create plain text alternative from HTML
* management of recipients and attachments
* load html from file
* format text to conform to the 1000 characters per line and correct newlines / encoding (7bit)
* fix bug with single . on line
* notifications of succes/failure
