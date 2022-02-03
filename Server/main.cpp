#include <LibCore/ArgsParser.h>
#include <LibCore/Stream.h>
#include <LibMain/Main.h>
#include <LibSourceEngine/BSP.h>
#include <Server/Server.h>

static Server* s_server;

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    String map_name;

    Core::ArgsParser args_parser;
    args_parser.add_positional_argument(map_name, "Name of the map to load", "map-name");
    args_parser.parse(arguments);

    auto bsp_file_stream =
        TRY(Core::Stream::File::open(String::formatted("{}.bsp", map_name), Core::Stream::OpenMode::Read));
    auto map = TRY(SourceEngine::BSP::try_parse(*bsp_file_stream));

    s_server = new Server(map_name, move(map));

    TRY(s_server->bind({}, 6666));

    return s_server->exec();
}