/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Joyent, Inc.
 */

/*
 * tcpkatest.js: test TCP keep-alive configuration.
 */

var mod_path = require('path');
var mod_util = require('util');
var mod_net = require('net');

var arg0;

function main()
{
	var remote, c, host, port, timeout, sock;

	arg0 = mod_path.basename(process.argv[1]);
	if (process.argv.length < 4) {
		warn('missing arguments');
		usage();
	}

	remote = process.argv[2];
	c = remote.lastIndexOf(':');
	if (c == -1) {
		warn('missing port');
		usage();
	}

	host = remote.substr(0, c);
	port = parseInt(remote.substr(c + 1), 10);
	if (isNaN(port) || port <= 0 || port >= 65536) {
		warn('bad port: "%s"', remote.substr(c + 1));
		usage();
	}

	timeout = parseInt(process.argv[3], 10);
	if (isNaN(timeout) || timeout < 0) {
		warn('bad timeout: "%s"', process.argv[3]);
		usage();
	}

	console.error('using timeout: %s', timeout);
	sock = mod_net.connect({
	    'host': host,
	    'port': port
	});
	sock.on('connect', function () {
		sock.setKeepAlive(true, timeout);
		console.error('%s: socket: connected',
		    new Date().toISOString());
	});
	sock.on('error', function (err) {
		console.error('%s: socket: error: %s',
		    new Date().toISOString(), err.message);
	});
	sock.on('data', function (chunk) {
		console.error('%s: socket: read %d bytes',
		    new Date().toISOString(), chunk.length);
	});
	sock.on('end', function () {
		console.error('%s: socket: graceful end of stream',
		    new Date().toISOString());
	});
}

function warn()
{
	var args, msg;

	args = Array.prototype.slice.call(arguments);
	msg = mod_util.format.apply(null, args);
	console.error('%s: %s', arg0, msg);
}

function usage()
{
	console.error('usage: %s HOST:PORT KEEPALIVE_IDLE', arg0);
	process.exit(2);
}

main();
