#include <AK/LexicalPath.h>
#include <AK/NumberFormat.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibCore/Stream.h>
#include <LibMain/Main.h>
#include <LibSourceEngine/VPK.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    StringView vpk_name;
    String extract_file_path;
    bool list_files{};
    bool extract_all_files{};

    Core::ArgsParser args_parser;
    args_parser.add_positional_argument(vpk_name, "Name of the VPK", "vpk-name");
    args_parser.add_option(extract_file_path, "Path of a file inside the VPK to extract", "extract", 'e',
                           "extract-file");
    args_parser.add_option(list_files, "List all files inside the VPK", "list", 'l');
    args_parser.add_option(extract_all_files, "Extract all files inside the VPK", "extract-all", 'E');
    args_parser.parse(arguments);

    auto vpk = TRY(SourceEngine::VPK::try_parse_from_file_path(vpk_name));

    if (list_files)
    {
        for (auto& entry : vpk.entries())
            outln("{} ({})", entry.key, human_readable_size(entry.value.entry_length()));

        return 0;
    }

    if (!extract_file_path.is_empty())
    {
        auto entry = TRY(vpk.entry(extract_file_path));
        auto entry_data = TRY(entry->read_data_from_archive(true));

        LexicalPath lexical_path_inside_vpk(extract_file_path);

        auto extract_stream =
            TRY(Core::Stream::File::open(lexical_path_inside_vpk.basename(), Core::Stream::OpenMode::Write));
        TRY(extract_stream->write(entry_data));

        return 0;
    }
    else if (extract_all_files)
    {
        auto current_working_directory = Core::File::current_working_directory();

        size_t number_of_entries_written = 0;
        size_t number_of_bytes_written = 0;
        for (auto& entry : vpk.entries())
        {
            auto entry_data = TRY(entry.value.read_data_from_archive(true));

            // FIXME: This should use realpath, somehow
            auto absolute_path_for_entry = String::formatted("{}/{}", current_working_directory, entry.key);

            if (!Core::File::ensure_parent_directories(absolute_path_for_entry))
                return Error::from_string_literal("Unable to ensure parent directories");

            auto entry_stream = TRY(Core::Stream::File::open(entry.key, Core::Stream::OpenMode::Write));
            TRY(entry_stream->write(entry_data));

            number_of_entries_written++;
            number_of_bytes_written += entry_data.size();

            out("\r{} entries written", number_of_entries_written);
        }
        outln();
        outln("{} written", human_readable_size(number_of_bytes_written));

        return 0;
    }

    args_parser.print_usage(stderr, arguments.argv[0]);

    return 1;
}