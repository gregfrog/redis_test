"use strict";

var moment = require("moment");
const process = require('process');
const fs = require("fs");
var path = require('path');
const { randomInt } = require('crypto');

const Logger = require('./logger.js');
var wslog = new Logger("tmsd");
wslog.setRunningLocal();

var accumulatedSubmitTime = BigInt(0);
var submits = 0;
var promiseList = Array();

var thinkTime = 1000;
var concurrency = 400;
var serverPort = 7799
var stopping = false;

async function submitRequests(nParallel) {

    await (async () => {
        let start = moment();
        let promiseArray = Array();
        for (let i = 0; i < concurrency; i++) {
            promiseArray.push(sendMessages(`HELLO MOTORCYCLE ${i}`));
        }
        await Promise.all(promiseArray);
    })();


    async function sendMessages(payloadString)
    {
        let logonMessage = `LOGON ${payloadString}`;
        let logoffMessage = `LOGOFF ${payloadString}`;

        sendOneMessage(logonMessage);
        while(!stopping)
        {
            await new Promise(resolve => setTimeout(resolve, randomInt(2* thinkTime)));
            if(randomInt(20) < 2) 
            {
                await sendOneMessage(logoffMessage);
                await new Promise(resolve => setTimeout(resolve, randomInt(2* thinkTime)));
                await sendOneMessage(logonMessage);
            }
            else
            {
                await sendOneMessage(payloadString);
            }
        }
    }

    async function sendOneMessage(messageString) {
        var hrTimeStart = process.hrtime.bigint();

        var net = require('net');

        var client = new net.Socket();
        client.connect(serverPort, '127.0.0.1', function () {
            console.log('Connected');
            client.write(messageString);
        });

        client.on('data', function (data) {
            console.log('Received: ' + data);
            if(data == "UNKNOWN")
            {
                console.log(`Unknown at ${moment()} ${messageString}`);
            }
            client.end(); // kill client after server's response
        });

        client.on('close', function (hadError) {
            if (hadError) {
                console.log('Connection closed with error');
            }
            console.log('Connection closed');
        });

        var hrTimeEnd = process.hrtime.bigint();
        accumulatedSubmitTime = accumulatedSubmitTime + hrTimeEnd - hrTimeStart;
        submits++;
    }
}

submitRequests(concurrency);

