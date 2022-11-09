
class Logger{

    constructor(logType){
      this.config = {
        host: 'localhost',
        port: 9880,
        timeout: 3.0,
        reconnectInterval: 600000 // 10 minutes
      };
      this.typeTag = logType;
      this.defaultLogger = require('fluent-logger').createFluentSender(this.typeTag, this.config);
      this.isRunningLocal = false;  
    }
  
  setRunningLocal()
  {
    this.isRunningLocal = true;
  }
  
  log(){
    let message = "";
    if(this.isRunningLocal){
        console.log.apply(console, arguments);
    } else {
      if(arguments.length == 1) {
        if(arguments[0] instanceof String) {
          message = { message: arguments[0] };
          message.full_message = JSON.stringify(message);
          message.short_message = arguments[0]
        }
        else {
            if(typeof arguments[0] === "string") {
              message = { message: arguments[0] };
              message.full_message = JSON.stringify(message);
              message.short_message = arguments[0]
            }      
            else {
              // its a single object, not a string
              message = arguments[0];
              if(Object.isExtensible(message))
              {
                message.full_message = JSON.stringify(arguments[0]);
                message.short_message = message.full_message.substring(0,50);
              }
            }
          }
      }
      else{
        let wkString = Array.from(arguments).join(" ");
        message = { message: wkString};
        message.full_message = wkString;
        message.short_message = message.full_message.substring(0,50);
      }
  
      console.log(JSON.stringify(message));
      this.defaultLogger.emit("x", message);
    }
  }
  
  end(){
    this.defaultLogger.end();
  }
  
    
  }
  
  module.exports = Logger;
  
  
  