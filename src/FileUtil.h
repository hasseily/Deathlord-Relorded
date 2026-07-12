#pragma once

#include <filesystem>
#include <string>

namespace dlrl
{

bool ReplaceFileFromTemplate(const std::filesystem::path& source,
                             const std::filesystem::path& destination,
                             std::string& error);

} // namespace dlrl
