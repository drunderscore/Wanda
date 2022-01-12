#include <LibMain/Main.h>
#include <Server/Server.h>

static Server* s_server;

ErrorOr<int> serenity_main(Main::Arguments)
{
    s_server = new Server;

    TRY(s_server->bind({}, 6666));

    return s_server->exec();
}