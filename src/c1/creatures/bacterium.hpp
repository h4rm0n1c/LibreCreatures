#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace creatures1::creatures {

enum class BacteriumActivityState : std::uint8_t {
    inactive = 0,
    dormant = 1,
    active = 2,
};

class Bacterium;
class Creature;

// The original CArchive record is eight consecutive bytes.  Stream framing,
// exceptions, and endian handling belong to the archive adapter.
class BacteriumArchive {
public:
    virtual ~BacteriumArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual void write_byte(std::uint8_t value) = 0;
};

class BacteriumRandomSource {
public:
    virtual ~BacteriumRandomSource() = default;

    virtual std::uint32_t next() = 0;
    virtual bool debug_logging_enabled() const = 0;
    virtual void log_replication(const Bacterium& bacterium) = 0;
};

// Supplies the concrete runtime services touched by CBacterium::Update.
// Registry storage, perception dispatch, and MapData's environmental array
// remain owned by the creature/world runtime rather than being reconstructed
// as globals or MFC containers in this policy layer.
class BacteriumUpdateHost {
public:
    virtual ~BacteriumUpdateHost() = default;
    virtual std::size_t creature_count() const = 0;
    virtual Creature* creature_at(std::size_t index) const = 0;
    virtual bool can_perceive(const Creature& owner,
                              const Creature& target) const = 0;
    virtual Bacterium& bacterium_of(Creature& creature) = 0;
    virtual Bacterium& environment_bacterium(std::size_t index) = 0;
    virtual BacteriumRandomSource& random_source() = 0;
};

class Bacterium {
public:
    // Native CBacterium::CBacterium uses the process CRT random source.  The
    // injected overload remains available for world-service tests and for
    // replication, but ordinary creature construction follows that native
    // no-argument ownership boundary.
    Bacterium();
    Bacterium(BacteriumRandomSource& random_source);

    void serialize(BacteriumArchive& archive);

    void update(Creature& owner, BacteriumUpdateHost& host);

    void replicate_and_mutate(Bacterium& offspring,
                              BacteriumRandomSource& random_source) const;

    BacteriumActivityState activity_state() const {
        return activity_state_;
    }
    void set_activity_state(BacteriumActivityState value) {
        activity_state_ = value;
    }
    std::uint8_t kill_threshold() const { return kill_threshold_; }
    std::uint8_t activation_threshold() const {
        return activation_threshold_;
    }
    std::int8_t input_chemical_id() const { return input_chemical_id_; }
    const std::array<std::int8_t, 4>& output_chemical_ids() const {
        return output_chemical_ids_;
    }
    std::array<std::int8_t, 4>& output_chemical_ids() {
        return output_chemical_ids_;
    }

private:
    BacteriumActivityState activity_state_ = BacteriumActivityState::inactive;
    std::uint8_t kill_threshold_ = 0;
    std::uint8_t activation_threshold_ = 0;
    std::int8_t input_chemical_id_ = 0;
    std::array<std::int8_t, 4> output_chemical_ids_{};
};

} // namespace creatures1::creatures
