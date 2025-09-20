#include <crow.h>
#include <crow/compression.h>
#include <inja/inja.hpp>

int main() {
    // Startup App
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "Hello World from Crow on AWS!!";
    });

    CROW_ROUTE(app, "/health")([]() {
        return crow::response(200, "OK");
    });

    char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;

    app
    .use_compression(crow::compression::GZIP)
    .port(port)
    .multithreaded().run();
}