require('v8-profiler');
var crypto = require('crypto');
var milter = require('./build/Debug/milter');

process.on('uncaughtException', function (e) { console.log(e); });


/**
 */
function connect (envelope, host, address)
{
  envelope.ts_start = (new Date).getTime();
  envelope.sid = crypto.randomBytes(2).toString('hex');

  console.log("[" + envelope.sid + "-" + envelope.debugenv + "] connection from " + host + " (" + address + ")");
  envelope.done(milter.SMFIS_CONTINUE);
}


/**
 */
function helo (envelope, identity)
{
  global.gc();
  console.log("[" + envelope.sid + "-" + envelope.debugenv + "] helo " + identity);
  envelope.done(milter.SMFIS_CONTINUE);
}


//
function unknown (envelope)  { envelope.done(milter.SMFIS_CONTINUE); }
function mailfrom (envelope) { envelope.done(milter.SMFIS_CONTINUE); }
function rcptto (envelope)   { envelope.done(milter.SMFIS_CONTINUE); }
function mstart (envelope)   { envelope.done(milter.SMFIS_CONTINUE); }
function header (envelope)   { envelope.done(milter.SMFIS_CONTINUE); }
function eoh (envelope)      { envelope.done(milter.SMFIS_CONTINUE); }
function segment (envelope)  { envelope.done(milter.SMFIS_CONTINUE); }
function mfinish (envelope)  { envelope.done(milter.SMFIS_CONTINUE); }
function abort (envelope)    { envelope.done(milter.SMFIS_CONTINUE); }


/**
 */
function close (envelope)
{
  var now = (new Date).getTime();
  console.log("[" + envelope.sid + "-" + envelope.debugenv + "] connection closed");
  console.log("[" + envelope.sid + "-" + envelope.debugenv + "] session lasted " + (now - envelope.ts_start) + " msec");
  envelope.done(milter.SMFIS_CONTINUE);
}


/** main **********************************************************************/

var ok = milter.start(
  // setconn. see postconf(5) "smtpd_milters" for details
  "inet:12345",

  // register flags. see smfi_register for details
  milter.SMFIF_QUARANTINE | milter.SMFIF_ADDHDRS | milter.SMFIF_CHGFROM,

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
