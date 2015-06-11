var crypto = require('crypto');
var milter = require('./build/Release/milter');

process.on('uncaughtException', function (e) { console.log(e); });


/**
 */
function connect (envelope, host, address)
{
  envelope.ts_start = (new Date).getTime();
  envelope.sid = crypto.randomBytes(2).toString('hex');

  console.log("[" + envelope.sid + "] connection from " + host + " (" + address + ")");
  return milter.SMFIS_CONTINUE;
}


/**
 */
function helo (envelope, identity)
{
  console.log("[" + envelope.sid + "] helo " + identity);
  return milter.SMFIS_CONTINUE;
}


//
function unknown ()  { return milter.SMFIS_CONTINUE; }
function mailfrom () { return milter.SMFIS_CONTINUE; }
function rcptto ()   { return milter.SMFIS_CONTINUE; }
function mstart ()   { return milter.SMFIS_CONTINUE; }
function header ()   { return milter.SMFIS_CONTINUE; }
function eoh ()      { return milter.SMFIS_CONTINUE; }
function segment ()  { return milter.SMFIS_CONTINUE; }
function mfinish ()  { return milter.SMFIS_CONTINUE; }
function abort ()    { return milter.SMFIS_CONTINUE; }


/**
 */
function close (envelope)
{
  var now = (new Date).getTime();
  console.log("[" + envelope.sid + "] connection closed");
  console.log("[" + envelope.sid + "] session lasted " + (now - envelope.ts_start) + " msec");
  return milter.SMFIS_CONTINUE;
}


/** main **********************************************************************/

var ok = milter.start(
  "inet:12345", // see postconf(5) "smtpd_milters" for details
  connect,  // connection event
  unknown,  // unknown client command
  helo,     // client "HELO" or "EHLO" i presume
  mailfrom, // client "MAIL FROM"
  rcptto,   // client "RCPT TO"
  mstart,   // client "DATA"
  header,   // parser event
  eoh,      // parser event
  segment,  // buffer of data
  mfinish,  // client "." (EOM)
  abort,    // ?
  close);   // connection event

console.log("milter.start:", ok);
