#pragma once

#include "../application/resource_hosts.hpp"
#include "../creatures/genome.hpp"
#include "../creatures/voice.hpp"

namespace creatures1::platform {

// C1's file-backed creature resources are application-owned.  The clean
// genome/voice classes receive only these semantic stores and never construct
// paths or depend on CFile/Win32 handles.
class C1CreatureResourceHost final
    : public creatures::GenomeFileStore,
      public creatures::VoiceFileStore {
public:
    C1CreatureResourceHost(application::ResourceFileBackend& files,
                           std::string secondary_genetics_directory,
                           std::string primary_genetics_directory,
                           std::string primary_main_directory);

    std::string secondary_genetics_path(
        creatures::GenomeFilenameId id) const override;
    std::string primary_genetics_path(
        creatures::GenomeFilenameId id) const override;
    bool regular_file_exists(const std::string& path) const override;
    std::vector<std::uint8_t> read_file(const std::string& path) override;
    void write_secondary_genetics_file(
        creatures::GenomeFilenameId id,
        const std::vector<std::uint8_t>& payload) override;

    std::vector<std::uint8_t> read_voice_file(
        std::string_view filename) override;

private:
    std::string genome_filename(creatures::GenomeFilenameId id) const;

    application::ResourceFileBackend& files_;
    std::string secondary_genetics_directory_;
    std::string primary_genetics_directory_;
    std::string primary_main_directory_;
};

} // namespace creatures1::platform
