require('v8-profiler');
var crypto = require('crypto');
var milter = require('./build/Debug/milter');
var dgram = require('dgram');

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

function connect (envelope, host, address)
{
  envelope.local.ts_start = (new Date).getTime();

  console.log("[" + envelope.local.sid + "] connection from " + host + " (" + address + ")");
  envelope.done(milter.SMFIS_CONTINUE);
}

function helo (envelope, identity)
{
  console.log("[" + envelope.local.sid + "] helo " + identity);
  envelope.done(milter.SMFIS_CONTINUE);
}

function mailfrom (envelope, addr)
{
  console.log("[" + envelope.local.sid + "] from " + addr);
  envelope.done(milter.SMFIS_CONTINUE);
}

function rcptto (envelope, addr)
{
  console.log("[" + envelope.local.sid + "] rcpt " + addr);
  envelope.done(milter.SMFIS_CONTINUE);
}

function mfinish (envelope)
{
  console.log("[" + envelope.local.sid + "] EOM");
  var udp = dgram.createSocket('udp4');
  var str = Buffer.from(JSON.stringify(envelope));

  // workaround for issue #2
  // without setImmediate wrapping this udpclient.send call, no datagram
  // is ever sent, and the callback provided to send() is never called
  setImmediate(function (udpclient, datas) {
    console.log('tick');
    udpclient.send(
      datas,
      0,
      datas.length,
      25828,
      '127.0.0.1',
      function (e) {
        console.log('sent');
        udpclient.close();
        if (e)
          console.log(e);
      });

    envelope.done(milter.SMFIS_CONTINUE);

  }, udp, str);
}

//
function unknown (envelope)  { console.log("[" + envelope.local.sid + "] unknown"); envelope.done(milter.SMFIS_CONTINUE); }
function mstart (envelope)   { console.log("[" + envelope.local.sid + "] DATA");    envelope.done(milter.SMFIS_CONTINUE); }
function header (envelope)   { console.log("[" + envelope.local.sid + "] header");  envelope.done(milter.SMFIS_CONTINUE); }
function eoh (envelope)      { console.log("[" + envelope.local.sid + "] EOH");     envelope.done(milter.SMFIS_CONTINUE); }
function segment (envelope)  { console.log("[" + envelope.local.sid + "] segment"); envelope.done(milter.SMFIS_CONTINUE); }
function abort (envelope)    { console.log("[" + envelope.local.sid + "] abort");   envelope.done(milter.SMFIS_CONTINUE); }

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

console.log("test.js milter.start:", ok);
