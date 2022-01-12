#include <LibSourceEngine/BitStream.h>

ErrorOr<void> operator>>(SourceEngine::ReadableBitStream& stream, String& value)
{
    Vector<char> characters;

    char c;

    TRY(stream >> c);

    while (c != '\0')
    {
        characters.append(c);
        TRY(stream >> c);
    }

    value = String(characters.data(), characters.size());

    return {};
}

ErrorOr<void> operator<<(SourceEngine::WritableBitStream& stream, StringView value)
{
    for (const char& c : value)
        TRY(stream << c);

    TRY(stream << '\0');

    return {};
}
