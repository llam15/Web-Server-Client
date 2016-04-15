# CS118 Project 1


## Makefile

By default (all target), it makes the `web-server` and `web-client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## web-server.cpp

Contains all of the code for the web server

## web-client.cpp

Contains the code for the web client

## http-message.h & http-message.cpp

Contains the implementation for the HTTP Messages class. The the HttpRequest
and HttpResponse classes inherit from the base class http-message.