#include <iostream>

#include "server/webserver.h"
#include "config/config.h"

int main() {
    Config config;
    WebServer server(config);
    server.Startup();
}