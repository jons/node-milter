require('v8-profiler');
var crypto = require('crypto');
var milter = require('./build/Debug/milter');

process.on('uncaughtException', function (e) { console.log(e); });


/**
 */
function negotiate (envelope, f0, f1, f2, f3)
{
  envelope.local.sid = crypto.randomBytes(2).toString('hex');

  console.log("[" + envelope.local.sid + "] filter session created");
  console.log("[" + envelope.local.sid + "] f0=" + f0);
  console.log("[" + envelope.local.sid + "] f1=" + f1);
  console.log("[" + envelope.local.sid + "] f2=" + f2);
  console.log("[" + envelope.local.sid + "] f3=" + f3);

  f2 = 0;
  f3 = 0;
  envelope.negotiate(f0, f1, f2, f3);
  envelope.done(milter.SMFIS_ALL_OPTS);
}

/**
 */
function connect (envelope, host, address)
{
  envelope.local.ts_start = (new Date).getTime();

  console.log("[" + envelope.local.sid + "] connection from " + host + " (" + address + ")");
  envelope.done(milter.SMFIS_CONTINUE);
}


/**
 */
function helo (envelope, identity)
{
  global.gc();
  console.log("[" + envelope.local.sid + "] helo " + identity);
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
  console.log("[" + envelope.local.sid + "] connection closed");
  console.log("[" + envelope.local.sid + "] session lasted " + (now - envelope.local.ts_start) + " msec");
  envelope.done(milter.SMFIS_CONTINUE);
}


/** main **********************************************************************/

var ok = milter.start(
  // setconn. see postconf(5) "smtpd_milters" for details
  "inet:12345",

  // register flags. see smfi_register for details
  milter.SMFIF_QUARANTINE | milter.SMFIF_ADDHDRS | milter.SMFIF_CHGFROM,

  negotiate, // milter-to-MTA negotiation
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
