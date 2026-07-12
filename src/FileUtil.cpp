#include "FileUtil.h"

#include <system_error>

namespace dlrl
{

bool ReplaceFileFromTemplate(const std::filesystem::path& source,
                             const std::filesystem::path& destination,
                             std::string& error)
{
    const std::filesystem::path temporary = destination.string() + ".new-game.tmp";
    const std::filesystem::path backup = destination.string() + ".new-game.backup";
    try
    {
        if (source.empty() || destination.empty() || !std::filesystem::is_regular_file(source))
        {
            error = "The clean hard disk image is unavailable";
            return false;
        }
        if (std::filesystem::exists(destination)
            && std::filesystem::equivalent(source, destination))
        {
            error = "The active hard disk image is also the clean template";
            return false;
        }

        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        std::filesystem::remove(backup, ignored);
        std::filesystem::copy_file(source, temporary,
                                   std::filesystem::copy_options::overwrite_existing);

        const bool hadDestination = std::filesystem::exists(destination);
        if (hadDestination) std::filesystem::rename(destination, backup);
        try
        {
            std::filesystem::rename(temporary, destination);
        }
        catch (...)
        {
            if (hadDestination && !std::filesystem::exists(destination))
                std::filesystem::rename(backup, destination);
            throw;
        }
        if (hadDestination) std::filesystem::remove(backup, ignored);
        return true;
    }
    catch (const std::exception& exception)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        if (std::filesystem::exists(backup) && !std::filesystem::exists(destination))
            std::filesystem::rename(backup, destination, ignored);
        error = exception.what();
        return false;
    }
}

} // namespace dlrl
